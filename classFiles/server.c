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



typedef struct thread_pool{
    thread **threads;  
    pthread_cond_t cond;
} thread_pool;

typedef struct request{
   struct request * behind;
   int requestInfo; //fd
   struct timeval arrivalTime;
   int countDispatchedPreviously;
   struct timeval dispatchedTime;
   struct timeval readCompletionTime;
   int numRequestsHigherPriority;
   int requestType; //0 for regular, 1 for html, 2 for image
   int hit;
   int numberRequest;
   long ret;
   char requestLine[BUFSIZE + 1];//took away "static"
} request;

typedef struct request_queue{
    request** requests;
    request * first;
    request * last;
    int length;
    pthread_mutex_t mutex;
    int priority; //0 for fifo, 1 for html, 2 for image
} request_queue;


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
    prototypes - see below
*/
thread_pool * createPool(int numThreads);
void * threadWait();
request_queue createQueue(int indicator);
request createRequest(int fd, int hit);
void logger(int type, char *s1, char *s2, int socket_fd);
void addRequest(request * req);
thread* createThread(int i);
request * removeRequest();
void web(int fd, int hit, request req, thread * thr, long ret);
int timeval_subtract (struct timeval *result,struct timeval *x,struct timeval *y);

/*
Fields
*/
thread_pool * ourThreads;
request_queue fifoqueue, srqueue;
int preference = 0;
int maxTotalQueueSize;
pthread_mutex_t queueMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t jobavail = PTHREAD_COND_INITIALIZER;
//pthread_cond_t hasJobs;
static int requestsPresentCount;//STAT-1: count of total requests present
static int completedRequestsCount;//STAT-5: completed request count 
struct timeval startUpTime;
static int dispatchedRequests = 0;
static int totalRequestsReceived = 0;

/*
    initialize Thread pool, initialize Threads, and add them to Thread pool
*/
thread_pool *  createPool(int numThreads)
{
    thread * newThreads[numThreads];
  
    thread_pool * pool = calloc(numThreads + 1, (sizeof(thread) * numThreads) + sizeof(pthread_cond_t));    
    pool->threads =  newThreads;
    int i;        
    logger(LOG, "Number of threads", "", numThreads);  

    for(i = 0; i < numThreads; i++)
    {
        pool->threads[i] = createThread(i);
        logger(LOG, "we have CREATED A THREAD", "THREADING", pool->threads[i]->id);  
    }   
    pthread_cond_init(&pool->cond, NULL);
    return pool;
}

/*
    initialize Thread
*/
thread * createThread(int i)
{
    thread * thr;
    thr = calloc(5, (sizeof(int) * 4) + sizeof(pthread_t));
    thr->countHttpRequests = 0;
    thr->countHtmlRequests = 0;
    thr->countImageRequests = 0;
    logger(LOG, "About to create pThread", "Number", i);   
    int status = pthread_create(&thr->pthread, NULL, threadWait, thr);
    if (status != 0)
    {
    	logger(LOG, "thread creation failed", "uh oh", 5);     
        exit(-1);
    }    
    thr->id = i;
    logger(LOG, "checking id", "in createthread", thr->id);
    return thr; 
}

/*
    initialize request queue(s)
        - indicator variable:
            0 for fifo 
            1 for html priority
            2 for image priority
*/
request_queue createQueue(int indicator)
{
    pthread_mutex_init(&queueMutex, NULL);
    request * newrequests[maxTotalQueueSize];//is this a random max we should have
    request_queue * rq = calloc(3, sizeof(newrequests) + sizeof(int) + sizeof(pthread_mutex_t));
    rq->requests = newrequests;
    rq->priority = indicator;
    rq->first = NULL;
    rq->last = NULL;
    rq->length = 0;
    return * rq;
}

request createRequest(int fd, int hit)
{
    struct timeval now;
    request * r = calloc(9, (sizeof(int) * 6) + sizeof(request) + sizeof(long) + (BUFSIZE + 1));// are we allocating too much space
    r->behind = NULL;
    r->requestInfo = fd;
    gettimeofday(&now, NULL);
    timeval_subtract(&r->arrivalTime, &now, &startUpTime);
    r->countDispatchedPreviously = 0;//TODO: how do we get this number?
    r->numRequestsHigherPriority = 0;
    r->hit = hit;
    r->numberRequest = totalRequestsReceived;
    //other fields declared later
    return * r;  
}

