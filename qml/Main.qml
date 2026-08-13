import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami
import io.github.timpalpant.kadenza as Kadenza

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Kadenza")
    minimumWidth: Kirigami.Units.gridUnit * 18
    minimumHeight: Kirigami.Units.gridUnit * 28
    width: Kirigami.Units.gridUnit * 64
    height: Kirigami.Units.gridUnit * 42

    readonly property string initialPageKey: App.authenticated ? App.lastPage : Kadenza.Pages.login
    property string currentPage: initialPageKey
    property var navigationHistory: [initialPageKey]
    // Destinations reachable from the sidebar. Everything else (detail pages)
    // stacks on top of whichever one is showing.
    readonly property var topLevelPages: [Kadenza.Pages.home, Kadenza.Pages.nowPlaying, Kadenza.Pages.recentlyAdded,
                                          Kadenza.Pages.songs, Kadenza.Pages.albums, Kadenza.Pages.artists, Kadenza.Pages.playlists,
                                          Kadenza.Pages.search, Kadenza.Pages.charts, Kadenza.Pages.radio, Kadenza.Pages.replay, Kadenza.Pages.queue,
                                          Kadenza.Pages.settings, Kadenza.Pages.about]
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
    Component { id: replayPage; ReplayPage {} }
    Component { id: queuePage; QueuePage {} }
    Component { id: settingsPage; SettingsPage {} }
    Component { id: aboutPage; AboutPage {} }
    Component { id: detailPage; DetailPage {} }
    Component { id: playlistFolderPage; PlaylistFolderPage {} }

    function componentFor(key) {
        switch (key) {
        case Kadenza.Pages.login: return loginPage;
        case Kadenza.Pages.home: return homePage;
        case Kadenza.Pages.nowPlaying: return nowPlayingPage;
        case Kadenza.Pages.recentlyAdded: return recentlyAddedPage;
        case Kadenza.Pages.songs: return songsPage;
        case Kadenza.Pages.albums: return albumsPage;
        case Kadenza.Pages.artists: return artistsPage;
        case Kadenza.Pages.playlists: return playlistsPage;
        case Kadenza.Pages.search: return searchPage;
        case Kadenza.Pages.charts: return chartsPage;
        case Kadenza.Pages.radio: return radioPage;
        case Kadenza.Pages.replay: return replayPage;
        case Kadenza.Pages.queue: return queuePage;
        case Kadenza.Pages.settings: return settingsPage;
        case Kadenza.Pages.about: return aboutPage;
        case Kadenza.Pages.detail: return detailPage;
        case Kadenza.Pages.playlistFolder: return playlistFolderPage;
        }
        return null;
    }

    /// Set while this file is the one driving pageStack, so the resync below
    /// only reacts to pops it did not perform itself.
    property bool navigating: false

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
        navigating = true;
        if (topLevelPages.indexOf(pageKey) >= 0) {
            pageStack.clear();
            pageStack.push(component);
            navigationHistory = [pageKey];
        } else {
            pageStack.push(component);
            navigationHistory = navigationHistory.concat([pageKey]);
        }
        navigating = false;
        currentPage = pageKey;
        App.lastPage = pageKey;
        if (compactMode) globalDrawer.close();
    }

    function goBack() {
        if (navigationHistory.length <= 1 || pageStack.depth <= 1) return;
        navigating = true;
        navigationHistory = navigationHistory.slice(0, -1);
        currentPage = navigationHistory[navigationHistory.length - 1];
        pageStack.pop();
        navigating = false;
    }

    // Kirigami's header back button calls PageRow.goBack(), which does not pop
    // the stack: it steps currentIndex back and leaves the page where it is.
    // So depth never changes, and without following currentIndex instead,
    // currentPage still said "detail" after returning to Artists —
    // navigate("detail") then matched it, returned early, and clicking another
    // artist did nothing at all.
    Connections {
        target: pageStack

        function onCurrentIndexChanged() {
            if (root.navigating)
                return;
            const index = pageStack.currentIndex;
            if (index < 0 || index >= root.navigationHistory.length - 1)
                return;
            root.navigationHistory = root.navigationHistory.slice(0, index + 1);
            root.currentPage = root.navigationHistory[index];
        }
    }

    function resetNavigation(key) {
        navigating = true;
        const component = componentFor(key);
        pageStack.clear();
        pageStack.push(component);
        currentPage = key;
        navigationHistory = [key];
        navigating = false;
    }

    function openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork) {
        App.openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork);
        navigate(Kadenza.Pages.detail);
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

    function openPlaylistFolder(folderId) {
        App.loadPlaylistFolder(folderId);
        navigate(Kadenza.Pages.playlistFolder);
    }

    Connections {
        target: App
        function onDetailOpened() { root.navigate(Kadenza.Pages.detail); }
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
            if (App.authenticated && root.currentPage === Kadenza.Pages.login) root.resetNavigation(App.lastPage);
            else if (!App.authenticated) root.resetNavigation(Kadenza.Pages.login);
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
                checked: root.currentPage === Kadenza.Pages.home
                onTriggered: root.navigate(Kadenza.Pages.home)
            },
            Kirigami.Action {
                text: i18n("Now Playing")
                icon.name: "media-album-cover"
                enabled: App.player.title.length > 0
                checkable: true
                checked: root.currentPage === Kadenza.Pages.nowPlaying
                onTriggered: root.navigate(Kadenza.Pages.nowPlaying)
            },
            Kirigami.Action {
                text: i18n("Recently Added")
                icon.name: "document-open-recent"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.recentlyAdded
                onTriggered: root.navigate(Kadenza.Pages.recentlyAdded)
            },
            Kirigami.Action {
                text: i18n("Songs")
                icon.name: "view-media-track"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.songs
                onTriggered: root.navigate(Kadenza.Pages.songs)
            },
            Kirigami.Action {
                text: i18n("Albums")
                icon.name: "view-media-album-cover"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.albums
                onTriggered: root.navigate(Kadenza.Pages.albums)
            },
            Kirigami.Action {
                text: i18n("Artists")
                icon.name: "view-media-artist"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.artists
                onTriggered: root.navigate(Kadenza.Pages.artists)
            },
            Kirigami.Action {
                text: i18n("Playlists")
                icon.name: "view-media-playlist"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.playlists
                onTriggered: root.navigate(Kadenza.Pages.playlists)
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: i18n("Search")
                icon.name: "search"
                shortcut: "Ctrl+F"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.search
                onTriggered: root.navigate(Kadenza.Pages.search)
            },
            Kirigami.Action {
                text: i18n("Charts")
                icon.name: "office-chart-bar"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.charts
                onTriggered: root.navigate(Kadenza.Pages.charts)
            },
            Kirigami.Action {
                text: i18n("Radio")
                icon.name: "radio"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.radio
                onTriggered: root.navigate(Kadenza.Pages.radio)
            },
            Kirigami.Action {
                text: i18n("Replay")
                icon.name: "view-calendar-year"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.replay
                onTriggered: root.navigate(Kadenza.Pages.replay)
            },
            Kirigami.Action {
                text: i18n("Queue")
                icon.name: "media-playlist-append"
                checkable: true
                checked: root.currentPage === Kadenza.Pages.queue
                onTriggered: root.navigate(Kadenza.Pages.queue)
            },
            Kirigami.Action {
                separator: true
            },
            Kirigami.Action {
                text: i18n("Settings")
                icon.name: "settings-configure"
                onTriggered: root.navigate(Kadenza.Pages.settings)
            },
            Kirigami.Action {
                text: i18n("About Kadenza")
                icon.name: "help-about"
                onTriggered: root.navigate(Kadenza.Pages.about)
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

    // The first page is pushed rather than bound to pageStack.initialPage.
    // PageRow reacts to initialPage changing by running clear() + push(), and
    // initialPageKey follows App.lastPage — which navigate() writes on every
    // visit. Bound, each navigation therefore built its page, threw it away
    // and built it again; on a library grid that was twice the work of the
    // most expensive page in the app.
    Component.onCompleted: root.resetNavigation(root.initialPageKey)

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
