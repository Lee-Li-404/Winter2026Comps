# Winter2026Comps
<img width="480" height="320" alt="worm on a keyboard" src="https://github.com/user-attachments/assets/3ea4cb00-61e2-4a5d-bab7-f4aa8af37f8a" />


General TODO:
    - Make worm.c work
    - Make visualization work for fingerd version of worm (is this already done?)
    - Create full centralized documentation
        - Update this file in particular
    - Clean up and organize repo
    - I think it would be nice to build a simple python program that does all the setup
        for the user, have it spin up docker containers, and have clear options for running the SMTP version of the worm, or fingerd version of the worm.


More refined (still very coarse) plan for demos
-----------------------------------------------

We need to "update" the base docker containers to have the `movemail` vulnerability, so that fingerd exploit can be used to get a root shell (see [bsd_additional_setup.md](src/fingerd_bof/bsd_additional_setup.md))

Instead of trying to combine the two exploits as a single worm, it may be worth while to have two set of demos (one for smtp and one for fingerd) 

I think it would be possible to share the underlying client-server architecture (either central or distributed model)

The planned unfolding of the worm (Changwoo)
---------------------------------------------

1. as a client, connect to the vulnerable fingerd socket 
2. deliver the payload and get a shell 
3. conduct privilege escalation to get a root shell (movemail and crontab) 

——————(yet to be implemented from this point) —————— 

4. install vector program on the target using the root shell 
    - one approach would be to just have the files as a giant string, echo’ing the text into a file 
    - another approach would be to load the vector program into an object like the original worm code and somehow send that over
5. compile and confirm that vector program is installed 
    - would probably involve waiting for certain magic word 
6. the vector program will be a client which tries to make a connection to the worm server 
    - as the vector program is getting compiled, the worm code should open up a new socket ( the socket info should probably be communicated with the vector program in some way) 
7. once the connection is established, send over relevant files over the socket
    - again, do i load the files somehow in an object? 
8. confirm that the file has been successfully transferred (I’m not going to worry about clean up just yet or, probably, ever) 
    - now i should have two sockets, one for the shell and another for transfer of files
    - i should use the shell socket to confirm that the file has been transferred and saved 
    - the vector program should somehow take the characters and write them to a file (file I/O) 
9. compile the worm program and execute it using the remote shell, which will bring us back to step 1. 
    - maybe a good idea to keep the socket open for sending more commands later? 


Unaddressed but important questions to consider
-----------------------------------------------

1. should i limit the number of targets that a single host can infect? 
2. how do we find the ip addresses? hard coded? 
    - another way might be to use expect script to start the simulation and run relevant network configuration commands, so that ip addresses of vax machines reflect those assigned by docker network 
3. central server or distributed? 
    - central server is where you have a prime worm server that all infected targets reports to
        - each socket message might be small, but you have to maintain a lot more sockets 
    - distributed model is where you have hierarchy of worm servers that facilitates connections with their immediate descendants 
        - each socket message might be big, but you don’t have to matain a lot of sockets 
4. Porting everything to work with SEED labs? 
	- this will make visualization much more easier (we can just `ping 1.2.3.4` and then use the seed lab visualizer); but this is only benefit that I can think of 
	- There are lot of hurdles for this undertaking, most notably the absence of sendmail program 
	- also, it appears that gcc is missing in the nodes and apt does not work because the nodes themselves are disconnected from the internet; necessary header files may not also be present in the system
