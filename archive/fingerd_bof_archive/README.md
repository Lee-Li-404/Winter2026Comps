Fingerd Buffer Overflow
=======================

TODO
----

- [x] merge the docker setup 
- [x] refactor `_bsd` versions
- [x] create a vector program to transfer files between host and server
- [x] automate host detection 
- [x] figure out how to send information back to the prime server

Files
-----

- [`worm.h`](worm.h) - contains useful functions for creating exploit buffer, reading/writing from/to sockets, and etc.
- [`non_interactive.c`](non_interactive.c) - demonstration of stack smashing to get remote shell 
- [`send_file_over_socket.c`](send_file_over_socket.c) - file server, takes list of files to transfer (assumes the files are in the same directory as the executable)  
- [`receive_file.c`](receive_file.c) - client that connects to file server to pull files; because of how I have implemented the application layer on top of the transport layer, files must be received in the way described in this file
- [`vax_bof.h`](vax_bof.h) - yet another demonstration, but that gives you a interactive terminal

`_bsd` suffix indicates that the c code is written in old c that is recognized by c compiler on 4.3BSD systems

Notes
-----

`forking.c` is a rough outline of what the worm's going to be; it will mainly have two processes, one for server logic and one for client logic. It is the server process that manages file clients and log clients. It is the client process that attacks vulnerable machines. The attach consists of two phase. The first phase is actual fingerd and movemail attack to transfer and install malware. The first phase happens over entirely in the client process. The second phase is the transfer of core files and compilation of the worm. The second phase involves both the client and the server processes. The client is responsible for connecting to the file server to transfer files and then `execl` into a shell. Then, it is file server's job to compile and run the worm. Currently `forking.c` is only half a worm because it sends over `send_file_over_socket_bsd.c` which lacks self-propagation capabilities. However, I was able to make sense of how to use `dup2` in tandem with sockets and `fflush`.

`dup2` alone is not enough because stdout/sterr gets buffered before getting flushed. Thus, you need to explicitly call `fflush` after every `printf` statement for it to be immediately sent over the socket. Gemini spat out a way to use `_doprnt`, a lower level function to wrap printing and flushing in one function, which I have tested and seems to work rather reliably.

Currently, the logging server just dumps whatever it receives over the socket without much formatting. It would be nice if we can keep track of from which host the logging's coming from. This can be achieved by first making the logging client send over their hostname, which will be stored in some kind of data structure (most likely a fixed char * array). This way, I can format the print statements to first print out the hostname, much like the docker compose dashboard(?).

Also, my currently handling of forked processes are really bad, because I never kill the server process (child). I should make it so that the main process relays `SIGTERM` to its child process to do clean up before properly exiting.

The worm has 2 main components: 
- server
	- [x] break into vulnerable hosts
	- [x] search for attack-able hosts
	- [x] send over vector file; giant script that echos the vector file
	- [x] if vector file successfully uploads core files, compile and run the program
- vector (the original vector program, once downloading the files, turns into a shell)
	- [x] pull files from the server 
	- [x] communicate back to the server if files were successfully downloaded on the host