/*
    http://www.gnu.org/savannah-checkouts/gnu/libc/manual/html_node/Elapsed-Time.html
    and https://www.linuxquestions.org/questions/programming-9/how-to-calculate-time-difference-in-milliseconds-in-c-c-711096/

int timeval_subtract (struct timeval * result,struct timeval * x,struct timeval * y)
{
   Perform the carry for the later subtraction by updating y. 
  if (x->tv_usec < y->tv_usec) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
    y->tv_usec -= 1000000 * nsec;
    y->tv_sec += nsec;
  }
  if (x->tv_usec - y->tv_usec > 1000000) {
    int nsec = (y->tv_usec - x->tv_usec) / 1000000;
    y->tv_usec += 1000000 * nsec;
    y->tv_sec -= nsec;
  }

   Compute the time remaining to wait.
     tv_usec is certainly positive. 
  result->tv_sec = x->tv_sec - y->tv_sec;
  result->tv_usec = x->tv_usec - y->tv_usec;

  Return 1 if result is negative. 
  return (x->tv_sec < y->tv_sec);
}*/

int timeval_subtract(struct timeval *result, struct timeval *x, struct timeval *y)
{
  struct timeval xx = *x;
  struct timeval yy = *y;
  x = &xx; y = &yy;

  if (x->tv_usec > 999999)
  {
    x->tv_sec += x->tv_usec / 1000000;
    x->tv_usec %= 1000000;
  }

  if (y->tv_usec > 999999)
  {
    y->tv_sec += y->tv_usec / 1000000;
    y->tv_usec %= 1000000;
  }

  result->tv_sec = x->tv_sec - y->tv_sec;

  if ((result->tv_usec = x->tv_usec - y->tv_usec) < 0)
  {
    result->tv_usec += 1000000;
    result->tv_sec--; // borrow
  }

  return result->tv_sec < 0;
}


void addRequest(request * req)
{
    if(req->requestType == preference && preference > 0)
    {
        //queue = &srqueue;
        if(srqueue.length == 0)
		{
			srqueue.first = req;	
			srqueue.last = req;	
		}
		else
		{
			srqueue.last->behind = req;
			srqueue.last = req;
		}
		srqueue.length++;
    }
    else //set queue to default fifo queue
    {
       if(fifoqueue.length == 0)
		{
			fifoqueue.first = req;	
			fifoqueue.last = req;	
		}
		else
		{
			fifoqueue.last->behind = req;
			fifoqueue.last = req;
		}
		fifoqueue.length++;
    }
	logger(LOG, "request fd", "", req->requestInfo);
	totalRequestsReceived++;
	requestsPresentCount++;
	//logger(LOG, "request info off of first in queue", "", fifoqueue.first->requestInfo);//test
}

/*
    method for threads to wait for request
*/
void * threadWait(thread * thr)
{
    //thread threadhere = thr;
    //logger(LOG, "in threadwait", "with thread", threadhere.id);//test
	request * req; struct timeval now2;
	int arrival = 0;
    while(1)
    {
    	pthread_mutex_lock(&queueMutex);//lock mutex
    	pthread_cond_wait(&jobavail, &queueMutex);
    	//logger(LOG, "mutex locked", "thread number ", thr.id);//testing
        req = removeRequest();
        //logger(LOG, "request received", "with thread", thr.id);//test
        gettimeofday(&now2, NULL);
	    timeval_subtract(&req->dispatchedTime, &now2, &startUpTime); 
	    //thr->id = 100;//ID test - works
	    req->countDispatchedPreviously = dispatchedRequests;
	    req->numRequestsHigherPriority =  0;
	    arrival = req->numberRequest;
	    if(dispatchedRequests  >  arrival){
	    	req->numRequestsHigherPriority = (dispatchedRequests - arrival ) ;
	    }

        web(req->requestInfo, req->hit, *req, thr, req->ret);
        req = NULL;
        completedRequestsCount++;
        dispatchedRequests++;
        pthread_mutex_unlock(&queueMutex);
        logger(LOG, "number of requests serviced", "...", completedRequestsCount);
    }

    return NULL;
}

