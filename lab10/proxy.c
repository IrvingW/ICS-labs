/*
 * proxy.c - CS:APP Web proxy
 *
 * TEAM MEMBERS:
 *     Andrew Carnegie, ac00@cs.cmu.edu 
 *     Harry Q. Bovik, bovik@cs.cmu.edu
 *
 *
 * Student Information:
 *		Name: Wang Tao
 *		ID: 515030910083
 *		email: thor.wang@sjtu.edu.cn
 *
 *
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 *
 *   Descripton:
 * 1.create a new thread for each request
 * 2.receive request from client and parse it
 * 3.resend request to server
 * 4.receive response from server
 * 5.decide method to redirect response body accroding to response header
 * 6.write log imformation into log file
 */ 

#include "csapp.h"

/*
 * Function prototypes
 */
// gived
int parse_uri(char *uri, char *target_addr, char *path, int  *port);
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);

/*     Funcions used in my implement
 */
 
/* the most important part of this lab, 
   which response receive the request sent by client side,
   then parse and resend the request to the server side.
   finally, the filter will receive the response from server
   and redirect them to client side.*/
void filter(int, struct sockaddr_in);

// the function execute when create a new thread
void * thread(void *vargp);

// open a client file descripter thread safely
int open_clientfd_ts(char *hostname, int port);

// Rio I/O for web version
ssize_t Rio_readlineb_w(rio_t * rp, void *usrbuf, size_t maxlen);
ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n);
int Rio_writen_w(int fd, void *usrbuf, size_t n);


/* Global */
sem_t mutex_openfd;
sem_t mutex_log;
int logfd; // the file descriptor of log file



/* 
 * main - Main routine for the proxy program 
 *
 * 1.create socket's as a server and client
 * 2.listen to request from client
 * 3.create a new thread for each request
 */
int main(int argc, char **argv)
{

    /* Check arguments */
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	exit(0);
    }
	// my implement
	// as a server part
	
	// ignore the SIGPIPE signal, which will happen when write to 
	// a connection that has been closed by the peer 
	signal(SIGPIPE, SIG_IGN);
	
	Sem_init(&mutex_openfd, 0, 1);
	Sem_init(&mutex_log, 0, 1);
	
	int listenfd, port;
	void *args;
	int *connfd_p;
	socklen_t clientlen = sizeof(struct sockaddr_in);
	struct sockaddr_in *clientaddr_p;
	pthread_t tid;
	
	port = atoi(argv[1]);
	listenfd = Open_listenfd(port);
	
	// open log file
	logfd = Open("proxy.log", O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR);

	while(1){
		args = malloc(sizeof(int) + sizeof(struct sockaddr_in));
		connfd_p = args;
		clientaddr_p = args + sizeof(int);
		*connfd_p = Accept(listenfd, (SA *)clientaddr_p, &clientlen);

		pthread_create(&tid, NULL, thread, (void *)args);
	}
	
	Close(logfd);
	Close(listenfd);
    exit(0);
}

/* The filter function

 *   each thread will use this function to:
 * 1.receive request from client and parse it
 * 2.resend request to server
 * 3.receive response from server
 * 4.decide method to redirect response body accroding to response header
 * 5.write log imformation into log file
 */
