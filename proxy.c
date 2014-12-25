/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
void *manage_connection(void *vargp);

/*
 * Open_clientfd_ts
 */
sem_t semaphore_clientfd;
int Open_clientfd_ts(char *hostname, int port) 
{
    int clientfd;
    struct hostent *hp;
    struct sockaddr_in serveraddr;

    if ((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	return -1; /* check errno for cause of error */

    /* Fill in the server's IP address and port */
	P(&semaphore_clientfd); //since gethostbyname is not thread-safe, we have to perform semaphore here.
    if ((hp = gethostbyname(hostname)) == NULL)
	return -2; /* check h_errno for cause of error */
	V(&semaphore_clientfd);
    bzero((char *) &serveraddr, sizeof(serveraddr));
    serveraddr.sin_family = AF_INET;
    bcopy((char *)hp->h_addr_list[0], 
	  (char *)&serveraddr.sin_addr.s_addr, hp->h_length);
    serveraddr.sin_port = htons(port);

    /* Establish a connection with the server */
    if (connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0)
	return -1;
    return clientfd;
}
/*
 * connection_params - it is an simple structure to give thread multiple information at once.
 * it will give sockaddr_in, and accepted file descriptor!
 */
struct connection_params
{
	struct sockaddr_in sockinfo;
	int fd;
};

/*
 * print_log - Get log string, and print it out to the file proxy.log
 * If it is not thread-safe, some log can be ignored :(
 * So, we use semaphore! one thread takes one operation!
 */
sem_t semaphore_log;
void print_log(char* lg)
{
	P(&semaphore_log); //wait!
	FILE *prxy_log;
	prxy_log = fopen("proxy.log", "a"); //append to proxy.log file.
	//fprintf(prxy_log, "%s\n", lg); //print it out!
	fwrite(lg, 1, strlen(lg), prxy_log);
	fclose(prxy_log); //close. save what we've done :)
	V(&semaphore_log); //post!
}

/*
 * reset_log - initialize log file.
 * Make proxy.log empty file. (optional choose)
 */
void reset_log()
{
	FILE *prxy_log;
	prxy_log = fopen("proxy.log", "w");
	fprintf(prxy_log, "");
	fclose(prxy_log);
}

/*
 * Rio wrappers
 * I implemented this as the spec says.
 */
ssize_t Rio_readnb_w(rio_t* rp, void *ptr, size_t nbytes)
{
	ssize_t n;
	if((n = rio_readnb(rp, ptr, nbytes)) < 0) {
		//printf("Warning : Rio_readn failed.\n");
		return 0;
	}
	return n;
}
ssize_t Rio_readlineb_w(rio_t* rp, void* usrbuf, size_t maxlen)
{
	ssize_t rc;
	if((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
	{
		//printf("Warning : Rio_readlineb failed.\n");
		return 0;
	}
	return rc;
}
void Rio_writen_w(int fd, void *usrbuf, size_t n)
{
	if(rio_writen(fd, usrbuf, n) != n)
	{
		//printf("Warning : Rio_writen failed.\n");
	}
}
/*
 * manage_connection - handles requests
 * fd of accept is transfered to vargp. We use it!
 */
void *manage_connection(void *vargp)
{
	Pthread_detach(Pthread_self()); //detach the thread!
	int accept_fd;
	int out_fd;
	rio_t rio_in, rio_out;
	char trash[MAXLINE], uri[MAXLINE];
	char dest_host[MAXLINE], dest_path[MAXLINE];
	char buf[MAXLINE];
	char log[MAXLINE];
	int dest_port;
	unsigned int n;
	ssize_t res_size = 0;
	struct connection_params* p = (struct connection_params*)vargp;
	accept_fd = p->fd;
	struct sockaddr_in clientaddr = p->sockinfo;
	rio_readinitb(&rio_in, accept_fd); //link robust IO with the proxy client
	if((n = Rio_readlineb(&rio_in, buf, MAXLINE)) <= 0) //read is not available?
	{
		Close(accept_fd); //close fd
		return (void*)0; //and it ends.
	}
	sscanf(buf, "%s %s %s", trash, uri, trash); //We DON'T care about the method, and HTTP version.
	if(parse_uri(uri, dest_host, dest_path, &dest_port) < 0) //parse!
	{
		Close(accept_fd);
		return (void*)0;
	}
	out_fd = Open_clientfd_ts(dest_host, dest_port); //and make a connection to the destination.
	rio_readinitb(&rio_out, out_fd); //link robust IO to the destination server.
	rio_writen(out_fd, buf, strlen(buf)); //write what we've got before.
	while((n = Rio_readlineb_w(&rio_in, buf, MAXLINE)) > 0){ //Read until we meet \r\n, which is the end of the req.
		if(!strcmp(buf, "\r\n")) {
			Rio_writen_w(out_fd, "Connection: close\r\n\r\n", strlen("Connection: close\r\n\r\n"));
			//I didn't consider about keep-alive connection
			break;
		}
		Rio_writen_w(out_fd, buf, strlen(buf)); //Write to destination!
	} 
	//rio_writen(out_fd, "\r\n", 2); //End mark.
	while((n = Rio_readnb_w(&rio_out, buf, MAXLINE)) > 0) //And, there's a response from server.
	{
		res_size += n; //add up the size!
		Rio_writen_w(accept_fd, buf, n); //Write to the proxy client.
	}
	//printf("%s\n",buf);
	Close(accept_fd); //Interaction with the proxy client is also over!
	Close(out_fd); //Interaction with the destination server is over!
	format_log_entry(log, (struct sockaddr_in*)&clientaddr, uri, res_size); //Let's make log.
	print_log(log); //And, print it out.
	return (void*)0;
}
int main(int argc, char **argv)
{	
	Signal(SIGPIPE, SIG_IGN); //We ignore broken pipe.
	reset_log(); //clear up the log file.
	Sem_init(&semaphore_log, 0, 1); //semaphore initialization for logging.
	Sem_init(&semaphore_clientfd, 0, 1); //semaphore initialization for Open_listenfd_ts
    /* Check arguments */
    if (argc != 2) { //oops! no port number is given!
	fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	exit(0);
    }
	//port check
	int port = atoi(argv[1]);
	if(port<1024 || port>65536) { //Port should be ephemeral and available.
		fprintf(stderr, "Port number should be between 1024 and 65536.\n");
		exit(0);
	}
	//server setting
	int listen_fd = Open_listenfd(port); //We listen connection through the port.
	//struct sockaddr_in clientaddr;
	unsigned int clientlen;

	//int *fd_conn;
	struct connection_params* prm; 
	prm = (struct connection_params*)malloc(sizeof(struct connection_params)); //parameter construction
	//We make a box for parameters.
	//Since every thread must not share there resources, I allocated new memory and gave it to thread.
	pthread_t tid;
	while(1)
	{
		clientlen = sizeof(prm->sockinfo); //SA size.
		prm->fd = Accept(listen_fd, (SA*)&(prm->sockinfo), &clientlen); //accept! get socket info and save it to sockinfo.
		Pthread_create(&tid, NULL, manage_connection, (void*)prm); //Make a thread with parameters.
	}
	Close(listen_fd); //Ends listening.
    exit(0);
}

/*
 * parse_uri - URI parser
 * 
 * Given a URI from an HTTP proxy GET request (i.e., a URL), extract
 * the host name, path name, and port.  The memory for hostname and
 * pathname must already be allocated and should be at least MAXLINE
 * bytes. Return -1 if there are any problems.
 */
int parse_uri(char *uri, char *hostname, char *pathname, int *port)
{
    char *hostbegin;
    char *hostend;
    char *pathbegin;
    int len;

    if (strncasecmp(uri, "http://", 7) != 0) {
		hostname[0] = '\0';
		return -1;
    }
       
    /* Extract the host name */
    hostbegin = uri + 7;
    hostend = strpbrk(hostbegin, " :/\r\n\0");
    len = hostend - hostbegin;
    strncpy(hostname, hostbegin, len);
    hostname[len] = '\0';
    
    /* Extract the port number */
    *port = 80; /* default */
    if (*hostend == ':')   
		*port = atoi(hostend + 1);
    
    /* Extract the path */
    pathbegin = strchr(hostbegin, '/');
    if (pathbegin == NULL) {
		pathname[0] = '\0';
    }
    else {
		pathbegin++;	
		strcpy(pathname, pathbegin);
    }

    return 0;
}

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
	//Fix : added URI and size to logstring..
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d\n", time_str, a, b, c, d, uri, size);
}


