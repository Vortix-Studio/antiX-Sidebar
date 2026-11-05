#!/bin/bash
# Rimuove strut residui e fa ricalcolare la workarea a IceWM
pkill -x antix-sidebar 2>/dev/null
sleep 0.6
icewm --restart
sleep 0.5

if xprop -root _NET_WORKAREA 2>/dev/null; then
    echo "✓ Workarea reset. IceWM è stato riavviato."
else
    echo "! Impossibile leggere _NET_WORKAREA. Riavvia IceWM manualmente."
fi
