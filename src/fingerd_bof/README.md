Fingerd Buffer Overflow
=======================

TODO
----

- [ ] merge the docker setup 
- [ ] automate host detection 
- [ ] create a vector program to transfer files between host and server
- [ ] figure out how to send information back to the prime server

Files
-----

- [`worm.h`](worm.h) - contains useful functions for creating exploit buffer, reading/writing from/to sockets, and etc.
- [`non_interactive.h`](non_interactive.h) - demonstration of stack smashing to get remote shell 
- [`vax_bof.h`](vax_bof.h) - yet another demonstration, but that gives you a interactive terminal

`_bsd` suffix indicates that the c code is written in old c that is recognized by c compiler on 4.3BSD systems

