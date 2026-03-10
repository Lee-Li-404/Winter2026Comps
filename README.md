# Winter2026Comps
<img width="480" height="320" alt="worm on a keyboard" src="https://github.com/user-attachments/assets/3ea4cb00-61e2-4a5d-bab7-f4aa8af37f8a" />

Welcome to our comps project repository! For our project, we chose to analyze a historical piece of malware, the morris worm. This repository contains all of the code needed to launch our (mostly) faithful historical recreation of the worm (written in old C) in a simulated 1988 internet made up of docker containers all running 4.3BSD Unix. Hope you find it interesting!

To run the automated version of the worm setup and attack
-

1. Set up the necessary docker containers
- Open this repository in your terminal, and run `cd ./main/docker`
- Run `./build.sh` to build the containers with the nescessary
    simh and 4.3BSD images

2. Launch the attack
- Run `cd ./main`
    - To run the sendmail debug exploit worm, run `./run_worm.sh -sendmail`
    - To run the fingerd buffer overflow attack worm, run `./run_worm.sh -fingerd`
- Navigate to the live visualization that should have automatically opened on your browser to watch the worm spread in real time!


To manually run the sendmail worm:
-

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run `cd ./main/docker`
    - Run `./build.sh` to build the containers with the necessary 
        simh and 4.3BSD images
    - Run `docker compose up` to start the network of containers

2. Launch the attack
    - Attach to the attacker node by running `docker attach attacker`
    - Press enter, and when prompted for a login, type `root`, and then hit enter again.
    - In the folder /main/sendmail, open the maunal_sendmail_attack.txt file, and enter the commands in there, hitting enter and allowing the machine to respond after each one.


To manually run the fingerd worm:
-

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run `cd ./main/docker`
    - Run `./build.sh` to build the containers with the necessary 
        simh and 4.3BSD images
    - Run `docker compose up` to start the network of containers

2. Launch the attack
    - Attach to the attacker node by running `docker attach attacker`
    - Press enter, and when prompted for a login, type `root`, and then hit enter again.
    - To get the worm files onto the attacking machine, run these exact commands in order:
   ```
        cd /tmp
        mt -f /dev/rmt12 rew
        tar xvf /dev/rmt12
        cc worm_bsd.c -o worm_bsd
        cc send_file_over_socket_bsd.c -o send_file_over_socket_bsd
    ```
    - Then, to launch the live infection tracker, run `python3 -m http.server 8000`
    - Navigate to [localhost:8000/visualization](http://localhost:8000/visualization/) in your browser to view the live infection tracker
    - Then, back in the simh environment, run `./worm_bsd` to start the worm.
  
To manually run the fingerd worm with live logging: 
-

The main difference of this version of the worm is that you start the worm on your host machine, 
which will infect `attacker` node to spread the worm inside to the network of 4.3 bsd machines. 
It also supports live logging, so you can see what the worms are doing as you are viewing the live 
tracker.

1. Set up necessary docker containers
    - Open this repository in your terminal, and run `cd ./main/docker`
    - Run `./build.sh` to build the containers with the necessary 
        simh and 4.3BSD images
    - Run `docker compose up` to start the network of containers
    - Once all the containers are booted, launch the live infection tracker by running `python3 -m http.server 8000`

2. Launch the attack
    - Open this repository in your terminal, and run `cd ./main/fingerd`
    - Compile the worm `gcc worm.h -o worm`
    - Run the worm `./worm`
    - See the worm unfold with live logging  

File Descriptions:
-

- /main
    - `README.md`: this file, general documentation
    - `run_worm.sh`: script that automatically sets up and runs a specified version of the worm
    - `infect.py`: logic for the run_worm.sh script
    - /docker
        - `build.sh`: Helper script for building containers
        - `docker-compose.yml`: Orchestrates all container services, networks, and startup configuration
        - `Dockerfile`: Builds the Docker image with SIMH, Alpine Linux, and BSD system setup
        - `boot.ini`: SIMH configuration file that sets up VAX boot parameters and virtual hardware
        - `install.ini`: Initial installation configuration for the 4.3BSD system
        - `setup.exp`: Expect script that automates interactive BSD system setup during Dockerfile build
        - `identity.exp`: Expect script that handles login/authentication before SIMH boots
        - `tracker.sh`: For visualization live monitoring
        - /fingerd_files: Contains fingerd worm source code (`send_file_over_socket_bsd.c`, `worm_bsd.c`, `worm_bsd.h`) that is ported into the simulation
        - `43.tap.gz`, `boot42.gz`, `miniroot.gz`: General configuration files
    - /fingerd
        - `send_file_over_socket_bsd.c`: Program for sending files over sockets written in old C. Works in tandem with the receive_file.c program that gets run on the target machine.
        - `worm_bsd.c`: The core fingerd worm logic, written in old C.
        - `worm_bsd.h`: Lots of helper functions for worm_bsd.c. Includes all of the payloads needed to complete the fingerd and movemail exploits. 
    - /sendmail
        - `manual_sendmail_attack.txt`: Includes all of the commands needed to execute a manual version of the sendmail attack. Also contains the sendmail worm source code.
    - /visualization
        - `index.html`: Defines live tracker website
        - `infected.txt`: Keeps track of infected nodes IP addresses
