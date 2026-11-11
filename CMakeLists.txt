cmake_minimum_required(VERSION 3.16)
project(antiX_Sidebar VERSION 1.0.0 LANGUAGES CXX)

include(GNUInstallDirs)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DGLIB_LOG_DOMAIN=\"sidebar\"")

find_package(PkgConfig REQUIRED)
find_package(Threads REQUIRED)

pkg_check_modules(GTK3 REQUIRED IMPORTED_TARGET gtk+-3.0)
pkg_check_modules(CAIRO REQUIRED IMPORTED_TARGET cairo)
pkg_check_modules(X11 REQUIRED IMPORTED_TARGET x11)
pkg_check_modules(GDKX11 REQUIRED IMPORTED_TARGET gdk-x11-3.0)
pkg_check_modules(CURL REQUIRED IMPORTED_TARGET libcurl)
pkg_check_modules(LIBXML2 REQUIRED IMPORTED_TARGET libxml-2.0)

set(SOURCES
    src/main.cpp
    src/sidebar.cpp
    src/theme_manager.cpp
    src/widgets/calendar_widget.cpp
    src/widgets/weather_widget.cpp
    src/widgets/open_meteo_client.cpp
    src/widgets/news_widget.cpp
    src/widgets/audio_control_widget.cpp
)

add_executable(antix-sidebar ${SOURCES})

target_include_directories(antix-sidebar PRIVATE src src/widgets)

target_link_libraries(antix-sidebar PRIVATE
    PkgConfig::GTK3
    PkgConfig::CAIRO
    PkgConfig::X11
    PkgConfig::GDKX11
    PkgConfig::CURL
    PkgConfig::LIBXML2
    Threads::Threads
)

target_compile_options(antix-sidebar PRIVATE 
    -Wall -Wextra -Wpedantic -O2
    -Wno-unused-parameter
)

# ========================================================================
# Percorso tema CSS default (nella sottodirectory themes/)
# ========================================================================
target_compile_definitions(antix-sidebar PRIVATE
    DEFAULT_CSS_PATH="${CMAKE_INSTALL_FULL_DATADIR}/antix-sidebar/themes/dark-navy.css"
)

# ========================================================================
# Localizzazione (Gettext)
# ========================================================================
set(GETTEXT_DOMAIN "antix-sidebar")

find_program(MSGFMT_EXECUTABLE msgfmt REQUIRED)
find_program(XGETTEXT_EXECUTABLE xgettext REQUIRED)

# Lingue disponibili
set(LANGUAGES "it" "fr" "de" "es" "pt" "nl" "pl" "ru")

# Estrazione delle stringhe traducibili dal codice sorgente
add_custom_target(update-pot
    COMMAND ${XGETTEXT_EXECUTABLE} --from-code=UTF-8
        --keyword=_ --keyword=N_ --keyword=dgettext:1,2
        --package-name=${GETTEXT_DOMAIN}
        --package-version=1.0.0
        --msgid-bugs-address="diego@example.com"
        --output=${CMAKE_SOURCE_DIR}/po/${GETTEXT_DOMAIN}.pot
        ${SOURCES}
    COMMENT "Genera il file pot delle stringhe traducibili"
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
)

# Genera file MO da PO per ogni lingua
set(MO_OUTPUTS "")

foreach(LANG IN LISTS LANGUAGES)
    set(PO_FILE "${CMAKE_SOURCE_DIR}/po/${LANG}.po")
    set(MO_DIR  "${CMAKE_BINARY_DIR}/locale/${LANG}/LC_MESSAGES")
    set(MO_FILE "${MO_DIR}/${GETTEXT_DOMAIN}.mo")

    if(EXISTS ${PO_FILE})
        add_custom_command(OUTPUT ${MO_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${MO_DIR}"
            COMMAND ${MSGFMT_EXECUTABLE} -o ${MO_FILE} ${PO_FILE}
            DEPENDS ${PO_FILE}
            COMMENT "Compila ${LANG}.po -> ${LANG}.mo"
        )
        list(APPEND MO_OUTPUTS ${MO_FILE})

        install(FILES ${MO_FILE}
            DESTINATION "${CMAKE_INSTALL_LOCALEDIR}/${LANG}/LC_MESSAGES/"
            RENAME "${GETTEXT_DOMAIN}.mo"
        )
    else()
        message(WARNING "File PO non trovato, saltato: ${PO_FILE}")
    endif()
endforeach()

# Target locales ALL dipende dinamicamente dai file MO generati
add_custom_target(locales ALL DEPENDS ${MO_OUTPUTS})

# Dipendenza del target principale dalle locali
add_dependencies(antix-sidebar locales)

# ========================================================================
# Copia file CSS e icone nella cartella di build
# ========================================================================
set(THEMES_CSS
    themes/dark-navy.css
    themes/forest-green.css
    themes/orange_vortix.css
    themes/violet-night.css
)

# Crea directory themes nella build
add_custom_command(TARGET antix-sidebar POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory
        "$<TARGET_FILE_DIR:antix-sidebar>/themes"
    COMMENT "Prepara directory temi in build"
)

# Copia ogni CSS esistente in themes/
foreach(CSS_FILE IN LISTS THEMES_CSS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CSS_FILE}")
        get_filename_component(CSS_NAME ${CSS_FILE} NAME)
        add_custom_command(TARGET antix-sidebar POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_CURRENT_SOURCE_DIR}/${CSS_FILE}"
                "$<TARGET_FILE_DIR:antix-sidebar>/themes/${CSS_NAME}"
            COMMENT "Copia ${CSS_NAME}"
        )
    else()
        message(WARNING "CSS non trovato, saltato: ${CMAKE_CURRENT_SOURCE_DIR}/${CSS_FILE}")
    endif()
endforeach()

# Copia icone (solo se la directory esiste)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/icons")
    add_custom_command(TARGET antix-sidebar POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            "${CMAKE_CURRENT_SOURCE_DIR}/src/icons"
            "$<TARGET_FILE_DIR:antix-sidebar>/icons"
        COMMENT "Copia le icone nella cartella di build"
    )
endif()

# ========================================================================
# Installazione
# ========================================================================
install(TARGETS antix-sidebar
    RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}"
)

# Installa CSS solo quelli che esistono
set(CSS_TO_INSTALL "")
foreach(CSS_FILE IN LISTS THEMES_CSS)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${CSS_FILE}")
        list(APPEND CSS_TO_INSTALL ${CSS_FILE})
    endif()
endforeach()

if(CSS_TO_INSTALL)
    install(FILES ${CSS_TO_INSTALL}
        DESTINATION "${CMAKE_INSTALL_DATADIR}/antix-sidebar/themes"
    )
endif()

# Icone (protette)
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/icons")
    install(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/src/icons/"
        DESTINATION "${CMAKE_INSTALL_DATADIR}/antix-sidebar/icons"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icon.svg")
    install(FILES icon.svg
        DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/antix-sidebar.desktop")
    install(FILES antix-sidebar.desktop
        DESTINATION "${CMAKE_INSTALL_DATADIR}/applications"
    )
endif()

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/tools")
    install(DIRECTORY tools/
        DESTINATION "${CMAKE_INSTALL_DATADIR}/antix-sidebar/tools"
        FILES_MATCHING PATTERN "*.sh"
    )
endif()

install(FILES LICENSE README.md
    DESTINATION "${CMAKE_INSTALL_DOCDIR}"
    OPTIONAL
)

message(STATUS "")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  antiX Sidebar Build Configuration")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "  GTK3 Version:  ${GTK3_VERSION}")
message(STATUS "  Install Prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "═══════════════════════════════════════════")
message(STATUS "")
