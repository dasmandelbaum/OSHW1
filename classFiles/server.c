#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404

/**
    References used:
        https://github.com/Pithikos/C-Thread-Pool/blob/master/thpool.c
        https://github.com/jonhoo/pthread_pool/blob/master/pthread_pool.c
*/



/* structs needed:
    1) Thread
        - pointer to pthread
        - STAT-8: thread ID
        - STAT-9: count of http requests handled
        - STAT-10: count of HTML requests handled
        - STAT-11: count of Image requests handled
    2) Threadpool
        - pointer to threads
        - mutex
        - cond
    3) Request
        - pointer to request
        - STAT-2: arrival time of request (relative to web server start time)
        - STAT-3: requests dispatched before this request count
        - STAT-4: time request was dispatched  
        - STAT-6: time of read completion (relative to web server start time)
        - STAT-7: number of requests given priority over this one
    4) FIFORequestQueue
        - pointer to requests 
        - mutex 
    5) SpecialRequestQueue
        - pointer to requests 
        - mutex
        - HTML or JPG priority
*/

struct  {  
    pthread_t pthread;
    int id;
    int countHttpRequests;
    int countHtmlRequests;
    int countImageRequests; 
} thread;

struct {
    thread **threads;
    pthread_cond_t cond;
} thread_pool;

struct {
   struct request * previous;
   int *requestInfo;
   int arrivalTime;
   int countDispatchedPreviously;
   int dispatchedTime;
   int readCompletionTime;
   int numRequestsHigherPriority;
} request;

struct {
    struct request** requests;
    pthread_mutex_t mutex;
} fifo_request_queue;

struct {
    request** requests;
    pthread_mutex_t mutex;
    int priority; //0 for html, 1 for jpg
} special_request_queue;

/*
    global variables needed:
        - STAT-1: count of total requests present
        - STAT-5: completed request count   
*/
static int requestsPresentCount;
static int requestCountTotal;

struct {
	char *ext;
	char *filetype;
} extensions [] = {
	{"gif", "image/gif" },  
	{"jpg", "image/jpg" }, 
	{"jpeg","image/jpeg"},
	{"png", "image/png" },  
	{"ico", "image/ico" },  
	{"zip", "image/zip" },  
	{"gz",  "image/gz"  },  
	{"tar", "image/tar" },  
	{"htm", "text/html" },  
	{"html","text/html" },  
	{0,0} };

static int dummy; //keep compiler happy


/*
    initialize thread pool, initialize threads, and add them to thread pool
*/
void createPool(int numThreads)
{
    thread_pool pool;
    *pool = (struct thread_pool*)malloc((sizeof(struct thread) * numThreads) + sizeof(pthread_cond_t));
    pool->threads[numThreads];
    int i;
    for(i = 0; i < numThreads; i++)
    {
        printf("creating thread %d\n", i);
        pool->&threads[i] = createThread();
    }   
    pool->cond = ;//TODO
}

/*
    initialize thread
*/
thread createThread()
{
    int status; thread thr;
    * thr = (struct thread*)malloc(sizeof(pthread_t) + (sizeof(int) * 4));
    status = pthread_create(&thr, NULL, threadWait(), NULL);
    if (status != 0)
    {
        printf("there was issue creating thread %d\n", i);
        exit(-1);
    }    
    return thr; 
}

/*
    initialize request queue(s)
        - indicator variable:
            0 for fifo 
            1 for html priority
            2 for image priority
*/
void createQueue(int indicator)
{

}


/*
    method for threads to wait for request
*/
void threadWait()
{
    
}


