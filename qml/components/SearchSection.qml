import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    property string heading
    property var model
    property bool horizontal: true
    property int tileWidth: Kirigami.Units.gridUnit * 11
    property real labelHeight: Kirigami.Units.gridUnit * 3
    property bool interactive: false
    property var delegateConfig

    Kirigami.Heading {
        text: root.heading
        level: 2
        visible: root.model && root.model.count > 0
    }

    Component {
        id: tileDelegate
        MediaGridDelegate {
            width: root.tileWidth
            height: root.tileWidth + root.labelHeight
            onOpenRequested: applicationWindow().openDetail(mediaId, catalogId, mediaType, title, subtitle, artwork)
            onPlayRequested: App.playCollection(mediaId, catalogId, mediaType)
        }
    }

    ListView {
        model: root.model
        orientation: root.horizontal ? ListView.Horizontal : ListView.Vertical
        flickableDirection: root.horizontal ? Flickable.HorizontalFlick : Flickable.VerticalFlick
        spacing: Kirigami.Units.smallSpacing
        clip: true
        visible: model && count > 0
        interactive: root.interactive
        reuseItems: !root.interactive
        Layout.fillWidth: true
        Layout.preferredHeight: root.horizontal ? root.tileWidth + root.labelHeight : contentHeight

        // A section passes its own row component (Songs uses MediaDelegate);
        // every other section shows the standard grid tile. The component is
        // handed to the ListView directly rather than through a Loader, so
        // delegates keep the ListView attached properties they size from.
        delegate: root.delegateConfig ?? tileDelegate

        QQC2.ScrollBar.horizontal: QQC2.ScrollBar {}
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {}
    }
}
