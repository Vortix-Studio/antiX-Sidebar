#!/bin/bash
# Add the sidebar to IceWM/GNOME/XFCE autostart
SCRIPT="${HOME}/.local/bin/start-sidebar.sh"
BIN="/usr/local/bin/antix-sidebar"

if [[ ! -x "$BIN" ]]; then
    echo "❌ Binary not found. Install it first with sudo ./tools/install.sh"
    exit 1
fi

mkdir -p "$(dirname "$SCRIPT")"
cat > "$SCRIPT" << 'INNER_EOF'
#!/bin/bash
sleep 3  # Wait for the window manager to load
/usr/local/bin/antix-sidebar
INNER_EOF
chmod +x "$SCRIPT"

# IceWM autostart
if [[ -d "$HOME/.icewm" ]]; then
    echo "$SCRIPT &" >> "$HOME/.icewm/startup"
    echo "✓ Added to ~/.icewm/startup"
fi

# GNOME/XFCE autostart (standard XDG)
DESKTOP="$HOME/.config/autostart/antix-sidebar.desktop"
if [[ -f "/usr/share/applications/antix-sidebar.desktop" ]]; then
    mkdir -p "$(dirname "$DESKTOP")"
    cp "/usr/share/applications/antix-sidebar.desktop" "$DESKTOP"
    sed -i "s|Exec=.*|Exec=$SCRIPT|" "$DESKTOP"
    echo "✓ Added to ~/.config/autostart/"
fi

echo "✓ The sidebar will start automatically at login."
