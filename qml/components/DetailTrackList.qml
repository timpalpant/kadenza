import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property var model
    property bool artistPage: false
    property bool collectionShelfPage: false

    ListView {
        model: root.model
        visible: !root.artistPage && !root.collectionShelfPage
        interactive: false
        reuseItems: true
        Layout.fillWidth: true
        Layout.preferredHeight: contentHeight
        delegate: MediaDelegate {
            onPlayRequested: App.playDetail(index)
            onOpenRequested: {}
        }
    }

    MediaGrid {
        model: root.model
        visible: root.collectionShelfPage
        Layout.fillWidth: true
        Layout.preferredHeight: fullHeight
    }

    Kirigami.PlaceholderMessage {
        visible: root.collectionShelfPage && !root.model.loading && root.model.count === 0
        text: i18n("Nothing here yet")
        icon.name: "view-media-playlist"
        Layout.alignment: Qt.AlignHCenter
        Layout.topMargin: Kirigami.Units.gridUnit * 6
    }
}
