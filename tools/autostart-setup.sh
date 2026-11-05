#!/bin/bash
# Aggiunge la sidebar all'autostart di IceWM/GNOME/XFCE
SCRIPT="${HOME}/.local/bin/start-sidebar.sh"
BIN="/usr/local/bin/antix-sidebar"

if [[ ! -x "$BIN" ]]; then
    echo "❌ Binario non trovato. Installa prima con sudo ./tools/install.sh"
    exit 1
fi

mkdir -p "$(dirname "$SCRIPT")"
cat > "$SCRIPT" << 'INNER_EOF'
#!/bin/bash
sleep 3  # attendi che il WM sia carico
/usr/local/bin/antix-sidebar
INNER_EOF
chmod +x "$SCRIPT"

# Autostart IceWM
if [[ -d "$HOME/.icewm" ]]; then
    echo "$SCRIPT &" >> "$HOME/.icewm/startup"
    echo "✓ Aggiunto a ~/.icewm/startup"
fi

# Autostart GNOME/XFCE (standard XDG)
DESKTOP="$HOME/.config/autostart/antix-sidebar.desktop"
if [[ -f "/usr/share/applications/antix-sidebar.desktop" ]]; then
    cp "/usr/share/applications/antix-sidebar.desktop" "$DESKTOP"
    sed -i "s|Exec=.*|Exec=$SCRIPT|" "$DESKTOP"
    echo "✓ Aggiunto a ~/.config/autostart/"
fi

echo "✓ La sidebar partirà automaticamente al login."
