#!/bin/sh

TARGET="172.20.0.10"
INFECTED_FILE="/app/www/infected.txt"
> $INFECTED_FILE

echo "--- TRACKER ACTIVE: Silent Monitoring for $TARGET ---"

# We use -n and -q to stay quiet, and filter for ICMP pings only
tcpdump -i any -n -U -l "icmp and dst $TARGET" 2>/dev/null | while read -r line; do
    
    # Use grep to extract ONLY the IP address pattern (e.g., 172.20.0.11)
    # This ignores 'P', 'Out', and interface names
    SRC=$(echo "$line" | grep -oE '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)

    # If we found an IP and it's not the target
    if [ -z "$SRC" ] || [ "$SRC" = "$TARGET" ]; then
        continue
    fi

    # Check if this IP is already in our list
    if ! grep -Fxq "$SRC" "$INFECTED_FILE"; then
        echo "$SRC" >> "$INFECTED_FILE"
        
        # Output only the clean alert and the current list
        echo ""
        echo "[!] NEW INFECTION: $SRC"
        echo "ALL UNIQUE IPS: $(tr '\n' ' ' < $INFECTED_FILE)"
        echo "------------------------------------------"
    fi
done