import QtQuick

Item {
    id: root
    property var source: null
    property int index: -1
    signal dropped(var source)

    DragHandler { id: dragHandler; enabled: root.enabled }
    Drag.active: dragHandler.active
    Drag.source: root
    Drag.hotSpot.x: width / 2
    Drag.hotSpot.y: height / 2

    DropArea {
        anchors.fill: parent
        enabled: root.enabled
        onDropped: drop => { if (drop.source) root.dropped(drop.source); }
    }
}
