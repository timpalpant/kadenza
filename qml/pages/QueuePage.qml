import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    title: i18n("Queue")

    ListView {
        id: queueList
        anchors.fill: parent
        model: App.player.queueModel
        currentIndex: App.player.currentQueueIndex
        clip: true
        reuseItems: true
        delegate: MediaDelegate {
            queueMode: true
            forceHighlighted: index === App.player.currentQueueIndex
            past: index < App.player.currentQueueIndex
            onPlayRequested: App.player.playQueueIndex(index)
            onOpenRequested: {}
        }
        onCurrentIndexChanged: if (currentIndex >= 0) positionViewAtIndex(currentIndex, ListView.Contain)
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
    }

    Connections {
        target: App.player
        function onQueuePositionChanged() {
            if (App.player.currentQueueIndex >= 0)
                queueList.positionViewAtIndex(App.player.currentQueueIndex, ListView.Contain);
        }
    }

    Kirigami.PlaceholderMessage {
        anchors.centerIn: parent
        visible: App.player.queueModel.count === 0
        text: i18n("The queue is empty")
        icon.name: "view-media-playlist"
    }
}