void filter(int connfd, struct sockaddr_in clientaddr){
	
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	
	rio_t rio_client, rio_server;
	Rio_readinitb(&rio_client, connfd);	
	Rio_readlineb_w(&rio_client, buf, MAXLINE);
	sscanf(buf, "%s %s %s", method, uri, version);

	// as a client part
	char hostname[MAXLINE], pathname[MAXLINE];
	int port;
	int serverfd;
	int n;
	// connect to the server
	parse_uri(uri, hostname, pathname, &port);
	serverfd = open_clientfd_ts(hostname, port);

	// forward my request line
	sprintf(buf, "%s /%s %s\r\n", method, pathname, version);
	Rio_writen_w(serverfd, buf, strlen(buf));
	
	// forward request header
	while((n = Rio_readlineb(&rio_client, buf, MAXLINE)) != 0){
		Rio_writen_w(serverfd, buf, strlen(buf));
		if(strcmp(buf, "\r\n") == 0){
			break;
		}
	}
	
	// deal with the response sent by server
	
	
	Rio_readinitb(&rio_server, serverfd);
	// receive and redirect response line
	n = Rio_readlineb_w(&rio_server, buf, MAXLINE);
	Rio_writen_w(connfd, buf, n);
	// not ok, and in this case, do not hava content
	if(strcmp(buf, "HTTP/1.1 200 OK\r\n")){
		puts("response not ok\n");

		while((n = Rio_readlineb_w(&rio_server, buf, MAXLINE) != 0)){
			Rio_writen_w(connfd, buf, sizeof(buf));

		}
	}else{ // HTTP 200 ok
		int chunked = 0; // Transfer Encoding: chunked
		int sum_size = 0;
		// deal with response header
		while(strcmp(buf, "\r\n")){
			n = Rio_readlineb(&rio_server, buf, MAXLINE);
			if(strstr(buf, "Content-Length"))
				sum_size = atoi(buf + 16);
			if(strstr(buf, "Transfer-Encoding: chunked"))
				chunked = 1;

			Rio_writen_w(connfd, buf, n);
		}

		// deal with response body
		if(chunked){
			int chunk_size = 0;
			sum_size = 0;
			// each chunk
			while(1){
				n = Rio_readlineb_w(&rio_server, buf, MAXLINE);
				Rio_writen_w(connfd, buf, n);
				
				//get the chunk size
				chunk_size = strtol(buf, NULL, 16);
				if(chunk_size == 0) // end of chunks
					break;
				
				sum_size += chunk_size;
				// redirect chunk in while 
				while(chunk_size > 0){
					// important , chunk_size do not contaion \n\r
					n = (chunk_size < (MAXLINE - 1)) ? chunk_size : (MAXLINE - 1);
					if(Rio_readnb_w(&rio_server, buf, n) > 0){
						// make a try later //////
						Rio_writen_w(connfd, buf, n);
					}
					chunk_size -= n;
				}
				// \r\n at the end
				n = Rio_readlineb_w(&rio_server, buf, MAXLINE);
				Rio_writen_w(connfd, buf, n);
			}
		}else{  // not chunked
			int content_len = sum_size;
			while(content_len > 0){
				n = (content_len < (MAXLINE - 1)) ? content_len : (MAXLINE - 1);
				if(Rio_readnb_w(&rio_server, buf, n) > 0){
					Rio_writen_w(connfd, buf, n);
				}
				content_len -= n;
			}
		}
		printf("write into log file...\n");
		// write into log file
		char logString[MAXLINE];
		format_log_entry(logString, &clientaddr, uri, sum_size);
		// use P and V 
		P(&mutex_log);
		Rio_writen_w(logfd, logString, strlen(logString));
		V(&mutex_log);
		//printf("write log done!\n");
	}
	Close(serverfd);
	printf("done!\n");
	return;
}


ssize_t Rio_readlineb_w(rio_t * rp, void *usrbuf, size_t maxlen){
	ssize_t rc;
	if((rc = rio_readlineb(rp, usrbuf, maxlen)) < 0)
		puts("Rio_readlineb error");

	return rc;
}

ssize_t Rio_readnb_w(rio_t *rp, void *usrbuf, size_t n){
	ssize_t rc;
	if((rc = rio_readnb(rp, usrbuf, n))<0)
		puts("Rio_readnb error");
	return rc;
}

int Rio_writen_w(int fd, void *usrbuf, size_t n){
	if(rio_writen(fd, usrbuf, n) != n){
		puts("Rio_writen error");
		return -1;
	}
	return 1;
}

void * thread(void *vargp){
	int connfd = *((int *)vargp);
	struct sockaddr_in clientaddr = *((struct sockaddr_in*)(vargp + sizeof(int)));
	free(vargp);

	pthread_detach(pthread_self());
	filter(connfd, clientaddr);
	Close(connfd);
	return NULL;
}

int open_clientfd_ts(char *hostname, int port){
	int clientfd;
	struct hostent *hp;
	struct sockaddr_in serveraddr;
	
	// create socket descriptor
	if((clientfd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
		return -1;

	P(&mutex_openfd);
	if((hp = gethostbyname(hostname)) == NULL){
		V(&mutex_openfd);
		return -2;
	}

	bzero((char *) &serveraddr, sizeof(serveraddr));
	serveraddr.sin_family = AF_INET;
	bcopy((char *)hp->h_addr_list[0],
			(char *)&serveraddr.sin_addr.s_addr, hp->h_length);
	serveraddr.sin_port = htons(port);

	if(connect(clientfd, (SA *) &serveraddr, sizeof(serveraddr)) < 0){
		V(&mutex_openfd);
		return -1;
	}
	
	V(&mutex_openfd);
	return clientfd;
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
    sprintf(logstring, "%s: %d.%d.%d.%d %s", time_str, a, b, c, d, uri);
}


