# Winter2026Comps
<img width="480" height="320" alt="worm on a keyboard" src="https://github.com/user-attachments/assets/3ea4cb00-61e2-4a5d-bab7-f4aa8af37f8a" />

Welcome to our comps project repository! This repository contains all of the code needed to launch our (mostly) faithful historical recreation of the Morris Worm in a simulated 1988 internet. Hope you find it interesting!

HOW TO USE THIS REPOSITORY:


To run the automated version of the worm setup and attack (easier but less interesting):

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run 'cd /main/docker'
    - Run 'docker compose up --build' to build the containers with the nescessary 
        simh and 4.3BSD image

2. Start the live visualization
    - Lee plz put the instructions on how to start the visualization in here

3. Launch the attack
    - Run 'cd /main'
        - To run the sendmail debug exploit worm, run './run_worm.sh -sendmail'
        - To run the fingerd buffer overflow attack worm, run './run_worm.sh -fingerd'
    - Navigate to the live visualization on your browser to watch the worm spread in real time!


To manualy run the sendmail worm:

1. Set up the necessary docker containers
    - Open this repository in your terminal, and run 'cd /main/docker'
    - Run 'docker compose up --build' to build the containers with the nescessary 
        simh and 4.3BSD image

2. Start the live visualization
    - Lee plz put the instructions on how to start the visualization in here

3. Launch the attack
    - Make sure you are in the docker folder, run 'cd /main/docker/'
    - Attach to the target-prime node by running 'docker attach target-prime'
    - Press enter, and when promted for a login
    - In the folder /main/sendmail, open the 
