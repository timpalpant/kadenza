import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

RowLayout {
    id: root

    spacing: Kirigami.Units.smallSpacing

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000));
        const minutes = Math.floor(seconds / 60);
        return minutes + ":" + String(seconds % 60).padStart(2, "0");
    }

    QQC2.Label {
        text: root.formatTime(slider.pressed ? slider.value : App.player.positionMs)
        color: Kirigami.Theme.disabledTextColor
        font.features: { "tnum": 1 }
        horizontalAlignment: Text.AlignRight
        // Both labels reserve the width of the longer one so the slider does
        // not shift sideways when the elapsed time gains a digit.
        Layout.preferredWidth: durationLabel.implicitWidth
    }

    QQC2.Slider {
        id: slider
        from: 0
        to: Math.max(1, App.player.durationMs)
        enabled: App.player.durationMs > 0
        Layout.fillWidth: true
        // A Slider's implicit width is generous; let it shrink so it competes
        // fairly with whatever sits beside it.
        Layout.minimumWidth: Kirigami.Units.gridUnit * 5
        Layout.preferredWidth: Kirigami.Units.gridUnit * 12

        // Dragging the handle assigns `value` imperatively, which destroys a
        // declarative binding permanently. Push the position in from a Binding
        // instead, so the handle keeps following playback after the first seek.
        Binding {
            target: slider
            property: "value"
            value: App.player.positionMs
            when: !slider.pressed
            restoreMode: Binding.RestoreNone
        }

        // Seek once on release rather than on every drag event, so scrubbing
        // does not flood the sidecar with intermediate positions.
        onPressedChanged: if (!pressed) App.player.seek(value)
    }

    QQC2.Label {
        id: durationLabel
        text: root.formatTime(App.player.durationMs)
        color: Kirigami.Theme.disabledTextColor
        font.features: { "tnum": 1 }
    }
}
