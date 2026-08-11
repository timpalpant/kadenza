import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    title: i18n("Queue")

    // Kirigami.Page insets its content, which would leave the scrollbar
    // floating in a gutter a whole grid unit clear of the window edge.
    // ScrollablePage anchors past that inset for the same reason.
    leftPadding: 0
    rightPadding: 0
    topPadding: 0
    bottomPadding: 0

    // In a ScrollView so the scrollbar takes its own width instead of being
    // drawn over the last control in each row.
    QQC2.ScrollView {
        anchors.fill: parent
        QQC2.ScrollBar.vertical.policy: QQC2.ScrollBar.AsNeeded

        ListView {
            id: queueList
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
        }
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
