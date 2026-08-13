import QtQuick
import QtQuick.Controls as QQC2

QQC2.Menu {
    id: root
    property int index
    required property string mediaId
    required property string catalogId
    required property string mediaType
    required property bool playable
    required property bool favorite
    required property bool inLibrary
    required property int rating
    required property bool queueMode

    QQC2.MenuItem {
        text: i18n("Play Next")
        enabled: root.playable
        onTriggered: App.player.playNext(root.catalogId || root.mediaId)
    }
    QQC2.MenuSeparator { visible: root.playable }

    QQC2.MenuItem {
        text: root.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
        icon.name: root.favorite ? "rating-unrated" : "rating"
        onTriggered: App.setFavorite(root.catalogId || root.mediaId, root.mediaType, !root.favorite)
    }
    QQC2.MenuItem {
        text: root.inLibrary ? i18n("Remove from Library") : i18n("Add to Library")
        icon.name: root.inLibrary ? "list-remove" : "list-add"
        onTriggered: App.setInLibrary(root.inLibrary ? root.mediaId : (root.catalogId || root.mediaId), root.mediaType, !root.inLibrary)
    }
    QQC2.MenuItem {
        text: root.rating > 0 ? i18n("Loved") : i18n("Love")
        icon.name: "love"
        checkable: true
        checked: root.rating > 0
        onTriggered: App.setRating(root.catalogId || root.mediaId, root.mediaType, root.rating > 0 ? 0 : 1)
    }
    QQC2.MenuItem {
        text: root.rating < 0 ? i18n("Disliked") : i18n("Dislike")
        icon.name: "dialog-cancel"
        checkable: true
        checked: root.rating < 0
        onTriggered: App.setRating(root.catalogId || root.mediaId, root.mediaType, root.rating < 0 ? 0 : -1)
    }
    QQC2.MenuSeparator {}

    QQC2.MenuItem {
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
        enabled: root.ListView.view && root.index < root.ListView.view.count - 1
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
}
