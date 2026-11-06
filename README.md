# antiX Sidebar

Una sidebar laterale per **antiX Linux** scritta in **C++ / GTK3**.
Si ancora sul lato sinistro dello schermo come un pannello EWMH, mostrando:

| Sezione | Contenuto |
|---|---|
| 📅 Calendario | Calendario mensile navigabile |
| 🌤 Meteo | Temperatura e condizioni (Open-Meteo, no API key) |
| 📰 Notizie | Feed RSS da fonti selezionate |
| 🎵 Audio | Controlli volumetrici PulseAudio |

## Caratteristiche principali

- **Sempre sopra**: mantiene la finestra sovrapposta ad altre (`_NET_WM_STATE_ABOVE`), con guard timer ogni 30s per riportarla in primo piano
- **Visibile su tutti i desktop**: dock sticky su workspace multipli (`_NET_WM_DESKTOP` = 0xFFFFFFFF)
- **Riserva spazio**: usa `_NET_WM_STRUT_PARTIAL` per non coprire le finestre
- **Overlay mode**: opzione per visualizzare senza riserva di spazio
- **Istanza singola**: previene duplicati tramite lock file (`flock`)
- **Resize dinamico**: trascinamento tramite grip laterale (140–560 px) con class switching CSS (`.compact` / `.wide`)
- **Gestione temi**: directory `themes/` con file CSS, nome letto dal commento in testa al file, tema persistente in `~/.config/antix-sidebar/theme.conf`
- **Dialog Personalizza**: scelta lingua (IT/EN), tema CSS (integrato o file locale), visibilità e riordino dei widget
- **Ottimizzazioni**: cache atom X11, debounce su `apply_always_on_top()`, kinetic/overlay scrolling disabilitati per IceWM
- **Tema scuro navy**: stile coerente con antiX (accenti #2196F3)

## Temi

La directory temi viene risolta in questo ordine:

1. Variabile d'ambiente `ANTIX_SIDEBAR_THEMES_DIR`
2. `<exe_dir>/themes`
3. `<exe_dir>/../share/antix-sidebar/themes`
4. `/usr/local/share/antix-sidebar/themes`
5. `/usr/share/antix-sidebar/themes`

Ogni file `.css` è un tema; il nome mostrato nel menu è preso dal commento
CSS in cima al file (`/* Nome Tema */`). Temi di default preferiti:
`dark-navy` o `violet-night`. Il tema scelto viene salvato automaticamente.

Variabili d'ambiente supportate:

| Variabile | Funzione |
|---|---|
| `ANTIX_SIDEBAR_THEMES_DIR` | Directory alternativa dei temi |
| `ANTIX_SIDEBAR_CSS` | File CSS da applicare all'avvio (override tema) |
| `ANTIX_SIDEBAR_LOCALEDIR` | Directory alternativa delle traduzioni |

---

## Struttura del progetto

antiX_sidebar/ ├── CMakeLists.txt # sistema di build CMake ├── style.css # tema CSS GTK (scuro + accento blu antiX) ├── antix-sidebar.desktop # voce del menu applicazioni XDG ├── themes/ # temi CSS della sidebar ├── tools/ │ ├── install.sh # installazione di sistema │ ├── uninstall.sh # rimozione completa │ ├── reset-workarea.sh # reset workarea se bloccata da strut │ ├── start-sidebar.sh # avvio comodo con opzione overlay │ └── autostart-setup.sh # configurazione autostart IceWM └── src/ ├── main.cpp # entry point + flock istanza singola ├── i18n.h # macro gettext ├── sidebar.h / .cpp # finestra principale + hint EWMH + dialog personalizza ├── theme_manager.h/.cpp # gestione temi, directory, persistenza └── widgets/ ├── calendar_widget.h / .cpp # calendario mensile ├── weather_widget.h / .cpp # meteo asincrono ├── news_widget.h / .cpp # feed notizie RSS ├── audio_control_widget.h / .cpp # volumi PulseAudio └── meteoblue_scraper.cpp # scraping meteo supplementare

## Personalizzazione

Dal pulsante ⚙ nell'header della sidebar si apre il dialog **Personalizza**
con cui è possibile:

- cambiare **lingua** (It / En / Es / pt / ru / de / ja)
- scegliere un **tema CSS** tra quelli disponibili, oppure caricare un
  **file CSS locale** con il file chooser
- **mostrare/nascondere** i singoli widget (Audio, Calendario, Meteo, Notizie)
- **riordinare** i widget con le frecce ↑/↓

Le impostazioni di tema sono persistenti in `~/.config/antix-sidebar/theme.conf`.

## Dipendenze

```bash
sudo apt install cmake pkg-config libgtk-3-dev libcairo2-dev \
                 libcurl4-openssl-dev libxml2-dev libx11-dev

Versioni minime:

    GTK 3.24+
    Cairo 1.16+
    CMake 3.16+
    IceWM 1.7+ (o WM conforme EWMH)