request * removeRequest() {
	request * req;
    if(preference != 0 && srqueue.length > 0) {
    	logger(LOG, "remove request", "taking from SPECIAL", 0);
    	logger(LOG, "                                how many are in SPECIAL", "", srqueue.length);
       	req = srqueue.first;	//TODO make sure this doesn't break the queue??
       	if(srqueue.length > 1){
       		srqueue.first = req->behind;
       	}
       	else{
       		srqueue.first = NULL;
       		srqueue.last = NULL;
       	}
       	srqueue.length--;
    }
    
    else {
    	logger(LOG, "remove request", "taking from FIFO", 0);
    	logger(LOG, "								how many are in FIFO", "", fifoqueue.length);
    	req = fifoqueue.first;
    	fifoqueue.first = req->behind;//NULL POINTER ERROR?
    	if(fifoqueue.length > 1){
       		fifoqueue.first = req->behind;
       	}
       	else{
       		fifoqueue.first = NULL;
       		fifoqueue.last = NULL;
       	}
       	fifoqueue.length--;
    }
	requestsPresentCount--;
    return req;
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

void web(int fd, int hit, request req, thread * thr, long ret)
{
	int j, file_fd, buflen;
	struct timeval now3;
	long i, /*ret,*/ len;
	char * fstr;
	//static char buffer[BUFSIZE+1]; /* static so zero filled */
	char * buffer = req.requestLine;
	logger(LOG, "in web", "i hope", hit);
	//ret =read(fd,buffer,BUFSIZE); 	/* read Web request in one go */
	logger(LOG, "in web", "read done", fd);
	//logger(LOG, "in web", "checking thread id", thr->id);//test
	if(ret == 0 || ret == -1) {	/* read failure stop now */
		logger(FORBIDDEN,"failed to read browser request",buffer,fd);
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
    (void)sprintf(buffer,"HTTP/1.1 200 OK\nServer: nweb/%d.0\nContent-Length: %ld\nConnection: close\nContent-Type: %s\n", VERSION, len, fstr); /* Header + a blank line */
	logger(LOG,"Header",buffer,hit);
	
	if((strstr(fstr, "jpg") != 0) || (strstr(fstr, "png") != 0) || (strstr(fstr, "gif") != 0)) //must be image
	{
	    thr->countImageRequests++;
	}
	else if(strstr(fstr, "html") != 0)//html request
	{
	     thr->countHtmlRequests++;
	}
	thr->countHttpRequests++;//total requests handled by thread
	
	gettimeofday(&now3, NULL);
	timeval_subtract(&req.readCompletionTime, &now3, &startUpTime); //read completion time
	dummy = write(fd,buffer,strlen(buffer));
	
	
    /* Send the statistical headers described in the instructions*/
    (void)sprintf(buffer,"X-stat-req-arrival-count: %d\r\n", req.numberRequest);
    logger(LOG,"X-stat-req-arrival-count",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-req-arrival-time: %lu\r\n", (req.arrivalTime.tv_sec) * 1000 + (req.arrivalTime.tv_usec) / 1000);//https://stackoverflow.com/a/3756954
    logger(LOG,"X-stat-req-arrival-time",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-req-dispatch-count: %d\r\n", req.countDispatchedPreviously);//doesn't work
    logger(LOG,"X-stat-req-dispatch-count",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-req-dispatch-time: %lu\r\n", (req.dispatchedTime.tv_sec) * 1000 + (req.dispatchedTime.tv_usec) / 1000);//seems to work?
    logger(LOG,"X-stat-req-dispatch-time",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-req-complete-count: %d\r\n", completedRequestsCount);//seems to work
    logger(LOG,"X-stat-req-complete-count",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-req-complete-time: %lu\r\n", (req.readCompletionTime.tv_sec) * 1000 + (req.readCompletionTime.tv_usec) / 1000);//seems to work?
    logger(LOG,"X-stat-req-complete-time",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-req-age: %d\r\n", req.numRequestsHigherPriority);//not yet implemented
    logger(LOG,"X-stat-req-age",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-thread-id: %d\r\n", thr->id);//seems to work?
    logger(LOG,"X-stat-thread-id",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-thread-count: %d\r\n", thr->countHttpRequests);//seems to work?
    logger(LOG,"X-stat-thread-count",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-thread-html: %d\r\n", thr->countHtmlRequests);//seems to work?
    logger(LOG,"X-stat-thread-html",buffer,hit);
    dummy = write(fd,buffer,strlen(buffer));
    (void)sprintf(buffer,"X-stat-thread-image: %d\r\n\r\n", thr->countImageRequests);//works
	logger(LOG,"Header",buffer,hit);
	dummy = write(fd,buffer,strlen(buffer));
    
    /* send file in 8KB block - last block may be smaller */
	while (	(ret = read(file_fd, buffer, BUFSIZE)) > 0 ) {
		dummy = write(fd,buffer,ret);
	}
	sleep(1);	/* allow socket to drain before signalling the socket is closed */
	close(fd);
	//new comment
	//exit(1);
}

int main(int argc, char **argv)
{
	int i, port, listenfd, socketfd, hit, numThreads;//pid,
	socklen_t length;
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
	gettimeofday(&startUpTime, NULL);
	/* Become deamon + unstopable and no zombies children (= no wait()) */
	if(fork() != 0)
		return 0; /* parent returns OK to shell */
	(void)signal(SIGCHLD, SIG_IGN); /* ignore child death */
	(void)signal(SIGHUP, SIG_IGN); /* ignore terminal hangups */
	for(i=0;i<32;i++)
		(void)close(i);		/* close open files */
	(void)setpgrp();		/* break away from process group */
	
	maxTotalQueueSize = atoi(argv[4]);
	logger(LOG,"\n\n nweb starting",argv[1],getpid());
	
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
	ourThreads = createPool(numThreads);
	//(ourThreads-> threads[5])-> countHttpRequests = 9;
	//int thNum = (ourThreads-> threads[5])-> countHttpRequests;//test
	//logger(LOG, "thread number", "i hope", thNum);//test
	
	
	/*
	    determine sizes for array(s)
	*/

	/*
	    create struct for requests based on the input scheduling:
	*/
	if(!strcmp(argv[5], "FIFO") || !strcmp(argv[5], "ANY")) //any treated as FIFO
	{
	    //create just fifo queue
	    fifoqueue = createQueue(preference); 
	 	logger(LOG, "we have reached FIFO", argv[5], preference);      
	}
	else if(!strcmp(argv[5], "HPHC") || !strcmp(argv[5], "HPIC"))
	{
	    //create fifo queue for everything other than preference 
	    fifoqueue = createQueue(preference); 
	    //create special queue if preference > 0
	    if(!strcmp(argv[5], "HPHC"))
	    {
	        preference = 1;
	        logger(LOG, "we have reached HPHC", argv[5], preference); 
	    }
	    else
	    {
	        preference = 2;
	        logger(LOG, "we have reached HPIC", argv[5], preference); 
	    }
	    srqueue = createQueue(preference);
	}

	else
	{
	    logger(ERROR,"system call","createQueue",0);//can we personalize this logger error?
	}
	
	//"portNum: %d  folder: %s  NumThreads: %d  schedule num: %d\n ", port, "folder", numThreads,preference);
	for(hit=1; ;hit++) {
		logger(LOG, "starting", "loop", hit);

		length = sizeof(cli_addr);
		if(requestsPresentCount < maxTotalQueueSize)
	    {
		    if((socketfd = accept(listenfd, (struct sockaddr *)&cli_addr, &length)) < 0) {
			    logger(ERROR,"system call","accept",0);
		    }
		
		logger(LOG, "reached here", "...", 513);
		request rq = createRequest(socketfd, hit); //still need to get priority type
		//logger(LOG, "checking", "request creation", rq.dispatchedTime);//test
		/* read Web request, test if img or html preference and pass to web after */
        //static char requestLine[BUFSIZE + 1]; /* static so zero filled */
        long ret = read(socketfd,rq.requestLine,BUFSIZE); 	
		if(ret == 0 || ret == -1) {	/* read failure stop now */
			logger(FORBIDDEN,"failed to read browser request","",612);
		}
		rq.ret = ret;
		if(preference != 0)//has a preference
		{
		    //read file to see if .jpg, .gif, or .png
		    
		    
		    logger(LOG, "we have reached request line", rq.requestLine, socketfd); 
		    if((strstr(rq.requestLine, ".jpg") != 0) || (strstr(rq.requestLine, ".png") != 0) || (strstr(rq.requestLine, ".gif") != 0)) //must be image
		    {
		        logger(LOG, "request line contains image", rq.requestLine, 5); 
		        rq.requestType = 2;
		    }
		    else if (strstr(rq.requestLine, ".html") != 0)//html request
		    {
		        logger(LOG, "request line contains html", rq.requestLine, 5); 
		        rq.requestType = 1;
		    }
		}
		else //everything in fifo
		{
		    rq.requestType = 0;
		}
		
		logger(LOG, "preference set to", "....", preference); 
		
		//set requestType and stats
	    /*
	        TODO: pass off request to struct holding requests:
	        1) lock
	        2) add request to queue
	    */
	    if(requestsPresentCount < maxTotalQueueSize)
	    {
	        logger(LOG, "about to add request", "woohoo", rq.requestType); 
	        pthread_mutex_lock(&queueMutex);//lock mutex
	        logger(LOG, "mutex locked", "in main method", hit);
	        addRequest(&rq);
	        pthread_mutex_unlock(&queueMutex);
	        logger(LOG, "mutex unlocked", "in main method", hit);
	        //signal thread that there is job to grab
	        logger(LOG, "sending signal", "in main method", hit);
	        pthread_cond_signal(&jobavail);
	    }
	    else
	    {
	        logger(ERROR, "request overload", "darn.", rq.requestType); 
	    }
	    }
	}
}
