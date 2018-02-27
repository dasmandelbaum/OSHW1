  /* Generic */
  #include <errno.h>
  #include <stdio.h>
  #include <stdlib.h>
  #include <string.h>
  #include <unistd.h>
  #include <pthread.h>
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

  /* Network */
  #include <netdb.h>
  #include <sys/socket.h>

  #define BUF_SIZE 100

  //global variable
  int schedule;
  int numThreads;
  int requestNumber = 0;

  pthread_barrier_t our_barrier;
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  char * host;
  char * portnum;

  void GET(int clientfd, char *path);
  void wait();

  typedef struct thread {
      pthread_t pthread;
      int id;
      int fd;
      char * filename;
      pthread_cond_t cond;
  } thread;

  typedef struct thread_pool {
      thread **threads;
      pthread_cond_t cond;
  } thread_pool;

  thread_pool * pool;

  void * threadWait();

  thread * createThread(int i, int fd, char * filename)
  {
      /*int status;*/ thread * thr;
      thr = calloc(5, sizeof(pthread_t) + (sizeof(int) * 4));

      thr->id = i;
      thr->filename = filename;
      //thr->fd = fd;
      pthread_cond_init(&(thr->cond), NULL);
      printf("ID: %d\n", thr->id);
      int status = pthread_create(&thr->pthread, NULL, threadWait, thr);
      printf("ID: %d\n", thr->id);
     if (status != 0)
      {
          //logger(LOG, "we have reached heaven", "HI", 5);
          printf("there was issue creating thread %d\n", i);
          exit(-1);
      }    
      printf("file in thread: %s\n", thr->filename);
      return thr;
  }

  thread_pool *  createPool(int fd, char * filename1,char * filename2)//, char * filename2)
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
          //TODA need to split the files across the threads depending on if there are 2 files or not
          //if there are 2 files do a mod (mod 2) calculation to split up the files evenly between the threads

          //if there are two files and it's odd then pass filename2
          if(filename2 != NULL && i % 2 != 0) {//if this doesnt work we will make a global variable to true or false if there is a second file
            printf("2 files found\n");
            pool->threads[i] = createThread(i, fd, filename2);
          }

          //else pass file1
          else {
            printf("1 file found\n");
            pool->threads[i] = createThread(i, fd, filename1);
          }
      }

      return pool;
  }

  //CONCUR vs. FIFO (schedalg is global variable)
  void * threadWait(thread *thr)
  {
  	//thread * thr2 = (thread *) thr;
    printf("in thread WAITTTT\n");
  	char buf[BUF_SIZE];
  	
  	while(1) {
  		 // if(schedule == 0) { //if FIFO
        printf("about to LOCK MUTEX with %d\n", thr->id);
  		pthread_mutex_lock(&mutex);
  			/*if it's the first one then don't wait
  			 if(requestNumber != 0) {
  				  pthread_cond_wait(&(thr->cond), &mutex);
  			 }*/
          //requestNumber++;s
  		 // }
  		thr->fd = establishConnection(getHostInfo(host, portnum));
    	if (clientfd == -1) {
     	    printf("[main:73] Failed to connect to: %s:%s \n", host, portnum);
  	    }
  		GET(thr->fd, thr->filename);
  		// if FIFO send a signal to next thread that it can send
  		  //if(schedule == 0) {
        printf("about to UNLCOK MUTEX with %d\n", thr->id);
  			 //int nextThread = ((thr->id) +1) % numThreads;
  			 //pthread_cond_t nextCondition = (pool->threads[nextThread])->cond;
  			 //pthread_cond_signal(&nextCondition);
  		pthread_mutex_unlock(&mutex);
  		 // }
        printf("about to do while \n");
  		while (recv(thr->fd, buf, BUF_SIZE, 0) > 0) {
      	    fputs(buf, stdout);
     	 	memset(buf, 0, BUF_SIZE);
     	 }
        close(thr->fd);
        if(schedule == 0)
        {
            printf("AT THE BARRIER...%d\n", thr->id);
     	    pthread_barrier_wait(&our_barrier);
            printf("AFTER THE BARRIER...%d\n", thr->id);
        }
    }
  	
      return NULL;
  }

  // Get host information (used to establishConnection)
  struct addrinfo *getHostInfo(char* host, char* port) {
    int r;
    struct addrinfo hints, *getaddrinfo_res;
    // Setup hints
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    if ((r = getaddrinfo(host, port, &hints, &getaddrinfo_res))) {
      fprintf(stderr, "[getHostInfo:21:getaddrinfo] %s\n", gai_strerror(r));
      return NULL;
    }

    return getaddrinfo_res;
  }

  // Establish connection with host
  int establishConnection(struct addrinfo *info) {
    if (info == NULL) return -1;

    int clientfd;
    for (;info != NULL; info = info->ai_next) {
      if ((clientfd = socket(info->ai_family,
                             info->ai_socktype,
                             info->ai_protocol)) < 0) {
        perror("[establishConnection:35:socket]");
        continue;
      }

      if (connect(clientfd, info->ai_addr, info->ai_addrlen) < 0) {
        close(clientfd);
        perror("[establishConnection:42:connect]");
        continue;
      }

      freeaddrinfo(info);
      return clientfd;
    }

    freeaddrinfo(info);
    return -1;
  }

  // Send GET request
  void GET(int clientfd, char *path) {
    char req[1000] = {0};
    sprintf(req, "GET %s HTTP/1.0\r\n\r\n", path);
    printf("%s\n", req);
    send(clientfd, req, strlen(req), 0);
  }

  int main(int argc, char **argv) {
    	int clientfd;
    	//char buf[BUF_SIZE];
    	char * file1;
    	char * file2;

    	if (argc < 6 || argc > 7) {
      	fprintf(stderr, "USAGE: ./httpclient [host] [portnum] [threads][schedalg] [filename1] [filename2] \n");
      	return 1;
    	}
    	// Establish connection with <hostname>:<port>
    	host = argv[1];
    	portnum = arv[2];
    	

    	if(!strcmp(argv[4], "FIFO")){
        printf("PICKED FIFO");
    		schedule = 0;
    	}
    	else if(!strcmp(argv[4], "CONCUR")){
        printf("PICKED CONCUR");
    		schedule = 1;
   	 }
   	 else{
      printf("PICKED OOPS");
    	fprintf(stderr,
     	       	  "[main] need to choose a schedule preference in arg 4");
      	   	 return 3;
    	}
    
    	file1 = argv[5];

    	if(argc == 7) {
    		file2 = argv[6];
    	}

    	else {
    		file2 = NULL;
    	}
    	
    	numThreads = atoi(argv[3]);
    	pthread_barrier_init(&our_barrier, NULL, numThreads);
    	printf("file name %s\n", file1);
      pool = createPool(clientfd, file1, file2); //will need to pass in both later
      while(1)
      {
        //spin until killed!
      }
      //wait(1000);
    // Send GET request > stdout
    
    /*
    GET(clientfd, argv[5]);
    while (recv(clientfd, buf, BUF_SIZE, 0) > 0) {
      fputs(buf, stdout);
      memset(buf, 0, BUF_SIZE);
    }
  	*/
    //close(clientfd);
    //return 0;
  }

