# antiX Sidebar

<p align="center">
  <img src="img/logo3.png" alt="antiX Sidebar" width="360">
</p>

A sidebar for **antiX Linux**, written in **C++ / GTK3**. It docks on the left side of the screen as an EWMH panel and displays a calendar, weather, news, and audio controls.

| Section | Content |
|---|---|
| 📅 Calendar | Navigable monthly calendar (`GtkCalendar`), with month names and localization via gettext |
| 🌤 Weather | Temperature and conditions from **wttr.in** (no API key required); configurable location |
| 📰 News | BBC World RSS feed with clickable headlines and a link to more articles |
| 🎵 Audio | MPRIS controls via **playerctl**: play/pause, stop, previous/next track, volume, and track progress |

## Main features

- **Always on top:** keeps the window above others (`_NET_WM_STATE_ABOVE`), with a periodic check every 30 seconds.
- **Visible on all desktops:** sticky dock across workspaces (`_NET_WM_DESKTOP` = `0xFFFFFFFF`).
- **Reserves screen space:** uses `_NET_WM_STRUT_PARTIAL` to prevent windows from being covered.
- **Overlay mode:** displays the sidebar without reserving screen space.
- **Single instance:** prevents duplicate instances using a lock file (`flock`).
- **Dynamic resizing:** resize by dragging the side grip, from 140 to 560 px; the initial width is 190 px. Applies different CSS classes (`.compact` below 175 px and `.wide` above 330 px).
- **Does not steal focus:** the window does not accept focus from the window manager (`accept-focus` and `focus-on-map` are disabled); keyboard focus is limited to interactive controls.
- **Theme management:** supports CSS files in the `themes/` directory; the theme name is read from the file’s opening comment. The CSS provider is replaced without accumulating providers. The selected theme is saved in `~/.config/antix-sidebar/theme.conf`; the theme is temporarily suspended while file choosers and system dialogs are open.
- **Persistent layout:** `~/.config/antix-sidebar/layout.conf` saves the width, language, theme, widget visibility, and widget order.
- **Customize dialog:** lets users choose from 11 languages, select a CSS theme, toggle widget visibility, and reorder widgets using the ↑/↓ arrows.
- **Optimizations:** X11 atom caching, debounce in `apply_always_on_top()`, kinetic/overlay scrolling disabled for IceWM, audio polling every 3 seconds, and asynchronous network requests with timeouts and cancellation.
- **Dark navy theme:** a style that matches antiX, with blue accents (`#2196F3`).

## Widgets

### Weather

- Fetches data from **wttr.in** using a custom format: `%C|%t|%f|%h|%w|%P|%p|%u|%c|%l`.
- The HTTP response is processed in a dedicated worker thread, with callbacks via `g_idle_add`.
- Set the location using the 📍 button. It is saved in `~/.config/antix-sidebar/weather.ini`.
- Refresh manually using ↻; an in-progress request can be cancelled.
- Dedicated weather icons are stored in `src/icons`. Conditions are recognized in English and Italian, with emoji as a fallback.
- A ↑max/↓min row is prepared for when the client provides daily data.

### News

- RSS feed: `feeds.bbci.co.uk/news/world/rss.xml`, with a maximum of 3 headlines.
- Asynchronous fetching via GTask, a 64 KB limit, and a 3 + 5 second timeout.
- Headlines are clickable and open in the default browser; URLs are validated with GUri.

### Audio

- A single `playerctl metadata` call retrieves data in the format `status|title|artist|album|volume|position|length`.
- Includes a track progress bar, a volume slider, and polling every 3 seconds.

### Calendar

- `GtkCalendar` with ◀/▶ navigation between months and years.
- The month label is translated using gettext.
- `LC_TIME` is set according to the selected language, with a fallback to `C`.
- An update every 60 seconds automatically refreshes the current day.

## Languages

Italian, English, German, Spanish, French, Japanese, Dutch, Polish, Portuguese, Russian, and Ukrainian.

Choose the language from the **Customize** dialog. Labels use gettext with the `antix-sidebar` domain.

## Themes

The themes directory is resolved in the following order:

1. `ANTIX_SIDEBAR_THEMES_DIR` environment variable
2. `<exe_dir>/themes`
3. `<exe_dir>/../share/antix-sidebar/themes`
4. `/usr/local/share/antix-sidebar/themes`
5. `/usr/share/antix-sidebar/themes`

Each `.css` file is treated as a theme. The name shown in the menu is read from a CSS comment at the beginning of the file, for example `/* Theme Name */`.

The preferred default themes are `dark-navy` and `violet-night`. The selected theme is saved automatically.

### Environment variables

| Variable | Purpose |
|---|---|
| `ANTIX_SIDEBAR_THEMES_DIR` | Alternative themes directory |
| `ANTIX_SIDEBAR_CSS` | CSS file to apply at startup, overriding the selected theme |
| `ANTIX_SIDEBAR_LOCALEDIR` | Alternative translations directory |

## Project structure

```text
antiX_sidebar/
├── CMakeLists.txt              # CMake build configuration
├── style.css                   # GTK CSS theme
├── antix-sidebar.desktop       # XDG application menu entry
├── themes/                      # Sidebar CSS themes
├── tools/
│   ├── install.sh              # System installation
│   ├── uninstall.sh            # Complete removal
│   ├── reset-workarea.sh       # Reset the work area if blocked by a strut
│   ├── start-sidebar.sh        # Convenient startup with overlay option
│   └── autostart-setup.sh      # IceWM autostart configuration
└── src/
    ├── main.cpp                # Entry point and flock single-instance lock
    ├── i18n.h                  # gettext macros
    ├── sidebar.h / .cpp        # Main window, EWMH hints, and Customize dialog
    ├── theme_manager.h / .cpp  # Theme, directory, and persistence management
    └── widgets/
        ├── calendar_widget.h / .cpp
        ├── weather_widget.h / .cpp
        ├── open_meteo_client.h / .cpp
        ├── news_widget.h / .cpp
        ├── audio_control_widget.h / .cpp
        └── icons/              # Weather icons (EN/IT names)
```

## Customization

Click the ⚙ button in the sidebar header to open the **Customize** dialog. From there, you can:

- change the language;
- choose an available CSS theme or load a local CSS file using the file chooser;
- show or hide the Audio, Calendar, Weather, and News widgets;
- reorder widgets using the ↑/↓ arrows.

Settings are saved in `~/.config/antix-sidebar/`:

- `theme.conf`: selected theme;
- `layout.conf`: width, language, widget visibility, and widget order;
- `weather.ini`: weather location.

## Dependencies

Install the build dependencies with:

```bash
sudo apt install cmake pkg-config libgtk-3-dev libcairo2-dev \
                 libcurl4-openssl-dev libxml2-dev libx11-dev
```

The Audio widget also requires `playerctl`:

```bash
sudo apt install playerctl
```

### Minimum versions

- GTK 3.24+
- Cairo 1.16+
- CMake 3.16+
- IceWM 1.7+ (or an EWMH-compliant window manager)
