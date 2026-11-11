# antiX Sidebar

<p align="center">
  <img src="img/logo3.png" alt="antiX Sidebar" width="360">
</p>

Una sidebar laterale per **antiX Linux**, scritta in **C++ / GTK3**. Si ancora sul lato sinistro dello schermo come un pannello EWMH e mostra calendario, meteo, notizie e controlli audio.

| Sezione | Contenuto |
|---|---|
| 📅 Calendario | Calendario mensile navigabile (`GtkCalendar`), nomi dei mesi e localizzazione tramite gettext |
| 🌤 Meteo | Temperatura e condizioni da **wttr.in** (senza API key); località configurabile |
| 📰 Notizie | Feed RSS BBC World con titoli cliccabili e link ad altri articoli |
| 🎵 Audio | Controlli MPRIS tramite **playerctl**: play/pausa, stop, traccia precedente/successiva, volume e progresso del brano |

## Caratteristiche principali

- **Sempre sopra:** mantiene la finestra sovrapposta alle altre (`_NET_WM_STATE_ABOVE`), con un controllo periodico ogni 30 secondi.
- **Visibile su tutti i desktop:** dock sticky su più workspace (`_NET_WM_DESKTOP` = `0xFFFFFFFF`).
- **Riserva spazio:** usa `_NET_WM_STRUT_PARTIAL` per evitare di coprire le finestre.
- **Modalità overlay:** permette di visualizzare la sidebar senza riservare spazio.
- **Istanza singola:** previene le istanze duplicate tramite un lock file (`flock`).
- **Ridimensionamento dinamico:** trascinamento tramite grip laterale, da 140 a 560 px; larghezza iniziale di 190 px. Applica classi CSS diverse (`.compact` sotto 175 px e `.wide` sopra 330 px).
- **Nessun furto del focus:** la finestra non accetta il focus del window manager (`accept-focus` e `focus-on-map` disattivati); il focus da tastiera è limitato ai controlli interattivi.
- **Gestione dei temi:** supporta file CSS nella directory `themes/`; il nome del tema viene letto dal commento iniziale del file. Il provider CSS viene sostituito senza accumularsi. Il tema scelto viene salvato in `~/.config/antix-sidebar/theme.conf`; durante l’apertura di file chooser e dialoghi di sistema il tema viene sospeso temporaneamente.
- **Layout persistente:** `~/.config/antix-sidebar/layout.conf` salva larghezza, lingua, tema, visibilità e ordine dei widget.
- **Dialog Personalizza:** permette di scegliere la lingua (11 lingue), il tema CSS, la visibilità e l’ordine dei widget tramite frecce ↑/↓.
- **Ottimizzazioni:** cache degli atom X11, debounce in `apply_always_on_top()`, scrolling kinetic/overlay disabilitato per IceWM, polling audio ogni 3 secondi e richieste di rete asincrone con timeout e annullamento.
- **Tema scuro navy:** stile coerente con antiX, con accenti blu (`#2196F3`).

## Widget

### Meteo

- Dati da **wttr.in** con formato personalizzato: `%C|%t|%f|%h|%w|%P|%p|%u|%c|%l`.
- La risposta HTTP viene elaborata in un thread di lavoro dedicato, con callback tramite `g_idle_add`.
- La località si modifica dal pulsante 📍 e viene salvata in `~/.config/antix-sidebar/weather.ini`.
- Aggiornamento manuale tramite ↻; la richiesta in corso può essere annullata.
- Icone meteo dedicate in `src/icons`, con riconoscimento delle condizioni in italiano e inglese e fallback sull’emoji.
- Riga ↑max/↓min predisposta per quando il client fornirà i dati giornalieri.

### Notizie

- Feed RSS: `feeds.bbci.co.uk/news/world/rss.xml`, con un massimo di 3 titoli.
- Recupero asincrono tramite GTask, limite di 64 KB e timeout di 3 + 5 secondi.
- I titoli sono cliccabili e si aprono nel browser predefinito; gli URL vengono validati con GUri.

### Audio

- Una singola chiamata a `playerctl metadata` recupera i dati nel formato `status|title|artist|album|volume|position|length`.
- Include barra di avanzamento del brano, slider del volume e polling ogni 3 secondi.

### Calendario

- `GtkCalendar` con navigazione ◀/▶ tra mesi e anni.
- Etichetta del mese tradotta tramite gettext.
- `LC_TIME` viene impostato in base alla lingua scelta, con fallback a `C`.
- Un aggiornamento ogni 60 secondi aggiorna automaticamente il giorno corrente.

## Lingue

Italiano, English, Deutsch, Español, Français, 日本語, Nederlands, Polski, Português, Русский e Українська.

La lingua si sceglie dal dialog **Personalizza**. Le etichette usano gettext con il dominio `antix-sidebar`.

## Temi

La directory dei temi viene individuata nel seguente ordine:

1. Variabile d’ambiente `ANTIX_SIDEBAR_THEMES_DIR`
2. `<exe_dir>/themes`
3. `<exe_dir>/../share/antix-sidebar/themes`
4. `/usr/local/share/antix-sidebar/themes`
5. `/usr/share/antix-sidebar/themes`

Ogni file `.css` è considerato un tema. Il nome mostrato nel menu viene letto dal commento CSS all’inizio del file, per esempio `/* Nome Tema */`.

I temi predefiniti preferiti sono `dark-navy` e `violet-night`. Il tema selezionato viene salvato automaticamente.

### Variabili d’ambiente

| Variabile | Funzione |
|---|---|
| `ANTIX_SIDEBAR_THEMES_DIR` | Directory alternativa dei temi |
| `ANTIX_SIDEBAR_CSS` | File CSS da applicare all’avvio, in override al tema |
| `ANTIX_SIDEBAR_LOCALEDIR` | Directory alternativa delle traduzioni |

## Struttura del progetto

```text
antiX\_sidebar/
├── CMakeLists.txt              # sistema di build CMake
├── style.css                   # tema CSS GTK
├── antix-sidebar.desktop       # voce del menu applicazioni XDG
├── themes/                      # temi CSS della sidebar
├── tools/
│   ├── install.sh              # installazione di sistema
│   ├── uninstall.sh            # rimozione completa
│   ├── reset-workarea.sh       # reset workarea se bloccata da strut
│   ├── start-sidebar.sh        # avvio comodo con opzione overlay
│   └── autostart-setup.sh      # configurazione autostart IceWM
└── src/
    ├── main.cpp                # entry point e flock per istanza singola
    ├── i18n.h                  # macro gettext
    ├── sidebar.h / .cpp        # finestra principale, hint EWMH e dialog Personalizza
    ├── theme\_manager.h / .cpp  # gestione temi, directory e persistenza
    └── widgets/
        ├── calendar\_widget.h / .cpp
        ├── weather\_widget.h / .cpp
        ├── open\_meteo\_client.h / .cpp
        ├── news\_widget.h / .cpp
        ├── audio\_control\_widget.h / .cpp
        └── icons/              # icone meteo (nomi EN/IT)
