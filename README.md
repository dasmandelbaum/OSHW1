•	Group info \n
    1) Judah Brick \n
    2) Jeffrey Hagler 
    3) David Mandelbaum 
    Breakdown of effort:
        Server - all of us together
        Client - Jeffrey and Judah
        Stats - David and Judah 
        Scheduling - Jeffrey and David
•	Design overview: 
    - Important structs
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
    - FIFO vs CONCUR - FILL IN 
•	Known bugs or problems: A list of any features that you did not implement 
or that you know are not working correctly 
    - FIFO vs CONCUR - FILL IN 
•	Testing: This requirement an aspect that I am very interested in.
    Describe how you tested the functionality of your web server. 
    Describe how can you use the various versions of your extended client to see 
    if the server is handing requests concurrently and implementing 
    the FIFO, HPHC, or HPIC policies. 
    Specifically, what are the exact parameters one should pass to the client and 
    the server to demonstrate that the server is handling requests concurrently? 
    To demonstrate that the server is correctly running the FIFO policy? 
    the HPIC policy? 
    the HPHC policy? 
    In each case, if your client and server are behaving correctly, 
    what output and statistics should you see? 
