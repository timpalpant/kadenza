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
    readonly property bool empty: !mediaModel.loading && mediaModel.count === 0

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
        }
    ]

    Component.onCompleted: App.loadLibrary(kind)

    ListView {
        anchors.fill: parent
        visible: !page.gridMode
        model: page.mediaModel
        clip: true
        reuseItems: true
        delegate: MediaDelegate {
            onPlayRequested: App.playModel(page.mediaModel, index)
            onOpenRequested: applicationWindow().openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork)
        }
        onAtYEndChanged: if (atYEnd) App.loadMore(page.kind)
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {
            policy: QQC2.ScrollBar.AsNeeded
        }
    }

    MediaGrid {
        anchors.fill: parent
        visible: page.gridMode
        model: page.mediaModel
        sizeScale: page.kind === "artists" ? 0.75 : 1
        onAtYEndChanged: if (atYEnd) App.loadMore(page.kind)
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {
            policy: QQC2.ScrollBar.AsNeeded
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
        text: page.mediaModel.error.length > 0 ? page.mediaModel.error : i18n("Nothing here yet")
        icon.name: page.iconName
    }
}
