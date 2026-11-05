# antiX Sidebar

Una sidebar laterale per **antiX Linux** scritta in **C++ / GTK3**.  
Si ancorra sul lato sinistro dello schermo come un pannello EWMH, mostrando:

| Sezione | Contenuto |
|---|---|
| 📅 Calendario | Calendario mensile navigabile |
| 🌤 Meteo | Temperatura e condizioni (Open-Meteo, no API key) |
| 📰 Notizie | Feed RSS da fonti selezionate |
| 🎵 Audio | Controlli volumetrici PulseAudio |

## Caratteristiche principali

- **Sempre sopra**: mantiene la finestra sovrapposta ad altre (`_NET_WM_STATE_ABOVE`)
- **Visibile su tutti i desktop**: dock stick su workspace multipli
- **Riserva spazio**: usa `_NET_WM_STRUT_PARTIAL` per non coprire le finestre
- **Overlay mode**: opzione per visualizzare senza riserva spazio
- **Istanza singola**: previene duplicati tramite lock file
- **Resize dinamico**: ridimensionamento con class switching CSS (.compact/.wide)
- **Temma scuro navy**: stile coerente con antiX (accenti #2196F3)

---

## Struttura del progetto

antiX_sidebar/ ├── CMakeLists.txt # sistema di build CMake ├── style.css # tema CSS GTK (scuro + accento blu antiX) ├── antix-sidebar.desktop # voce del menu applicazioni XDG ├── tools/ │ ├── install.sh # installazione di sistema │ ├── uninstall.sh # rimozione completa │ ├── reset-workarea.sh # reset workarea se bloccata da strut │ ├── start-sidebar.sh # avvio comodo con opzione overlay │ └── autostart-setup.sh # configurazione autostart IceWM └── src/ ├── main.cpp # entry point + flock istanza singola ├── sidebar.h / .cpp # finestra principale + hint EWMH └── widgets/ ├── calendar_widget.h / .cpp # calendario mensile ├── weather_widget.h / .cpp # meteo asincrono ├── news_widget.h / .cpp # feed notizie RSS ├── audio_control_widget.h / .cpp # volumi PulseAudio └── meteoblue_scraper.cpp # scraping meteo supplementare

---

## Dipendenze

```bash
sudo apt install cmake pkg-config libgtk-3-dev libcairo2-dev \
                 libcurl4-openssl-dev libxml2-dev libx11-dev

Versioni minime:

    GTK 3.24+
    Cairo 1.16+
    CMake 3.16+
    IceWM 1.7+ (o WM conforme EWMH)
