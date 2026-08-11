import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: root
    required property int index
    required property string mediaId
    required property string catalogId
    required property string mediaType
    required property string title
    required property string subtitle
    required property string album
    required property string artwork
    required property int durationMs
    required property bool explicitContent
    required property bool playable
    required property string artistId
    required property string albumId
    required property bool favorite
    required property bool inLibrary
    required property int rating
    property bool forceHighlighted: false
    property bool queueMode: false
    property bool past: false
    property bool hidePast: false
    // Drops the album, duration and explicit columns so the row still fits in
    // a narrow side panel such as Now Playing's Up Next list.
    property bool compact: false

    signal openRequested()
    signal playRequested()
    signal artistRequested()
    signal albumRequested()

    readonly property bool song: root.mediaType === "songs" || root.mediaType === "library-songs"

    // Wide enough that pulling three buttons out of the menu still leaves the
    // title and album room to breathe. A track list on a detail page clears
    // this easily; the Up Next panel does not.
    readonly property bool roomForActions:
        root.song && !root.compact && root.width > Kirigami.Units.gridUnit * 30

    width: ListView.view ? ListView.view.width : implicitWidth
    height: root.hidePast && root.past ? 0 : implicitHeight
    visible: !(root.hidePast && root.past)
    padding: Kirigami.Units.smallSpacing
    enabled: !root.song || root.playable
    highlighted: root.forceHighlighted || (root.song && ListView.isCurrentItem)
    opacity: root.past ? 0.55 : 1
    onClicked: {
        if (root.song && ListView.view) ListView.view.currentIndex = root.index;
        else if (!root.song) root.openRequested();
    }
    onDoubleClicked: if (root.song) root.playRequested()
    QQC2.ToolTip.visible: hovered && root.song && !root.playable
    QQC2.ToolTip.text: i18n("Not available to play yet")

    contentItem: RowLayout {
        spacing: Kirigami.Units.largeSpacing
        ArtworkImage {
            source: root.artwork
            fallbackIcon: root.mediaType.indexOf("artist") >= 0 ? "view-media-artist" : "media-album-cover"
            circular: root.mediaType.indexOf("artist") >= 0
            Layout.preferredWidth: root.compact ? Kirigami.Units.iconSizes.medium
                                                : Kirigami.Units.iconSizes.large
            Layout.preferredHeight: Layout.preferredWidth
        }
        ColumnLayout {
            spacing: 0
            Layout.fillWidth: true
            QQC2.Label {
                text: root.title
                elide: Text.ElideRight
                font.weight: Font.Medium
                Layout.fillWidth: true
            }
            RowLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true
                LinkLabel {
                    text: root.subtitle
                    // The subtitle is an artist on songs and albums, but a
                    // curator or genre on playlists and stations.
                    linked: root.subtitle.length > 0
                            && (root.song || root.mediaType.indexOf("albums") >= 0)
                    onActivated: applicationWindow().openArtist(root.artistId, root.subtitle)
                }
                // The separator is deliberately outside the link: including it
                // made the hyphen part of the clickable album name.
                QQC2.Label {
                    text: "—"
                    color: Kirigami.Theme.disabledTextColor
                    visible: albumLink.visible
                }
                LinkLabel {
                    id: albumLink
                    text: root.album.length > 0 && root.album !== root.title ? root.album : ""
                    visible: !root.compact && text.length > 0
                    linked: root.album.length > 0
                    Layout.fillWidth: true
                    onActivated: applicationWindow().openAlbum(root.albumId, root.album,
                                                              root.subtitle, root.artwork)
                }
            }
        }
        Kirigami.Icon {
            visible: root.rating !== 0
            source: root.rating > 0 ? "love" : "dialog-cancel"
            implicitWidth: Kirigami.Units.iconSizes.small
            implicitHeight: Kirigami.Units.iconSizes.small
            color: root.rating > 0 ? Kirigami.Theme.positiveTextColor
                                   : Kirigami.Theme.disabledTextColor
            QQC2.ToolTip.visible: ratingHover.hovered
            QQC2.ToolTip.text: root.rating > 0 ? i18n("Loved") : i18n("Disliked")
            HoverHandler { id: ratingHover }
        }
        Rectangle {
            visible: !root.compact && root.explicitContent
            implicitWidth: Kirigami.Units.gridUnit
            implicitHeight: Kirigami.Units.gridUnit
            radius: Kirigami.Units.smallSpacing / 2
            color: "transparent"
            border.width: 1
            border.color: Kirigami.Theme.disabledTextColor
            QQC2.Label {
                anchors.centerIn: parent
                text: i18nc("Abbreviation for explicit content", "E")
                font: Kirigami.Theme.smallFont
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.ToolTip.visible: explicitHover.hovered
            QQC2.ToolTip.text: i18n("Explicit content")
            HoverHandler { id: explicitHover }
        }
        QQC2.Label {
            visible: !root.compact && root.durationMs > 0
            text: Math.floor(root.durationMs / 60000) + ":" + String(Math.floor(root.durationMs / 1000) % 60).padStart(2, "0")
            color: Kirigami.Theme.disabledTextColor
            font.features: { "tnum": 1 }
        }
        // The per-track actions come out of the menu when the row is wide
        // enough to hold them; queueing especially, so a queue can be built up
        // while browsing without opening a menu for every track. Narrow lists —
        // the Up Next panel, a compact window — keep everything folded away.
        // The test reads the row's own width, which the view sets, so unfolding
        // these cannot feed back into the decision to unfold them.
        QQC2.ToolButton {
            visible: root.roomForActions
            text: i18n("Play Next")
            icon.name: "media-playlist-append"
            display: QQC2.AbstractButton.IconOnly
            enabled: root.playable
            onClicked: App.player.playNext(root.catalogId || root.mediaId)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
        }
        QQC2.ToolButton {
            visible: root.roomForActions
            text: i18n("Play Later")
            icon.name: "media-playlist-normal"
            display: QQC2.AbstractButton.IconOnly
            enabled: root.playable
            onClicked: App.player.playLater(root.catalogId || root.mediaId)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
        }
        QQC2.ToolButton {
            visible: root.roomForActions
            text: root.inLibrary ? i18n("Remove from Library")
                                 : i18n("Add to Library")
            icon.name: root.inLibrary ? "list-remove" : "list-add"
            display: QQC2.AbstractButton.IconOnly
            onClicked: App.setInLibrary(root.inLibrary
                                        ? root.mediaId
                                        : (root.catalogId || root.mediaId),
                                        root.mediaType, !root.inLibrary)
            QQC2.ToolTip.visible: hovered
            QQC2.ToolTip.text: text
        }
        QQC2.ToolButton {
            icon.name: "application-menu"
            text: i18n("More Options")
            display: QQC2.AbstractButton.IconOnly
            onClicked: menu.popup()
            QQC2.Menu {
                id: menu
                QQC2.MenuItem {
                    text: i18n("Play Next")
                    enabled: root.playable
                    onTriggered: App.player.playNext(root.catalogId || root.mediaId)
                }
                QQC2.MenuSeparator { visible: root.song }
                QQC2.MenuItem {
                    visible: root.song
                    text: root.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
                    icon.name: root.favorite ? "rating-unrated" : "rating"
                    onTriggered: App.setFavorite(root.catalogId || root.mediaId, root.mediaType, !root.favorite)
                }
                QQC2.MenuItem {
                    visible: root.song
                    text: root.inLibrary ? i18n("Remove from Library") : i18n("Add to Library")
                    icon.name: root.inLibrary ? "list-remove" : "list-add"
                    onTriggered: App.setInLibrary(root.inLibrary ? root.mediaId : (root.catalogId || root.mediaId), root.mediaType, !root.inLibrary)
                }
                QQC2.MenuItem {
                    visible: root.song
                    text: root.rating > 0 ? i18n("Loved") : i18n("Love")
                    icon.name: "love"
                    checkable: true
                    checked: root.rating > 0
                    // Choosing the current rating again clears it.
                    onTriggered: App.setRating(root.catalogId || root.mediaId,
                                               root.mediaType,
                                               root.rating > 0 ? 0 : 1)
                }
                QQC2.MenuItem {
                    visible: root.song
                    text: root.rating < 0 ? i18n("Disliked") : i18n("Dislike")
                    icon.name: "dialog-cancel"
                    checkable: true
                    checked: root.rating < 0
                    onTriggered: App.setRating(root.catalogId || root.mediaId,
                                               root.mediaType,
                                               root.rating < 0 ? 0 : -1)
                }
                QQC2.MenuSeparator { visible: root.song }
                QQC2.MenuItem {
                    visible: root.song
                    text: i18n("Add to Playlist…")
                    icon.name: "view-media-playlist"
                    onTriggered: applicationWindow().showAddToPlaylist(root.catalogId || root.mediaId)
                }
                QQC2.MenuSeparator { visible: root.queueMode }
                QQC2.MenuItem {
                    visible: root.queueMode
                    text: i18n("Move Up")
                    enabled: root.index > 0
                    onTriggered: App.player.moveQueueItem(root.index, root.index - 1)
                }
                QQC2.MenuItem {
                    visible: root.queueMode
                    text: i18n("Move Down")
                    enabled: ListView.view && root.index < ListView.view.count - 1
                    onTriggered: App.player.moveQueueItem(root.index, root.index + 1)
                }
                QQC2.MenuItem {
                    visible: root.queueMode
                    text: i18n("Remove from Queue")
                    icon.name: "edit-delete"
                    onTriggered: App.player.removeQueueItem(root.index)
                }
                QQC2.MenuItem {
                    text: i18n("Play Later")
                    enabled: root.playable
                    onTriggered: App.player.playLater(root.catalogId || root.mediaId)
                }
                QQC2.MenuItem {
                    text: i18n("Open")
                    visible: !root.song
                    onTriggered: root.openRequested()
                }
            }
        }
    }

    DragHandler {
        id: dragHandler
        enabled: root.queueMode
    }
    Drag.active: dragHandler.active
    Drag.source: root
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2
    DropArea {
        anchors.fill: parent
        enabled: root.queueMode
        onDropped: drop => {
            const source = drop.source;
            if (source && source.index !== root.index)
                App.player.moveQueueItem(source.index, root.index);
        }
    }
}
