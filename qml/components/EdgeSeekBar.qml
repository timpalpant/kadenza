import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// The playback progress line that hugs the top edge of the player bar.
//
// It is drawn as a hairline so it reads as part of the window chrome rather
// than as a control competing for the row below, but the pointer target is the
// full height of this item — several times taller than the line. Asking anyone
// to hit a two-pixel strip would not be acceptable. The line thickens on hover
// and grows a handle, so it still announces itself as something draggable.
Item {
    id: root

    readonly property int durationMs: App.player.durationMs
    readonly property bool seekable: durationMs > 0
    property bool dragging: false
    property real dragFraction: 0

    // Computed rather than assigned, so a drag cannot destroy the binding that
    // follows playback the way an imperative Slider value does.
    readonly property real fraction: dragging
        ? dragFraction
        : (seekable ? Math.min(1, App.player.positionMs / durationMs) : 0)

    readonly property bool active: hover.hovered || dragging
    readonly property int lineHeight: active ? 4 : 2

    // Only as tall as the line it draws. A taller item would reserve dead
    // space along the whole top of the bar and push everything below it off
    // centre; the pointer target is grown separately, by letting the mouse
    // area overhang downwards, which costs no layout height.
    implicitHeight: Kirigami.Units.smallSpacing
    z: 1

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000));
        const minutes = Math.floor(seconds / 60);
        return minutes + ":" + String(seconds % 60).padStart(2, "0");
    }

    function fractionAt(x) {
        return Math.max(0, Math.min(1, x / Math.max(1, root.width)));
    }

    Rectangle {
        id: track
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: root.lineHeight
        color: Kirigami.Theme.textColor
        opacity: 0.15
        Behavior on height {
            NumberAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    Rectangle {
        id: elapsed
        anchors.left: parent.left
        anchors.top: parent.top
        width: root.fraction * root.width
        height: root.lineHeight
        color: Kirigami.Theme.highlightColor
        Behavior on height {
            NumberAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    Rectangle {
        id: handle
        x: elapsed.width - width / 2
        y: track.height / 2 - height / 2
        width: Kirigami.Units.gridUnit * 0.75
        height: width
        radius: width / 2
        color: Kirigami.Theme.highlightColor
        border.width: 1
        border.color: Kirigami.Theme.backgroundColor
        visible: opacity > 0
        opacity: root.active && root.seekable ? 1 : 0
        Behavior on opacity {
            NumberAnimation { duration: Kirigami.Units.shortDuration }
        }
    }

    HoverHandler {
        id: hover
        enabled: root.seekable
        cursorShape: Qt.PointingHandCursor
    }

    MouseArea {
        id: area
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        // Deliberately taller than the item, overhanging the row below: a
        // two-pixel strip is not a reasonable thing to ask anyone to hit. Kept
        // shallow enough that it does not cover the artwork or the title.
        height: Kirigami.Units.smallSpacing * 2.5
        enabled: root.seekable
        hoverEnabled: true
        preventStealing: true

        onPressed: mouse => {
            root.dragFraction = root.fractionAt(mouse.x);
            root.dragging = true;
        }
        onPositionChanged: mouse => {
            if (root.dragging) root.dragFraction = root.fractionAt(mouse.x);
        }
        // Seek once on release rather than on every drag event, so scrubbing
        // does not flood the sidecar with intermediate positions.
        onReleased: {
            App.player.seek(Math.round(root.dragFraction * root.durationMs));
            root.dragging = false;
        }
        onCanceled: root.dragging = false

        // The line carries no numbers of its own, so the position it would seek
        // to is spelled out under the pointer.
        QQC2.ToolTip.visible: (area.containsMouse || root.dragging) && root.seekable
        QQC2.ToolTip.text: root.formatTime(
            (root.dragging ? root.dragFraction : root.fractionAt(area.mouseX))
            * root.durationMs)
    }
}