void logger(int type, char *s1, char *s2, int socket_fd)
{
	int fd ;
	char logbuffer[BUFSIZE*2];

	switch (type) {
	case ERROR: (void)sprintf(logbuffer,"ERROR: %s:%s Errno=%d exiting pid=%d",s1, s2, errno,getpid()); 
		break;
	case FORBIDDEN: 
		dummy = write(socket_fd, "HTTP/1.1 403 Forbidden\nContent-Length: 185\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>403 Forbidden</title>\n</head><body>\n<h1>Forbidden</h1>\nThe requested URL, file type or operation is not allowed on this simple static file webserver.\n</body></html>\n",271);
		(void)sprintf(logbuffer,"FORBIDDEN: %s:%s",s1, s2); 
		break;
	case NOTFOUND: 
		dummy = write(socket_fd, "HTTP/1.1 404 Not Found\nContent-Length: 136\nConnection: close\nContent-Type: text/html\n\n<html><head>\n<title>404 Not Found</title>\n</head><body>\n<h1>Not Found</h1>\nThe requested URL was not found on this server.\n</body></html>\n",224);
		(void)sprintf(logbuffer,"NOT FOUND: %s:%s",s1, s2); 
		break;
	case LOG: (void)sprintf(logbuffer," INFO: %s:%s:%d",s1, s2,socket_fd); break;
	}	
	/* No checks here, nothing can be done with a failure anyway */
	if((fd = open("nweb.log", O_CREAT| O_WRONLY | O_APPEND,0644)) >= 0) {
		dummy = write(fd,logbuffer,strlen(logbuffer)); 
		dummy = write(fd,"\n",1);      
		(void)close(fd);
	}
	if(type == ERROR || type == NOTFOUND || type == FORBIDDEN) exit(3);
}

/* this is a child web server process, so we can exit on errors */
void web(int fd, int hit)
{
	int j, file_fd, buflen;
	long i, ret, len;
	char * fstr;
	static char buffer[BUFSIZE+1]; /* static so zero filled */

	ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		logger(FORBIDDEN,"failed to read browser request","",fd);
	}
	if(ret > 0 && ret < BUFSIZE)	/* return code is valid chars */
		buffer[ret]=0;		/* terminate the buffer */
	else buffer[0]=0;
	for(i=0;i<ret;i++)	/* remove CF and LF characters */
		if(buffer[i] == '\r' || buffer[i] == '\n')
			buffer[i]='*';
	logger(LOG,"request",buffer,hit);
	if( strncmp(buffer,"GET ",4) && strncmp(buffer,"get ",4) ) {
		logger(FORBIDDEN,"Only simple GET operation supported",buffer,fd);
	}
	for(i=4;i<BUFSIZE;i++) { /* null terminate after the second space to ignore extra stuff */
		if(buffer[i] == ' ') { /* string is "GET URL " +lots of other stuff */
			buffer[i] = 0;
			break;
		}
	}
	for(j=0;j<i-1;j++) 	/* check for illegal parent directory use .. */
		if(buffer[j] == '.' && buffer[j+1] == '.') {
			logger(FORBIDDEN,"Parent directory (..) path names not supported",buffer,fd);
		}
	if( !strncmp(&buffer[0],"GET /\0",6) || !strncmp(&buffer[0],"get /\0",6) ) /* convert no filename to index file */
		(void)strcpy(buffer,"GET /index.html");

	/* work out the file type and check we support it */
	buflen=strlen(buffer);
	fstr = (char *)0;
	for(i=0;extensions[i].ext != 0;i++) {
		len = strlen(extensions[i].ext);
		if( !strncmp(&buffer[buflen-len], extensions[i].ext, len)) {
			fstr =extensions[i].filetype;
			break;
		}
	}
	if(fstr == 0) logger(FORBIDDEN,"file extension type not supported",buffer,fd);

	if(( file_fd = open(&buffer[5],O_RDONLY)) == -1) {  /* open the file for reading */
		logger(NOTFOUND, "failed to open file",&buffer[5],fd);
	}
	logger(LOG,"SEND",&buffer[5],hit);
	len = (long)lseek(file_fd, (off_t)0, SEEK_END); /* lseek to the file end to find the length */
	      (void)lseek(file_fd, (off_t)0, SEEK_SET); /* lseek back to the file start ready for reading */
          (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n\n", VERSION, len, fstr); /* Header + a blank line */
	logger(LOG,"Header",buffer,hit);
	dummy = write(fd,buffer,strlen(buffer));
	
    /* Send the statistical headers described in the paper, example below
    
    (void)sprintf(buffer,"X-stat-req-arrival-count: %d\r\n", xStatReqArrivalCount);
	dummy = write(fd,buffer,strlen(buffer));
    */
    
    /* send file in 8KB block - last block may be smaller */
	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		dummy = write(fd,buffer,ret);
	}
	sleep(1);	/* allow socket to drain before signalling the socket is closed */
	close(fd);
	exit(1);
}

