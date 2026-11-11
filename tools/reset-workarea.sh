#!/bin/bash
# Remove leftover struts and make IceWM recalculate the work area
pkill -x antix-sidebar 2>/dev/null
sleep 0.6
icewm --restart
sleep 0.5

if xprop -root _NET_WORKAREA 2>/dev/null; then
    echo "✓ Work area reset. IceWM has been restarted."
else
    echo "! Could not read _NET_WORKAREA. Restart IceWM manually."
fi
