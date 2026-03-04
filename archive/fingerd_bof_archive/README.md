Fingerd Buffer Overflow
=======================

TODO
----

- [x] merge the docker setup 
- [x] refactor `_bsd` versions
- [x] create a vector program to transfer files between host and server
- [ ] automate host detection 
- [ ] figure out how to send information back to the prime server

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

The worm has 2 main components: 
- server
	- [x] break into vulnerable hosts
	- [ ] search for attack-able hosts
	- [ ] send over vector file; giant script that echos the vector file
	- [ ] if vector file successfully uploads core files, compile and run the program
- vector (the original vector program, once downloading the files, turns into a shell)
	- [x] pull files from the server 
	- [x] communicate back to the server if files were successfully downloaded on the host

