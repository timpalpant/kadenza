cmake_minimum_required(VERSION 3.28)

project(kadenza VERSION 0.1.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(QT_MIN_VERSION 6.8.0)
set(KF_MIN_VERSION 6.0.0)

find_package(Qt6 ${QT_MIN_VERSION} REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2 Widgets Network DBus Sql Test)
find_package(KF6CoreAddons ${KF_MIN_VERSION} REQUIRED)
find_package(KF6I18n ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Config ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Kirigami ${KF_MIN_VERSION} REQUIRED)
find_package(KF6Notifications ${KF_MIN_VERSION} REQUIRED)
find_package(KF6GlobalAccel ${KF_MIN_VERSION} REQUIRED)

include(GNUInstallDirs)
include(CTest)

qt_standard_project_setup(REQUIRES 6.8)
qt_policy(SET QTP0004 NEW)

configure_file(src/version.h.in ${CMAKE_CURRENT_BINARY_DIR}/version.h @ONLY)

qt_add_executable(kadenza
    src/main.cpp
    src/appcontroller.cpp src/appcontroller.h
    src/api/apiclient.cpp src/api/apiclient.h
    src/data/database.cpp src/data/database.h
    src/data/librarycache.cpp src/data/librarycache.h
    src/media/mediaitem.cpp src/media/mediaitem.h
    src/media/mediamodel.cpp src/media/mediamodel.h
    src/player/playercontroller.cpp src/player/playercontroller.h
    src/player/mprisplayer.cpp src/player/mprisplayer.h
)

target_include_directories(kadenza PRIVATE
    src src/api src/data src/media src/player ${CMAKE_CURRENT_BINARY_DIR})

target_compile_definitions(kadenza PRIVATE
    KADENZA_SOURCE_SIDECAR="${CMAKE_CURRENT_SOURCE_DIR}/sidecar")

qt_add_qml_module(kadenza
    URI io.github.timpalpant.kadenza
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/pages/LoginPage.qml
        qml/pages/HomePage.qml
        qml/pages/NowPlayingPage.qml
        qml/pages/LibraryPage.qml
        qml/pages/SearchPage.qml
        qml/pages/ChartsPage.qml
        qml/pages/RadioPage.qml
        qml/pages/DetailPage.qml
        qml/pages/QueuePage.qml
        qml/pages/SettingsPage.qml
        qml/pages/AboutPage.qml
        qml/components/ArtworkImage.qml
        qml/components/MediaDelegate.qml
        qml/components/MediaGridDelegate.qml
        qml/components/PlayerBar.qml
        qml/components/SeekSlider.qml
        qml/components/EdgeSeekBar.qml
        qml/components/MediaGrid.qml
        qml/components/ArtworkSizeSlider.qml
        qml/components/LinkLabel.qml
    RESOURCES
        data/icons/io.github.timpalpant.kadenza.svg
        data/demo/violet-hours.svg
        data/demo/night-transit.svg
        data/demo/tidal-memory.svg
        data/demo/soft-geometry.svg
        data/demo/cloudline.svg
        data/demo/static-bloom.svg
)

target_link_libraries(kadenza PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Widgets Qt6::Network Qt6::DBus Qt6::Sql
    KF6::CoreAddons KF6::I18n KF6::I18nQml KF6::ConfigCore)
target_link_libraries(kadenza PRIVATE KF6::Notifications KF6::GlobalAccel)

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()

install(TARGETS kadenza RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(FILES data/io.github.timpalpant.kadenza.desktop
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/applications)
install(FILES data/io.github.timpalpant.kadenza.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/metainfo)
install(FILES data/icons/io.github.timpalpant.kadenza.svg
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/icons/hicolor/scalable/apps)
install(FILES data/kadenza.notifyrc
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/knotifications6)
install(FILES sidecar/main.js sidecar/preload.js sidecar/package.json sidecar/package-lock.json
        DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/kadenza/sidecar)
# Only dist/, not the whole npm package: locateElectron() (playercontroller.cpp)
# execs node_modules/electron/dist/electron directly and never touches the
# package's own cli.js, install.js, docs, or vmp-resign.py. Bundling those
# unused files broke RPM packaging outright -- Fedora's build policy hard-
# rejects vmp-resign.py's ambiguous "#!/usr/bin/env python" shebang -- for
# files nothing ever runs.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/sidecar/node_modules/electron/dist/electron")
    install(DIRECTORY sidecar/node_modules/electron/dist
            DESTINATION ${CMAKE_INSTALL_DATAROOTDIR}/kadenza/sidecar/node_modules/electron
            USE_SOURCE_PERMISSIONS)
endif()

set(CPACK_PACKAGE_NAME "kadenza")
set(CPACK_PACKAGE_VENDOR "Timothy Palpant")
set(CPACK_PACKAGE_CONTACT "Timothy Palpant <tim@palpant.us>")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Native Apple Music client for KDE Plasma")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "${CPACK_PACKAGE_CONTACT}")
set(CPACK_DEBIAN_PACKAGE_SECTION "sound")
# Not nodejs: the bundled Electron is self-contained (it embeds its own Node
# runtime), and Kadenza execs it directly by absolute path rather than through a
# system node/npm.
# qml6-module-org-kde-desktop provides the "org.kde.desktop" QQC2 style
# main.cpp always selects unless QT_QUICK_CONTROLS_STYLE is already set --
# without it the QML engine refuses to load at all, confirmed by installing
# this .deb in a bare debian:trixie container with none of Kadenza's own
# dependencies pre-satisfied by a desktop environment.
#
# qt6-svg-plugins is the QImageReader plugin the demo library's placeholder
# artwork (data/demo/*.svg) needs to decode at all -- Arch never surfaces this
# gap because qt6-svg is already a dependency of kirigami there, but Debian
# does not pull it in transitively the same way.
set(CPACK_DEBIAN_PACKAGE_DEPENDS
    "qml6-module-qtcore, qml6-module-qtquick, qml6-module-qtquick-controls, qml6-module-qtquick-layouts, qml6-module-org-kde-kirigami, qml6-module-org-kde-desktop, qt6-svg-plugins, libqt6sql6-sqlite, libkf6notifications6, libkf6globalaccel6")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
# Not nodejs: see the matching note above CPACK_DEBIAN_PACKAGE_DEPENDS.
# kf6-qqc2-desktop-style is the Fedora name for the same "org.kde.desktop"
# style covered by qml6-module-org-kde-desktop on Debian, above.
# qt6-qtbase-sqlite is not a real Fedora package -- Fedora bundles the SQLite
# Qt driver directly in qt6-qtbase, which qt6-qtdeclarative already requires;
# that bogus name aborted every `dnf install` of this package outright,
# confirmed by installing this .rpm in a bare Fedora container.
# qt6-qtsvg is the SVG imageformats plugin the demo library's placeholder
# artwork (data/demo/*.svg) needs to decode at all; see the matching note
# above CPACK_DEBIAN_PACKAGE_DEPENDS.
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtdeclarative, kf6-kirigami, kf6-knotifications, kf6-kglobalaccel, kf6-qqc2-desktop-style, qt6-qtsvg")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
include(CPack)
