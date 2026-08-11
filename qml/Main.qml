import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.timpalpant.kanzi

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Kanzi")
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: Kirigami.Units.gridUnit * 28
    width: Kirigami.Units.gridUnit * 64
    height: Kirigami.Units.gridUnit * 42

    readonly property string initialPageKey: App.authenticated ? App.lastPage : "login"
    property string currentPage: initialPageKey
    property var navigationHistory: [initialPageKey]
    // Destinations reachable from the sidebar. Everything else (detail pages)
    // stacks on top of whichever one is showing.
    readonly property var topLevelPages: ["home", "now-playing", "recently-added",
                                          "songs", "albums", "artists", "playlists",
                                          "search", "charts", "radio", "queue", "settings",
                                          "about"]
    property string pendingPlaylistSongId: ""
    readonly property bool compactMode: width < Kirigami.Units.gridUnit * 40
    // Wide enough for the longest destination ("Recently Added") at the default
    // font, so the sidebar does not open with an elided label.
    property real desktopSidebarWidth: App.sidebarWidth > 0
                                       ? App.sidebarWidth
                                       : Kirigami.Units.gridUnit * 10

    Component { id: loginPage; LoginPage {} }
    Component { id: homePage; HomePage {} }
    Component { id: nowPlayingPage; NowPlayingPage {} }
    Component { id: recentlyAddedPage; LibraryPage { kind: "recently-added"; title: i18n("Recently Added"); iconName: "document-open-recent"; gridMode: true } }
    Component { id: songsPage; LibraryPage { kind: "songs"; title: i18n("Songs"); iconName: "view-media-track" } }
    Component { id: albumsPage; LibraryPage { kind: "albums"; title: i18n("Albums"); iconName: "media-album-cover"; gridMode: true } }
    Component { id: artistsPage; LibraryPage { kind: "artists"; title: i18n("Artists"); iconName: "view-media-artist"; gridMode: true } }
    Component { id: playlistsPage; LibraryPage { kind: "playlists"; title: i18n("Playlists"); iconName: "view-media-playlist"; gridMode: true } }
    Component { id: searchPage; SearchPage {} }
    Component { id: chartsPage; ChartsPage {} }
    Component { id: radioPage; RadioPage {} }
    Component { id: queuePage; QueuePage {} }
    Component { id: settingsPage; SettingsPage {} }
    Component { id: aboutPage; AboutPage {} }
    Component { id: detailPage; DetailPage {} }

    function componentFor(key) {
        switch (key) {
        case "login": return loginPage;
        case "home": return homePage;
        case "now-playing": return nowPlayingPage;
        case "recently-added": return recentlyAddedPage;
        case "songs": return songsPage;
        case "albums": return albumsPage;
        case "artists": return artistsPage;
        case "playlists": return playlistsPage;
        case "search": return searchPage;
        case "charts": return chartsPage;
        case "radio": return radioPage;
        case "queue": return queuePage;
        case "settings": return settingsPage;
        case "about": return aboutPage;
        case "detail": return detailPage;
        }
        return null;
    }

    function navigate(key) {
        const pageKey = String(key);
        const component = componentFor(pageKey);
        if (!component) return;
        if (currentPage === pageKey) {
            if (compactMode) globalDrawer.close();
            return;
        }
        // Switching sidebar destinations replaces the stack. Pushing on every
        // visit grew pageStack without bound and made Back retrace the whole
        // click history instead of leaving a detail page.
        if (topLevelPages.indexOf(pageKey) >= 0) {
            pageStack.clear();
            pageStack.push(component);
            navigationHistory = [pageKey];
        } else {
            pageStack.push(component);
            navigationHistory = navigationHistory.concat([pageKey]);
        }
        currentPage = pageKey;
        App.lastPage = pageKey;
        if (compactMode) globalDrawer.close();
    }

    function goBack() {
        if (navigationHistory.length <= 1 || pageStack.depth <= 1) return;
        navigationHistory = navigationHistory.slice(0, -1);
        currentPage = navigationHistory[navigationHistory.length - 1];
        pageStack.pop();
    }

    function resetNavigation(key) {
        const component = componentFor(key);
        pageStack.clear();
        pageStack.push(component);
        currentPage = key;
        navigationHistory = [key];
    }

    function openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork) {
        App.openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork);
        navigate("detail");
    }

    // Most Apple responses omit the artist and album relationships, so an id
    // is the exception rather than the rule. Without one, look the name up in
    // the catalog and navigate when the answer arrives.
    function openArtist(artistId, artistName) {
        if (artistId.length > 0) {
            openDetail(artistId, artistId, "artists", artistName, "", "");
        } else {
            App.openArtistNamed(artistName);
        }
    }

    function openAlbum(albumId, albumName, artistName, artwork) {
        if (albumId.length > 0) {
            openDetail(albumId, albumId, "albums", albumName, artistName, artwork);
        } else {
            App.openAlbumNamed(albumName, artistName);
        }
    }

    Connections {
        target: App
        function onDetailOpened() { root.navigate("detail"); }
    }

    function showAddToPlaylist(songId) {
        pendingPlaylistSongId = songId;
        App.loadLibrary("playlists");
        addToPlaylistDialog.open();
    }

    function showCreatePlaylist() {
        playlistName.clear();
        createPlaylistDialog.open();
        playlistName.forceActiveFocus();
    }

    Shortcut { sequence: "Alt+Left"; enabled: pageStack.depth > 1; onActivated: root.goBack() }
    Shortcut { sequence: "Ctrl+Alt+Space"; onActivated: App.player.playPause() }

    Connections {
        target: App
        function onAuthenticatedChanged() {
            if (App.authenticated && root.currentPage === "login") root.resetNavigation(App.lastPage);
            else if (!App.authenticated) root.resetNavigation("login");
        }
    }

    globalDrawer: Kirigami.GlobalDrawer {
        id: globalDrawer
        title: ""
        titleIcon: ""
        // Collapsed, Kirigami sizes the drawer to its icon rail itself.
        width: collapsed && !root.compactMode
               ? implicitWidth
               : root.compactMode
                 ? Math.min(root.width * 0.85, Kirigami.Units.gridUnit * 16)
                 : root.desktopSidebarWidth
        isMenu: false
        modal: root.compactMode
        drawerOpen: !root.compactMode && App.authenticated
        enabled: App.authenticated
        collapsible: !root.compactMode
        collapsed: !root.compactMode && App.sidebarCollapsed
        onCollapsedChanged: if (!root.compactMode) App.sidebarCollapsed = collapsed
        actions: [
            Kirigami.Action {
                text: i18n("Listen Now")
                icon.name: "audio-headphones"
                checkable: true
                checked: root.currentPage === "home"
                onTriggered: root.navigate("home")
            },
            Kirigami.Action {
                text: i18n("Now Playing")
                icon.name: "media-album-cover"
                enabled: App.player.title.length > 0
                checkable: true
                checked: root.currentPage === "now-playing"
                onTriggered: root.navigate("now-playing")
            },
            Kirigami.Action {
                text: i18n("Recently Added")
                icon.name: "document-open-recent"
                checkable: true
                checked: root.currentPage === "recently-added"
                onTriggered: root.navigate("recently-added")
            },
            Kirigami.Action {
                text: i18n("Songs")
                icon.name: "view-media-track"
                checkable: true
                checked: root.currentPage === "songs"
                onTriggered: root.navigate("songs")
            },
            Kirigami.Action {
                text: i18n("Albums")
                icon.name: "view-media-album-cover"
                checkable: true
                checked: root.currentPage === "albums"
                onTriggered: root.navigate("albums")
            },
            Kirigami.Action {
                text: i18n("Artists")
                icon.name: "view-media-artist"
                checkable: true
                checked: root.currentPage === "artists"
                onTriggered: root.navigate("artists")
            },
            Kirigami.Action {
                text: i18n("Playlists")
                icon.name: "view-media-playlist"
                checkable: true
                checked: root.currentPage === "playlists"
                onTriggered: root.navigate("playlists")
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: i18n("Search")
                icon.name: "search"
                shortcut: "Ctrl+F"
                checkable: true
                checked: root.currentPage === "search"
                onTriggered: root.navigate("search")
            },
            Kirigami.Action {
                text: i18n("Charts")
                icon.name: "office-chart-bar"
                checkable: true
                checked: root.currentPage === "charts"
                onTriggered: root.navigate("charts")
            },
            Kirigami.Action {
                text: i18n("Radio")
                icon.name: "radio"
                checkable: true
                checked: root.currentPage === "radio"
                onTriggered: root.navigate("radio")
            },
            Kirigami.Action {
                text: i18n("Queue")
                icon.name: "media-playlist-append"
                checkable: true
                checked: root.currentPage === "queue"
                onTriggered: root.navigate("queue")
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: i18n("Settings")
                icon.name: "settings-configure"
                onTriggered: root.navigate("settings")
            },
            Kirigami.Action {
                text: i18n("About Kanzi")
                icon.name: "help-about"
                onTriggered: root.navigate("about")
            }
        ]

        MouseArea {
            id: drawerResizeHandle
            parent: globalDrawer.contentItem
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            width: Kirigami.Units.smallSpacing * 2
            visible: !root.compactMode && !globalDrawer.collapsed
            hoverEnabled: true
            cursorShape: Qt.SplitHCursor
            z: 100

            property real pressedAt
            property real pressedWidth

            onPressed: mouse => {
                pressedAt = mapToItem(root.contentItem, mouse.x, mouse.y).x;
                pressedWidth = root.desktopSidebarWidth;
            }
            onPositionChanged: mouse => {
                if (!pressed) return;
                const current = mapToItem(root.contentItem, mouse.x, mouse.y).x;
                root.desktopSidebarWidth = Math.max(Kirigami.Units.gridUnit * 7,
                                                    Math.min(root.width * 0.45,
                                                             pressedWidth + current - pressedAt));
            }
            onReleased: App.sidebarWidth = Math.round(root.desktopSidebarWidth)

            Rectangle {
                anchors.top: parent.top
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                width: 1
                color: drawerResizeHandle.containsMouse || drawerResizeHandle.pressed
                       ? Kirigami.Theme.highlightColor
                       : Kirigami.Theme.disabledTextColor
            }
        }
    }

    pageStack.initialPage: componentFor(initialPageKey)
    pageStack.columnView.columnResizeMode: Kirigami.ColumnView.SingleColumn

    pageStack.anchors.bottomMargin: playerBar.visible ? playerBar.height : 0

    PlayerBar {
        id: playerBar
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        visible: App.authenticated && App.player.title.length > 0
                 && root.currentPage !== "now-playing"
        z: 20
    }

    Kirigami.InlineMessage {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: playerBar.visible ? playerBar.top : parent.bottom
        anchors.margins: Kirigami.Units.largeSpacing
        visible: App.error.length > 0
        text: App.error
        type: Kirigami.MessageType.Error
        showCloseButton: true
        onVisibleChanged: if (!visible) App.clearError()
        z: 10
    }

    Kirigami.InlineMessage {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: playerBar.visible ? playerBar.top : parent.bottom
        anchors.margins: Kirigami.Units.largeSpacing
        visible: App.message.length > 0
        text: App.message
        type: Kirigami.MessageType.Positive
        showCloseButton: true
        onVisibleChanged: if (!visible) App.clearMessage()
        z: 11

        Timer {
            interval: 3500
            running: parent.visible
            onTriggered: App.clearMessage()
        }
    }

    QQC2.Dialog {
        id: addToPlaylistDialog
        parent: root.contentItem
        anchors.centerIn: parent
        modal: true
        title: i18n("Add to Playlist")
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        onAccepted: {
            if (playlistPicker.currentIndex >= 0)
                App.addToPlaylist(playlistPicker.currentValue, root.pendingPlaylistSongId);
        }
        ColumnLayout {
            width: Kirigami.Units.gridUnit * 18
            QQC2.ComboBox {
                id: playlistPicker
                model: App.playlistsModel
                textRole: "title"
                valueRole: "mediaId"
                Layout.fillWidth: true
            }
            QQC2.Button {
                text: i18n("Create New Playlist…")
                icon.name: "list-add"
                onClicked: {
                    addToPlaylistDialog.close();
                    root.showCreatePlaylist();
                }
            }
        }
    }

    QQC2.Dialog {
        id: createPlaylistDialog
        parent: root.contentItem
        anchors.centerIn: parent
        modal: true
        title: i18n("New Playlist")
        standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
        onAccepted: App.createPlaylist(playlistName.text)
        QQC2.TextField {
            id: playlistName
            width: Kirigami.Units.gridUnit * 18
            placeholderText: i18n("Playlist name")
            onAccepted: createPlaylistDialog.accept()
        }
    }
}
