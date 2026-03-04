#!/bin/bash

# --- CONFIGURATION ---
NODES=("target-prime" "node-1" "node-2" "node-3" "node-4" "node-5" "node-6" "node-7" "node-8" "node-9")
TARGET_PHRASE="4.3 BSD UNIX (simh) (console)"
TARGET_COUNT=2
RUN_INFECT=false
URL="http://localhost:8000/visualization/"

# Parse flags
for arg in "$@"; do [ "$arg" == "-sendmail", "$arg" == "-fingerd" ] && RUN_INFECT=true; done

# --- 1. CLEANUP & START ---
# Stop old containers and start the fresh 10-node cluster
docker-compose down --remove-orphans
docker-compose up -d

# --- 2. STABILIZATION (The "Wait for 2nd Banner" Loop) ---
# We wait for the specific BSD banner to appear twice to account for the SIMH reboot.
echo "Waiting for 4.3BSD nodes to reach final boot state..."
for NODE in "${NODES[@]}"; do
    while true; do
        # Count occurrences of the specific system banner
        CURRENT_COUNT=$(docker logs "$NODE" 2>&1 | grep -c "$TARGET_PHRASE")
        
        if [ "$CURRENT_COUNT" -ge "$TARGET_COUNT" ]; then
            echo -e "\r[+] $NODE: STABLE ($CURRENT_COUNT/2 banners detected)          "
            break
        else
            echo -ne "\r[-] Waiting for $NODE... ($CURRENT_COUNT/$TARGET_COUNT)"
            sleep 2
        fi
    done
done

# --- 3. PORT RESET ---
# Clear Port 8000 (Cross-platform: Mac/Linux/Windows Git Bash)
if [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then
    PID_ON_8000=$(netstat -ano | findstr :8000 | awk '{print $5}' | head -n 1)
    [ -n "$PID_ON_8000" ] && taskkill //F //PID $PID_ON_8000 2>/dev/null
else
    PID_ON_8000=$(lsof -t -i:8000)
    [ -n "$PID_ON_8000" ] && kill -9 $PID_ON_8000
fi
sleep 1

# --- 4. WEB MONITORING & BROWSER ---
# Start background server and launch browser
PY_CMD=$(command -v python3 || command -v python)
nohup $PY_CMD -m http.server 8000 > /dev/null 2>&1 &

if [[ "$OSTYPE" == "darwin"* ]]; then open "$URL"
elif [[ "$OSTYPE" == "msys" || "$OSTYPE" == "cygwin" ]]; then start "$URL"
else xdg-open "$URL"; fi

# --- 5. OPTIONAL ACTION ---
# Execute infection simulation if flag is present
if [ "$RUN_INFECT" = true ]; then
    echo "--- Executing Infection Simulation ---"
    $PY_CMD infect.py
fi

echo "-------------------------------------------------------"
echo "Deployment Complete"