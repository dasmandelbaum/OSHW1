•	Group info 
	1) Judah Brick 
	2) Jeffrey Hagler 
	3) David Mandelbaum 
	
	*Breakdown of effort:*
	Server - all of us together
	Client - Jeffrey and Judah
	Stats - David and Judah 
	Scheduling - Jeffrey and David
	
•	Design overview: 
	*Important structs*
	1) Thread struct with pthread and stats
	2) Thread pool struct with threads 
	3) Request struct with stats
	4) Request queue struct with requests and stats
	- A FIFO queue was created for every server, which holds either all of the requests
    or the requests not in a special queue. A special queue would be created only if the 
    server client line demands HPIC or HPHC. 
    - Threads created upon server startup, added to thread pool, the server then receives 
    request from client, adds request to correct request queue, then dispatches worker to
    handle the request.  

•	Complete specification: Describe how you handled any ambiguities in the specification. 
	- ANY was treated as regular FIFO.
    - FIFO vs CONCUR - 
    Our understanding of the difference between FIFO & CONCUR was this: 
    FIFO required that only one thread at a time in the client was allowed to send a request 
    at a time.  In CONCUR all threads are allowed to send requests at the same time.  
    For this reason the only main difference we have between FIFO and CONCUR in the Client
    is that in FIFO there is a mutex lock on the GET method that sends a GET request to the server.

**Known bugs or problems:** 
	As far as we know when running on a linux macheine and changing the port number 
	between each test the server and client should be working 100%
	- FIFO vs CONCUR - FILL IN 
**Testing:** 

	For testing our server we did a number of things.  We started out by using one server 
	servicing one request at a time, using the original client.  
	Later when we had confidence in our multithreaded server 
	we made a shell script to send multiple client requests at once.  
	After we finished our mulit-threaded client we  merged two of our git branches 
	and would just run our multi-threaded client against our multi-threaded sever
	and saw that both were working well together.
	
	Our client was really only fully tested once we finished our server 
	and we knew that it was capable of handling all of the requests at once.
	
	We found that through testing it was better to change the port number each new test.  
	Sometimes even after killing a server we would try to use the same port number but it wouldn't work. 
	
	We were able to see if our server was properly running the schedule requirements through the log.
	We were able to see what number request, of which type and from which request queue it was taken off of.
	
	
**Concurrency:**
	Both our client and our server are capable of running concurrently.  
	When looking through the logs and print outs you can see that when using CONCUR on the client 
	you can see how all of the threads are running at different parts of the code.
	In order to see this you can type     ./server 3333 . 4 10 HPIC &     into the command line
	followed by      ./client localhost 3333 CONCUR /index.html /nigel.jpg &
	

**SCHEDULING**
	
Highest Priority to Image Content (HPIC):  
	./server 4444 . 4 10 HPIC &
	./client localhost 4444 CONCUR /index.html /nigel.jpg &
	
	when running this you should see in the output that all of the received image requests
	will be serviced before all other requests.  What is important to notice is that there
	are no html requests who's arrival time will be later than an image request's arrival time
	and have an earlier dispatch time than the image request; meaning there will not be an
	html request received after an image request and serviced before the image request.
	
Highest Priority to HTML Content (HPHC):
	./server 5555 . 4 10 HPHC &
	./client localhost 5555 CONCUR /index.html /nigel.jpg &
	
	When running this we will se the exact oposite as in HPIC.  All of the html requests 
	will be serviced before the image requests even if it arrived later.
	
First-in-First-out (FIFO): 
	./server 6666 . 4 10 FIFO &
	./client localhost 6666 CONCUR /index.html /nigel.jpg &
	
	In FIFO we should never see a request that arrived later than any other request being serviced
	before it's friend who arrived earlier.  The first request the server receives,
	it services it and then the second and so on.
	
Any Concurrent Policy (ANY):
	./server 7777 . 4 10 ANY &
	./client localhost 7777 CONCUR /index.html /nigel.jpg &
	
	This will work just like FIFO.





