#!/bin/sh

EDGE_FILE="/app/www/infected.txt"
> $EDGE_FILE

# 1. Auto-discover the specific Docker bridge handling our network
BRIDGE_IFACE=$(ip route | grep "172.20.0.0/24" | awk '{print $3}')

if [ -z "$BRIDGE_IFACE" ]; then
    echo "ERROR: Could not find Docker bridge. Defaulting to 'any'."
    BRIDGE_IFACE="any"
fi

echo "--- TRACKER ACTIVE: Monitoring $BRIDGE_IFACE for Infection Vectors ---"

# 2. Listen explicitly on the L2 bridge so we catch all VAX-to-VAX traffic
tcpdump -i "$BRIDGE_IFACE" -n -U -l "tcp and (dst port 25 or dst port 79)" 2>/dev/null | grep --line-buffered "Flags \[S\]" | while read -r line; do
    
    # Split at '>' to isolate Source and Destination
    SRC=$(echo "$line" | awk -F'>' '{print $1}' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' | tail -n 1)
    DST=$(echo "$line" | awk -F'>' '{print $2}' | grep -oE '[0-9]+\.[0-9]+\.[0-9]+\.[0-9]+' | head -n 1)

    if [ -z "$SRC" ] || [ -z "$DST" ] || [ "$SRC" = "$DST" ]; then
        continue
    fi

    EDGE="${SRC},${DST}"

    if ! grep -Fxq "$EDGE" "$EDGE_FILE"; then
        echo "$EDGE" >> "$EDGE_FILE"
        echo "[!] INFECTION VECTOR DETECTED: $SRC ---> $DST"
    fi
done