int main(int argc, char **argv)
{
	int i, port, pid, listenfd, socketfd, hit;
	socklen_t length;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

	if( argc < 3  || argc > 3 || !strcmp(argv[1], "-?") ) {
		(void)printf("hint: nweb Port-Number Top-Directory\t\tversion %d\n\n"
	"\tnweb is a small and very safe mini web server\n"
	"\tnweb only servers out file/web pages with extensions named below\n"
	"\t and only from the named directory or its sub-directories.\n"
	"\tThere is no fancy features = safe and secure.\n\n"
	"\tExample: nweb 8181 /home/nwebdir &\n\n"
	"\tOnly Supports:", VERSION);
		for(i=0;extensions[i].ext != 0;i++)
			(void)printf(" %s",extensions[i].ext);

		(void)printf("\n\tNot Supported: URLs including \"..\", Java, Javascript, CGI\n"
	"\tNot Supported: directories / /etc /bin /lib /tmp /usr /dev /sbin \n"
	"\tNo warranty given or implied\n\tNigel Griffiths nag@uk.ibm.com\n"  );
		exit(0);
	}
	if( !strncmp(argv[2],"/"   ,2 ) || !strncmp(argv[2],"/etc", 5 ) ||
	    !strncmp(argv[2],"/bin",5 ) || !strncmp(argv[2],"/lib", 5 ) ||
	    !strncmp(argv[2],"/tmp",5 ) || !strncmp(argv[2],"/usr", 5 ) ||
	    !strncmp(argv[2],"/dev",5 ) || !strncmp(argv[2],"/sbin",6) ){
		(void)printf("ERROR: Bad top directory %s, see nweb -?\n",argv[2]);
		exit(3);
	}
	if(chdir(argv[2]) == -1){ 
		(void)printf("ERROR: Can't Change to directory %s\n",argv[2]);
		exit(4);
	}
	/* Become deamon + unstopable and no zombies children (= no wait()) */
	if(fork() != 0)
		return 0; /* parent returns OK to shell */
	(void)signal(SIGCHLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	for(i=0;i<32;i++)
		(void)close(i);		/* close open files */
	(void)setpgrp();		/* break away from process group */
	logger(LOG,"nweb starting",argv[1],getpid());
	/* setup the network socket */
	if((listenfd = socket(AF_INET, SOCK_STREAM,0)) <0)
		logger(ERROR, "system call","socket",0);
	port = atoi(argv[1]);
	if(port < 0 || port >60000)
		logger(ERROR,"Invalid port number (try 1->60000)",argv[1],0);
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	serv_addr.sin_port = htons(port);
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		logger(ERROR,"system call","bind",0);
	if( listen(listenfd,64) <0)
		logger(ERROR,"system call","listen",0);
		
	/*TODO: create threadpool struct with worker threads
	    1) malloc space for number of threads provided/create threadpool
	    2) for number of threads
	        - create thread, handing off to method to wait for condition to be fulfilled
	            - pointer to pthread
                - STAT-8: thread ID
                - STAT-9: count of http requests handled
                - STAT-10: count of HTML requests handled
                - STAT-11: count of Image requests handled
	        - add to threadpool
	*/
	
	/*TODO: create struct for requests based on the input scheduling:
	    1) fifo (queue)
	        - pointer to requests 
            - mutex
            - STAT-1: count of total requests present
            - STAT-5: completed request count  
	    and (if not fifo) 2) html/image first 
	        - pointer to requests 
            - mutex
            - HTML or JPG priority
            - STAT-1: count of total requests present
            - STAT-5: completed request count
	*/
	for(hit=1; ;hit++) {
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			logger(ERROR,"system call","accept",0);
	    /*
	        TODO: pass off request to struct holding requests:
	        1) lock
	        2) add request to queue
	            - if fifo requested or HTML/JPG requested and this is not, 
	                add to fifo queue
	            - if HTML/JPG priority requested and this is it, add to to queue
	        3) unlock
	        4) alert workers that condition (request added) fulfilled 
	     */
		
		/*TODO: remove fork
		if((pid = fork()) < 0) {
			logger(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	/* child 
				(void)close(listenfd);
				web(socketfd,hit); /* never returns 
			} else { 	/* parent 
				(void)close(socketfd);
			}
		}*/
	}
}

/**
    Method for worker thread to wait for request
*/

/**
    Method for getting request from queue once worker awake
*/

/**
    Method to add request to queue
*/
