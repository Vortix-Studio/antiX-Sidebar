# antiX Sidebar

<p align="center">
  <img src="img/logo3.png" alt="antiX Sidebar" width="360">
</p>


Una sidebar laterale per **antiX Linux** scritta in **C++ / GTK3**.
Si ancora sul lato sinistro dello schermo come un pannello EWMH, mostrando:

| Sezione | Contenuto |
|---|---|
| 📅 Calendario | Calendario mensile navigabile (GtkCalendar), nomi mesi/località via gettext |
| 🌤 Meteo | Temperatura e condizioni via **wttr.in** (no API key), località configurable |
| 📰 Notizie | Feed RSS (BBC World) con titoli cliccabili e link "altri articoli" |
| 🎵 Audio | Controlli player MPRIS via **playerctl**: play/pausa, stop, prev/next, volume, progresso brano |

## Caratteristiche principali

- **Sempre sopra**: mantiene la finestra sovrapposta ad altre (`_NET_WM_STATE_ABOVE`), con guard timer ogni 30s per riportarla in primo piano
- **Visibile su tutti i desktop**: dock sticky su workspace multipli (`_NET_WM_DESKTOP` = 0xFFFFFFFF)
- **Riserva spazio**: usa `_NET_WM_STRUT_PARTIAL` per non coprire le finestre
- **Overlay mode**: opzione per visualizzare senza riserva di spazio
- **Istanza singola**: previene duplicati tramite lock file (`flock`)
- **Resize dinamico**: trascinamento tramite grip laterale (140–560 px, iniziale 190 px) con class switching CSS (`.compact` sotto 175 px / `.wide` sopra 330 px)
- **Nessun furto di focus**: la finestra non accetta il focus del WM (`accept-focus`/`focus-on-map` disattivati) e il focus da tastiera è limitato ai soli controlli interattivi (bottoni, slider, combo, entry)
- **Gestione temi**: directory `themes/` con file CSS, nome letto dal commento in testa al file, provider CSS sostituito correttamente (senza accumulo); tema persistente in `~/.config/antix-sidebar/theme.conf`, sospensione temporanea del tema per file chooser e dialoghi di sistema
- **Layout persistente**: `~/.config/antix-sidebar/layout.conf` salva larghezza, lingua, tema, visibilità e ordine dei widget
- **Dialog Personalizza**: scelta lingua (11 lingue), tema CSS (integrato o file locale), visibilità e riordino dei widget con frecce ↑/↓
- **Ottimizzazioni**: cache atom X11, debounce su `apply_always_on_top()`, kinetic/overlay scrolling disabilitati per IceWM, polling audio ogni 3 s (riduce i fork di ~85%), richieste di rete asincrone (thread/GTask) con timeout e annullamento
- **Tema scuro navy**: stile coerente con antiX (accenti #2196F3)

## Widget

### Meteo
- Dati da **wttr.in** con formato custom (`%C|%t|%f|%h|%w|%P|%p|%u|%c|%l`), risposta HTTP in un thread di lavoro dedicato, callback via `g_idle_add`
- **Cambio località** dal pulsante 📍 (dialog modale), salvata in `~/.config/antix-sidebar/weather.ini`
- **Aggiornamento manuale** con ↻, richiesta annullabile in corsa
- Icone meteo dedicate (`src/icons`), riconoscimento condizione in italiano e inglese, con fallback sull'emoji
- Riga ↑max/↓min predisposta (da attivare quando il client fornirà i dati giornalieri)

### Notizie
- Feed RSS `feeds.bbci.co.uk/news/world/rss.xml`, massimo 3 titoli
- Fetch asincrono con GTask, limite 64 KB, timeout 3+5 s
- Titoli cliccabili (apertura nel browser predefinito), URL validati con GUri

### Audio
- Singola chiamata `playerctl metadata` con formato unificato (status|title|artist|album|volume|position|length)
- Barra di avanzamento brano sincronizzata, slider volume, polling ogni 3 s

### Calendario
- GtkCalendar con navigazione ◀/▶ mese/anno, etichetta mese tradotta via gettext
- LC_TIME impostato secondo la lingua scelta (con fallback a "C")
- Tick ogni 60 s per aggiornare automaticamente il giorno corrente

## Lingue

Italiano, English, Deutsch, Español, Français, 日本語, Nederlands, Polski, Português, Русский, Українська.
La lingua si sceglie dal dialog Personalizza; le etichette usano gettext (dominio `antix-sidebar`).

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

```text
antiX_sidebar/
├── CMakeLists.txt          # sistema di build CMake
├── style.css               # tema CSS GTK (scuro + accento blu antiX)
├── antix-sidebar.desktop   # voce del menu applicazioni XDG
├── themes/                  # temi CSS della sidebar
├── tools/
│   ├── install.sh           # installazione di sistem


## Personalizzazione

Dal pulsante ⚙ nell'header della sidebar si apre il dialog **Personalizza**
con cui è possibile:

- cambiare **lingua**
- scegliere un **tema CSS** tra quelli disponibili, oppure caricare un
  **file CSS locale** con il file chooser (il tema di sistema viene
  temporaneamente ripristinato durante la scelta)
- **mostrare/nascondere** i singoli widget (Audio, Calendario, Meteo, Notizie)
- **riordinare** i widget con le frecce ↑/↓

Le impostazioni sono persistenti in `~/.config/antix-sidebar/`
(`theme.conf` per il tema, `layout.conf` per larghezza, lingua, visibilità
e ordine dei widget, `weather.ini` per la località meteo).

## Dipendenze

```bash
sudo apt install cmake pkg-config libgtk-3-dev libcairo2-dev \
                 libcurl4-openssl-dev libxml2-dev libx11-dev

Runtime: playerctl (per il widget Audio).

Versioni minime:

    GTK 3.24+
    Cairo 1.16+
    CMake 3.16+
    IceWM 1.7+ (o WM conforme EWMH)
