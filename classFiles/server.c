/**
    References used:
        https://github.com/Pithikos/C-Thread-Pool/blob/master/thpool.c
        https://github.com/jonhoo/pthread_pool/blob/master/pthread_pool.c
*/

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
#include <sys/time.h>

#define VERSION 23
#define BUFSIZE 8096
#define ERROR      42
#define LOG        44
#define FORBIDDEN 403
#define NOTFOUND  404



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

typedef struct Thread {  
    pthread_t pthread;
    int id;
    int countHttpRequests;
    int countHtmlRequests;
    int countImageRequests; 
} thread;

thread* createThread();

typedef struct {
    thread **threads;  
    pthread_cond_t cond;
} thread_pool;

typedef struct {
   struct request * previous;
   int requestInfo; //fd
   struct timeval arrivalTime;
   int countDispatchedPreviously;
   int dispatchedTime;
   int readCompletionTime;
   int numRequestsHigherPriority;
   int requestType; //0 for regular, 1 for html, 2 for jpg
} request;

typedef struct {
    request** requests;
    pthread_mutex_t mutex;
    int priority; //0 for fifo, 1 for html, 2 for jpg
} request_queue;

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
}	extensions [] = {
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
    prototypes
*/
thread_pool * createPool(int numThreads);
void * threadWait();
request_queue * createQueue(int indicator);
request createRequest(int fd);
void logger(int type, char *s1, char *s2, int socket_fd);

/*
Fields
*/

thread_pool * ourThreads;
/*
    initialize Thread pool, initialize Threads, and add them to Thread pool
*/
thread_pool *  createPool(int numThreads)
{

    thread * newThreads[numThreads];
  
    thread_pool * pool = calloc(numThreads + 1, (sizeof(thread) * numThreads) + sizeof(pthread_cond_t));    
    pool->threads =  newThreads;
    int i;
    for(i = 0; i < numThreads; i++)
    {
    	//char * j;
    	//j   = 
        //sprintf("creating Thread %d\n", i);
        pool->threads[i] = createThread(i);
        logger(LOG, "we have CREATED A THREAD", "THREADING", i);  
    }   
    pthread_cond_init(&pool->cond, NULL);
    return pool;
}

/*
    initialize Thread
*/
thread * createThread(int i)
{
    /*int status;*/ thread * thr;
    thr = (struct Thread*)calloc(5, sizeof(pthread_t) + (sizeof(int) * 4));
    
    thr->id = i;
    thr->countHttpRequests = 0;
    thr->countHtmlRequests = 0;
    thr->countImageRequests = 0;
       
    pthread_create(&thr->pthread, NULL, threadWait, thr);
   /* if (status == NULL)
    {
    	logger(LOG, "we have reached heaven", "HI", 5);     

        printf("there was issue creating thread %d\n", i);
        exit(-1);
    }    */
    
    return thr; 
}

/*
    initialize request queue(s)
        - indicator variable:
            0 for fifo 
            1 for html priority
            2 for image priority
*/
request_queue * createQueue(int indicator)
{
    request * newrequests[50];//is this a random max we should have
    request_queue * rq = calloc(3, sizeof(newrequests) + sizeof(int) + sizeof(pthread_mutex_t));
    rq->requests = newrequests;
    rq->priority = indicator;
    //pthread_mutex_init(&rq->mutex, NULL);
    return rq;
}

request createRequest(int fd)
{
    request * r = calloc(7, (sizeof(int) * 6) + sizeof(request));
    r->previous = NULL;
    r->requestInfo = fd;
    gettimeofday(&r->arrivalTime, NULL);
    r->countDispatchedPreviously = 0;//TODO: how do we get this number?
    r->dispatchedTime = 0;
    r->readCompletionTime = 0;
    r->numRequestsHigherPriority = 0;
    //r->requestType = ; dont know this yes
    return * r;  
}


/*
    method for threads to wait for request
*/
void * threadWait(thread thr)
{
    return NULL;
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
	int i, port, listenfd, socketfd, hit, numThreads;//pid,
	socklen_t length;
	request_queue* fifoqueue, srqueue;
	static struct sockaddr_in cli_addr; /* static = initialised to zeros */
	static struct sockaddr_in serv_addr; /* static = initialised to zeros */

	if( argc < 6  || argc > 6 || !strcmp(argv[1], "-?") ) {
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
	printf("\n     HI        \n");
	if(bind(listenfd, (struct sockaddr *)&serv_addr,sizeof(serv_addr)) <0)
		logger(ERROR,"system call","bind",0);
	if( listen(listenfd,64) <0)
		logger(ERROR,"system call","listen",0);
		
	/*
	    create threadpool struct with worker threads
	*/
    logger(LOG, "we have reached CREATE POOL", argv[3], 5);      

	numThreads = atoi(argv[3]);
	ourThreads = createPool(numThreads);//TODO: might need to add return type threadpool
	(ourThreads-> threads[5])-> countHttpRequests = 9;
	int thNum = (ourThreads-> threads[5])-> countHttpRequests;
	
	logger(LOG, "thread number 4", "i hope", thNum);

	/*
	    create struct for requests based on the input scheduling:
	*/
	int preference = 0;
	if(strcmp(argv[5], "FIFO") || strcmp(argv[5], "ANY")) //any treated as FIFO
	{
	    //create just fifo queue
	    fifoqueue = createQueue(preference); 
	 	logger(LOG, "we have reached FIFO", argv[5], 5);      
	}
	else if(strcmp(argv[5], "HPHC") || strcmp(argv[5], "HPIC"))
	{
	    //create fifo queue for everything other than preference 
	    fifoqueue = createQueue(preference); 
	    //create special queue 
	    if(!strcmp(argv[5], "HPHC"))
	    {
	        preference = 1;
	    }
	    else
	    {
	        preference = 2;
	    }
	    srqueue = createQueue(preference);
	}

	else
	{
	    logger(ERROR,"system call","createQueue",0);//can we personalize this logger error?
	}
	
	int pri = srqueue->priority;
	logger(LOG, "checking", "priority number",pri);
	logger(LOG, "we have reached here", argv[5], 5); 
	
	//"portNum: %d  folder: %s  NumThreads: %d  schedule num: %d\n ", port, "folder", numThreads,preference);
	for(hit=1; ;hit++) {
		length = sizeof(cli_addr);
		if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0)
			logger(ERROR,"system call","accept",0);
		//read file to see if .jpg, .gif, or .png
		static char buffer[BUFSIZE+1]; /* static so zero filled */
		long ret = read(socketfd,buffer,BUFSIZE); 	/* read Web request */
		char * requestLine;
		requestLine = (char*)&ret; //https://stackoverflow.com/a/16537142
		if(preference != 0)//has a preference
		{
		    if((strstr(requestLine, ".jpg") != 0) || (strstr(requestLine, ".png") != 0) || (strstr(requestLine, ".gif") != 0)) //must be image
		    {
		
		    }
		    else if (strstr(requestLine, ".html") != 0)//html request
		    {
		    
		    }
		}
		//create request
		// UNUSED VARIABLE    request r = createRequest(socketfd);
		//set requestType and stats
	    /*
	        TODO: pass off request to struct holding requests:
	        1) lock
	        2) add request to queue
	    */
	            requestsPresentCount++;
                requestCountTotal++;
	           /* - if fifo requested or HTML/JPG requested and this is not, 
	                add to fifo queue
	            - if HTML/image priority requested and this is it, add to to queue
	        3) unlock
	        4) alert workers that condition (request added) fulfilled 
	     */
		
		/*TODO: remove fork
		if((pid = fork()) < 0) {
			logger(ERROR,"system call","fork",0);
		}
		else {
			if(pid == 0) { 	// child 
				(void)close(listenfd);
				web(socketfd,hit); // never returns 
			} else { 	// parent 
				(void)close(socketfd);
			}
		}*/
	}
}

/**
    Method for getting request from queue once worker awake
*/

/*
    Method to add request to queue
*/
