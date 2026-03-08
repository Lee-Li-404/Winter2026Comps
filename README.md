# Winter2026Comps
<img width="480" height="320" alt="worm on a keyboard" src="https://github.com/user-attachments/assets/3ea4cb00-61e2-4a5d-bab7-f4aa8af37f8a" />

Welcome to our comps project repository! This repository contains all of the code needed to launch our (mostly) faithful historical recreation of the Morris Worm in a simulated 1988 internet. Hope you find it interesting!

HOW TO USE THIS REPOSITORY:


To run the automated version of the worm setup and attack:

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run `cd /main/docker`
    - Run `./build.sh` to build the containers with the nescessary 
        simh and 4.3BSD images

2. Launch the attack
    - Run `cd /main`
        - To run the sendmail debug exploit worm, run `./run_worm.sh -sendmail`
        - To run the fingerd buffer overflow attack worm, run `./run_worm.sh -fingerd`
    - Navigate to the live visualization that should have automatically opened on your browser to watch the worm spread in real time!


To manualy run the sendmail worm:

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run `cd ./main/docker`
    - Run `./build.sh` to build the containers with the nescessary 
        simh and 4.3BSD images
    - Run `docker compose up` to start the network of containers

2. Launch the attack
    - Make sure you are in the docker folder, run `cd ./main/docker/`
    - Attach to the attacker node by running `docker attach attacker`
    - Press enter, and when promted for a login
    - In the folder `/main/sendmail`, open the `maunal_sendmail_attack.txt` file, and enter the commands in there, hitting enter and allowing the machine to respond after each one.


To manualy run the fingerd worm:

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run `cd ./main/docker`
    - Run `./build.sh` to build the containers with the nescessary 
        simh and 4.3BSD images
    - Run `docker compose up` to start the network of containers

2. Launch the attack
    - Make sure you are in the docker folder, run `cd ./main/docker/`
    - Attach to the attacker node by running `docker attach attacker`
    - Press enter, and when promted for a login
    - To get the worm files onto the attacking machine, run these exact commands in order:
```
cd /tmp
mt -f /dev/rmt12 rew
tar xvf /dev/rmt12 
cc worm_bsd.c -o worm_bsd
cc send_file_over_socket_bsd.c -o send_file_over_socket_bsd
```
    - Then, to launch the live infection tracker, run `python3 -m http.server 8000`
    - Navigate to localhost:8000/www in your browser to view the live infection tracker
    - Then, back in the simh enviorment, run `./worm_bsd` to start the worm. 


