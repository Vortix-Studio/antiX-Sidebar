#!/bin/bash
# Start the sidebar, optionally in overlay mode
APP="${APP:-$HOME/Antigravity/antiX_sidebar/build/antix-sidebar}"

case "$1" in
    --overlay|--no-strut) export ANTIX_SIDEBAR_NO_STRUT=1 ;;
esac

pkill -x antix-sidebar 2>/dev/null
sleep 0.4
exec "$APP"
