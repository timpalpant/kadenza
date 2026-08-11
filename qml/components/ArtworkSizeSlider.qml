import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Zoom control for grid artwork, in the spirit of Dolphin's view slider. The
// value is global and persisted, so every grid page stays in step.
RowLayout {
    id: root

    readonly property int minimumSize: 7
    readonly property int maximumSize: 20

    spacing: 0

    QQC2.ToolButton {
        text: i18n("Smaller Album Art")
        icon.name: "zoom-out"
        display: QQC2.AbstractButton.IconOnly
        enabled: App.artworkSize > root.minimumSize
        onClicked: App.artworkSize = App.artworkSize - 1
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
    }

    QQC2.Slider {
        id: slider
        from: root.minimumSize
        to: root.maximumSize
        stepSize: 1
        snapMode: QQC2.Slider.SnapAlways
        value: App.artworkSize
        onMoved: App.artworkSize = value
        Layout.preferredWidth: Kirigami.Units.gridUnit * 8
        QQC2.ToolTip.visible: hovered || pressed
        QQC2.ToolTip.text: i18n("Album art size")
    }

    QQC2.ToolButton {
        text: i18n("Larger Album Art")
        icon.name: "zoom-in"
        display: QQC2.AbstractButton.IconOnly
        enabled: App.artworkSize < root.maximumSize
        onClicked: App.artworkSize = App.artworkSize + 1
        QQC2.ToolTip.visible: hovered
        QQC2.ToolTip.text: text
    }
}
