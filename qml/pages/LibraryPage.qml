import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    required property string kind
    property string iconName
    property bool gridMode: false
    readonly property var mediaModel: kind === "recently-added" ? App.recentlyAddedModel
                                      : kind === "songs" ? App.songsModel
                                      : kind === "albums" ? App.albumsModel
                                      : kind === "artists" ? App.artistsModel : App.playlistsModel

    MediaFilterProxyModel {
        id: proxyModel
        sourceModel: mediaModel
    }

    readonly property bool empty: !mediaModel.loading && proxyModel.count === 0

    header: QQC2.ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            Kirigami.SearchField {
                id: searchField
                placeholderText: i18n("Search %1", page.title.toLowerCase())
                Layout.fillWidth: true
                onTextChanged: proxyModel.filterString = searchField.text
            }
        }
    }

    actions: [
        Kirigami.Action {
            visible: page.gridMode && !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: App.loadLibrary(page.kind, true)
        },
        Kirigami.Action {
            text: i18n("Play All")
            icon.name: "media-playback-start"
            visible: page.kind === "songs"
            enabled: page.mediaModel.count > 0
            onTriggered: App.playModel(page.mediaModel, 0)
        },
        Kirigami.Action {
            text: i18n("New Playlist")
            icon.name: "list-add"
            visible: page.kind === "playlists"
            onTriggered: applicationWindow().showCreatePlaylist()
        },
        Kirigami.Action {
            text: i18n("Browse Folders")
            icon.name: "folder"
            visible: page.kind === "playlists"
            onTriggered: applicationWindow().openPlaylistFolder("")
        }
    ]

    Component.onCompleted: App.loadLibrary(kind)

    // Kirigami.Page insets its content, which would leave the scrollbar
    // floating in a gutter a whole grid unit clear of the window edge.
    // ScrollablePage anchors past that inset for the same reason.
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    // The views sit in a ScrollView rather than carrying an attached ScrollBar
    // of their own: an attached bar is drawn over the flickable and reserves no
    // width, so it landed on top of the last control in every row. ScrollView
    // insets the content instead, which is also what Kirigami.ScrollablePage
    // does for the pages that were never affected.
    // Only the representation in use is built. Hiding the other one still left
    // it laying out and instantiating a viewport's worth of delegates —
    // measured at 29 rows for a list nobody was looking at.
    Loader {
        anchors.fill: parent
        active: !page.gridMode
        sourceComponent: QQC2.ScrollView {
            QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AsNeeded

            ListView {
                model: proxyModel
                clip: true
                reuseItems: true
                delegate: MediaDelegate {
                    onPlayRequested: App.playModel(page.mediaModel, proxyModel.mapToSource(proxyModel.index(index, 0)).row)
                    onOpenRequested: applicationWindow().openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork)
                }
                onAtYEndChanged: if (atYEnd) App.loadMore(page.kind)
            }
        }
    }

    Loader {
        anchors.fill: parent
        active: page.gridMode
        sourceComponent: QQC2.ScrollView {
            QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AsNeeded

            MediaGrid {
                model: proxyModel
                sizeScale: page.kind === "artists" ? 0.75 : 1
                onAtYEndChanged: if (atYEnd) App.loadMore(page.kind)
            }
        }
    }

    // One loading state at a time: skeleton tiles where a grid is coming,
    // a spinner where a list is.
    QQC2.BusyIndicator {
        anchors.centerIn: parent
        visible: page.mediaModel.loading && page.mediaModel.count === 0 && !page.gridMode
        running: visible
    }

    Row {
        anchors.centerIn: parent
        spacing: Kirigami.Units.largeSpacing
        visible: page.mediaModel.loading && page.mediaModel.count === 0 && page.gridMode
        Repeater {
            model: 3
            Rectangle {
                width: Kirigami.Units.gridUnit * 7
                height: width
                radius: Kirigami.Units.smallSpacing
                color: Kirigami.Theme.alternateBackgroundColor
                SequentialAnimation on opacity {
                    running: parent.visible
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 120 }
                    NumberAnimation { to: 0.35; duration: 500 }
                    NumberAnimation { to: 1; duration: 500 }
                }
            }
        }
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        visible: page.empty
        text: searchField.text.length > 0
              ? i18n("No results")
              : (page.mediaModel.error.length > 0 ? page.mediaModel.error : i18n("Nothing here yet"))
        icon.name: page.iconName
    }
}
