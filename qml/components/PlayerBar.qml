import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Progress runs along the top edge of the bar rather than inline, which frees
// the row of the one element that wanted unbounded width. What is left is three
// zones: the track on the left, transport in the middle, and the secondary
// buttons on the right. The outer two carry equal weight, so the transport
// stays put instead of sliding sideways whenever a track title changes length.
QQC2.ToolBar {
    id: root

    readonly property bool compact: applicationWindow().compactMode

    // The queue model's get() is a plain call rather than a binding source, so
    // the playing track's rating has to be re-read whenever the queue changes
    // underneath it. Same shape as Now Playing, which needs it for the same
    // reason.
    property int queueRevision: 0
    readonly property var currentItem: {
        const revision = root.queueRevision;
        return App.player.currentQueueIndex >= 0
               ? App.player.queueModel.get(App.player.currentQueueIndex)
               : ({});
    }
    readonly property int currentRating: root.currentItem.rating || 0

    Connections {
        target: App.player.queueModel
        function onDataChanged() { root.queueRevision++ }
        function onModelReset() { root.queueRevision++ }
        function onRowsInserted() { root.queueRevision++ }
        function onRowsRemoved() { root.queueRevision++ }
    }

    function formatTime(milliseconds) {
        const seconds = Math.max(0, Math.floor(milliseconds / 1000));
        const minutes = Math.floor(seconds / 60);
        return minutes + ":" + String(seconds % 60).padStart(2, "0");
    }

    position: QQC2.ToolBar.Footer
    // The progress line has to reach both window edges, so the padding lives on
    // the controls row rather than on the bar.
    padding: 0

    contentItem: ColumnLayout {
        spacing: 0

        EdgeSeekBar { Layout.fillWidth: true }

        // Anchored rather than laid out in a row: Qt Quick Layouts hand out
        // spare width in proportion to what each item asks for, so the track
        // zone kept swallowing it and pushed the transport off-centre. Pinning
        // the transport to the middle of the bar makes its position depend on
        // the window alone, never on the length of a track title.
        Item {
            id: controls
            Layout.fillWidth: true
            Layout.leftMargin: Kirigami.Units.largeSpacing
            Layout.rightMargin: Kirigami.Units.largeSpacing
            // The progress line sits above this row and is smallSpacing tall,
            // so the bottom margin carries that much extra. That puts the
            // transport on the true vertical centre of the whole bar rather
            // than the centre of the space underneath the line.
            Layout.topMargin: Kirigami.Units.smallSpacing
            Layout.bottomMargin: Kirigami.Units.smallSpacing * 2
            implicitHeight: Math.max(nowPlaying.implicitHeight,
                                     transport.implicitHeight,
                                     secondary.implicitHeight)

            // Now playing ---------------------------------------------------
            RowLayout {
                id: nowPlaying
                spacing: Kirigami.Units.largeSpacing
                anchors.left: parent.left
                anchors.right: transport.left
                anchors.rightMargin: Kirigami.Units.largeSpacing
                anchors.verticalCenter: parent.verticalCenter

                ArtworkImage {
                    source: App.player.artwork
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 2.2
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 2.2
                    Layout.alignment: Qt.AlignVCenter
                }

                ColumnLayout {
                    spacing: 0
                    Layout.fillWidth: true
                    Layout.alignment: Qt.AlignVCenter

                    QQC2.Label {
                        text: App.player.title
                        elide: Text.ElideRight
                        font.weight: Font.Medium
                        Layout.fillWidth: true
                    }
                    QQC2.Label {
                        text: App.player.artist
                        elide: Text.ElideRight
                        font: Kirigami.Theme.smallFont
                        color: Kirigami.Theme.disabledTextColor
                        Layout.fillWidth: true
                    }

                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                    TapHandler {
                        onTapped: applicationWindow().navigate("now-playing")
                    }
                }
            }

            // Transport -----------------------------------------------------
            RowLayout {
                id: transport
                spacing: 0
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.verticalCenter: parent.verticalCenter

                // The progress line carries no numbers, so the times flank the
                // transport. Both reserve the same width, so the buttons do not
                // shift sideways when the elapsed time gains a digit.
                QQC2.Label {
                    visible: !root.compact
                    text: root.formatTime(App.player.positionMs)
                    color: Kirigami.Theme.disabledTextColor
                    font.features: { "tnum": 1 }
                    horizontalAlignment: Text.AlignRight
                    Layout.preferredWidth: durationLabel.implicitWidth
                    Layout.rightMargin: Kirigami.Units.largeSpacing
                }
                QQC2.ToolButton {
                    visible: !root.compact
                    text: i18n("Shuffle")
                    icon.name: "media-playlist-shuffle"
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: App.player.shuffle
                    onClicked: App.player.shuffle = checked
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.ToolButton {
                    visible: !root.compact
                    text: i18n("Previous")
                    icon.name: "media-skip-backward"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: App.player.previous()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.RoundButton {
                    text: App.player.playing ? i18n("Pause") : i18n("Play")
                    icon.name: App.player.playing ? "media-playback-pause"
                                                  : "media-playback-start"
                    display: QQC2.AbstractButton.IconOnly
                    highlighted: true
                    implicitWidth: Kirigami.Units.gridUnit * 2.2
                    implicitHeight: implicitWidth
                    onClicked: App.player.playPause()
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                    Layout.rightMargin: Kirigami.Units.smallSpacing
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text

                    QQC2.BusyIndicator {
                        anchors.centerIn: parent
                        width: parent.width + Kirigami.Units.smallSpacing * 2
                        height: width
                        running: App.player.busy
                        visible: running
                    }
                }
                QQC2.ToolButton {
                    visible: !root.compact
                    text: i18n("Next")
                    icon.name: "media-skip-forward"
                    display: QQC2.AbstractButton.IconOnly
                    onClicked: App.player.next()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.ToolButton {
                    visible: !root.compact
                    text: App.player.repeatMode === "one" ? i18n("Repeat Track")
                                                          : i18n("Repeat")
                    icon.name: App.player.repeatMode === "one"
                               ? "media-playlist-repeat-song"
                               : "media-playlist-repeat"
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: App.player.repeatMode !== "none"
                    onClicked: App.player.cycleRepeat()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.Label {
                    id: durationLabel
                    visible: !root.compact
                    text: root.formatTime(App.player.durationMs)
                    color: Kirigami.Theme.disabledTextColor
                    font.features: { "tnum": 1 }
                    Layout.leftMargin: Kirigami.Units.largeSpacing
                }
            }

            // Secondary -----------------------------------------------------
            RowLayout {
                id: secondary
                spacing: 0

                property real volumeBeforeMute: 1
                // Room for every button, the slider itself, and a margin on
                // top, so it appears only when it is not pressed up against
                // the transport.
                readonly property bool hasRoomForVolume:
                    width > loveButton.implicitWidth + dislikeButton.implicitWidth
                            + queueButton.implicitWidth + volumeButton.implicitWidth
                            + Kirigami.Units.gridUnit * 8
                anchors.left: transport.right
                anchors.leftMargin: Kirigami.Units.largeSpacing
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter

                Item { Layout.fillWidth: true }

                // Rating what is playing, without leaving whatever page you
                // are on.
                QQC2.ToolButton {
                    id: loveButton
                    visible: !root.compact
                    text: root.currentRating > 0 ? i18n("Loved") : i18n("Love")
                    icon.name: "love"
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: root.currentRating > 0
                    enabled: App.player.currentId.length > 0
                    onClicked: App.setRating(App.player.currentId, "songs",
                                             root.currentRating > 0 ? 0 : 1)
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.ToolButton {
                    id: dislikeButton
                    visible: !root.compact
                    text: root.currentRating < 0 ? i18n("Disliked") : i18n("Dislike")
                    icon.name: "dialog-cancel"
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: root.currentRating < 0
                    enabled: App.player.currentId.length > 0
                    onClicked: App.setRating(App.player.currentId, "songs",
                                             root.currentRating < 0 ? 0 : -1)
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.ToolButton {
                    id: queueButton
                    visible: !root.compact
                    text: i18n("Queue")
                    icon.name: "media-playlist-append"
                    display: QQC2.AbstractButton.IconOnly
                    checkable: true
                    checked: applicationWindow().currentPage === "queue"
                    onClicked: applicationWindow().navigate("queue")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: text
                }
                QQC2.ToolButton {
                    id: volumeButton
                    visible: !root.compact
                    text: i18n("Volume")
                    icon.name: App.player.volume === 0 ? "audio-volume-muted"
                               : App.player.volume < 0.5 ? "audio-volume-low"
                                                         : "audio-volume-high"
                    display: QQC2.AbstractButton.IconOnly
                    // With the slider already on show there is nothing for a
                    // popup to reveal, so the icon becomes a mute toggle.
                    onClicked: {
                        if (!volumeSlider.visible) return volumePopup.open();
                        if (App.player.volume > 0) {
                            secondary.volumeBeforeMute = App.player.volume;
                            App.player.volume = 0;
                        } else {
                            App.player.volume = secondary.volumeBeforeMute;
                        }
                    }
                    QQC2.ToolTip.visible: hovered && !volumePopup.opened
                    QQC2.ToolTip.text: !volumeSlider.visible ? text
                                       : App.player.volume > 0 ? i18n("Mute")
                                                               : i18n("Unmute")

                    QQC2.Popup {
                        id: volumePopup
                        y: -height - Kirigami.Units.smallSpacing
                        x: volumeButton.width - width
                        padding: Kirigami.Units.largeSpacing
                        QQC2.Slider {
                            from: 0
                            to: 1
                            value: App.player.volume
                            onMoved: App.player.volume = value
                        }
                    }
                }
                // Unfolds out of the button once the bar is wide enough to
                // hold it without crowding the transport. The zone's width
                // comes from the anchors, not from this slider, so showing it
                // cannot feed back into the test that decides to show it.
                QQC2.Slider {
                    id: volumeSlider
                    from: 0
                    to: 1
                    value: App.player.volume
                    onMoved: App.player.volume = value
                    visible: !root.compact && secondary.hasRoomForVolume
                    Layout.preferredWidth: Kirigami.Units.gridUnit * 6
                    Layout.leftMargin: Kirigami.Units.smallSpacing
                }
            }
        }
    }
}
