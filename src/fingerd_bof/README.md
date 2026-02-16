Fingerd Buffer Overflow
=======================

TODO
----

- [x] merge the docker setup 
- [ ] refactor `_bsd` versions
- [ ] automate host detection 
- [ ] create a vector program to transfer files between host and server
- [ ] figure out how to send information back to the prime server

Files
-----

- [`worm.h`](worm.h) - contains useful functions for creating exploit buffer, reading/writing from/to sockets, and etc.
- [`non_interactive.h`](non_interactive.h) - demonstration of stack smashing to get remote shell 
- [`vax_bof.h`](vax_bof.h) - yet another demonstration, but that gives you a interactive terminal

`_bsd` suffix indicates that the c code is written in old c that is recognized by c compiler on 4.3BSD systems

Notes
-----

The worm has 2 main components: 
- server
	- search for attack-able hosts
	- break into vulnerable hosts
	- send over vector file; giant bash script?
	- if vector file successfully uploads core files, compile and run the program
- vector (the original vector function does more than this)
	- pull files from the server 
	- communicate back to the server if files were successfully downloaded on the host

It is my current suspicion that we need every worm to become a client that connects back to the prime worm server, so that information can be gathered and viewed on the prime server

