import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// A fixed-height page rather than a ScrollablePage: the artwork column and the
// side panel each own their share of the viewport, and the lyric and queue
// lists scroll inside themselves instead of stretching one giant flickable.
Kirigami.Page {
    id: page
    title: i18n("Now Playing")
    padding: Kirigami.Units.largeSpacing

    readonly property bool tabbed: width < Kirigami.Units.gridUnit * 48
    // Only meaningful in the wide layout; the narrow one already puts lyrics
    // behind their own tab.
    property bool lyricsExpanded: true
    property int queueRevision: 0
    readonly property var currentItem: currentQueueItem()

    function currentQueueItem() {
        const revision = queueRevision;
        if (revision >= 0 && App.player.currentQueueIndex >= 0)
            return App.player.queueModel.get(App.player.currentQueueIndex);
        return {};
    }

    Connections {
        target: App.player.queueModel
        function onDataChanged() { page.queueRevision++ }
        function onModelReset() { page.queueRevision++ }
        function onRowsInserted() { page.queueRevision++ }
        function onRowsRemoved() { page.queueRevision++ }
    }

    // A dim wash of the current artwork behind the page. Deliberately plain
    // Image + scrim rather than a shader blur, which did not render reliably.
    background: Item {
        Image {
            anchors.fill: parent
            source: App.player.artwork
            fillMode: Image.PreserveAspectCrop
            asynchronous: true
            opacity: 0.10
        }
        Rectangle {
            anchors.fill: parent
            color: Kirigami.Theme.backgroundColor
            opacity: 0.80
        }
    }

    ColumnLayout {
        anchors.fill: parent
        spacing: Kirigami.Units.largeSpacing

        QQC2.TabBar {
            id: tabs
            visible: page.tabbed
            Layout.fillWidth: true
            QQC2.TabButton { text: i18n("Artwork") }
            QQC2.TabButton { text: i18n("Lyrics") }
            QQC2.TabButton { text: i18n("Up Next") }
        }

        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: Kirigami.Units.gridUnit * 2

            // Artwork, metadata and transport -----------------------------
            ColumnLayout {
                visible: !page.tabbed || tabs.currentIndex === 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.minimumWidth: Kirigami.Units.gridUnit * 14
                spacing: Kirigami.Units.largeSpacing

                // Absorbs the slack in the column so the artwork is as large
                // as the window allows instead of a fixed 18 grid units.
                Item {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.minimumHeight: Kirigami.Units.gridUnit * 6

                    ArtworkImage {
                        id: heroArtwork
                        anchors.centerIn: parent
                        width: Math.max(Kirigami.Units.gridUnit * 6,
                                        Math.min(parent.width, parent.height,
                                                 Kirigami.Units.gridUnit * 24))
                        height: width
                        source: App.player.artwork
                        fallbackIcon: "media-album-cover"
                        radius: Kirigami.Units.largeSpacing
                    }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    spacing: 0

                    Kirigami.Heading {
                        text: App.player.title.length > 0 ? App.player.title
                                                          : i18n("Nothing Playing")
                        level: 1
                        horizontalAlignment: Text.AlignHCenter
                        elide: Text.ElideRight
                        maximumLineCount: 2
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    Kirigami.LinkButton {
                        text: App.player.artist
                        visible: text.length > 0
                        enabled: page.currentItem.artistId
                                 && page.currentItem.artistId.length > 0
                        elide: Text.ElideRight
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: parent.width
                        onClicked: applicationWindow().openArtist(
                                       page.currentItem.artistId, App.player.artist)
                    }
                    Kirigami.LinkButton {
                        text: App.player.album
                        visible: text.length > 0
                        enabled: page.currentItem.albumId
                                 && page.currentItem.albumId.length > 0
                        font: Kirigami.Theme.smallFont
                        elide: Text.ElideRight
                        Layout.alignment: Qt.AlignHCenter
                        Layout.maximumWidth: parent.width
                        onClicked: applicationWindow().openAlbum(
                                       page.currentItem.albumId, App.player.album,
                                       App.player.artist, App.player.artwork)
                    }
                }

                SeekSlider { Layout.fillWidth: true }

                RowLayout {
                    spacing: 0
                    Layout.alignment: Qt.AlignHCenter

                    QQC2.ToolButton {
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
                        implicitWidth: Kirigami.Units.gridUnit * 3
                        implicitHeight: implicitWidth
                        icon.width: Kirigami.Units.iconSizes.smallMedium
                        icon.height: Kirigami.Units.iconSizes.smallMedium
                        onClicked: App.player.playPause()
                        Layout.leftMargin: Kirigami.Units.largeSpacing
                        Layout.rightMargin: Kirigami.Units.largeSpacing
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
                        text: i18n("Next")
                        icon.name: "media-skip-forward"
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: App.player.next()
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                    }
                    QQC2.ToolButton {
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
                }

                RowLayout {
                    spacing: Kirigami.Units.largeSpacing
                    Layout.alignment: Qt.AlignHCenter

                    QQC2.ToolButton {
                        text: page.currentItem.favorite ? i18n("Favorited")
                                                        : i18n("Favorite")
                        icon.name: page.currentItem.favorite ? "rating"
                                                             : "rating-unrated"
                        display: QQC2.AbstractButton.TextBesideIcon
                        enabled: App.player.currentId.length > 0
                        onClicked: App.setFavorite(App.player.currentId, "songs",
                                                   !page.currentItem.favorite)
                    }
                    QQC2.ToolButton {
                        readonly property int rating: page.currentItem.rating || 0
                        text: rating > 0 ? i18n("Loved") : i18n("Love")
                        icon.name: "love"
                        display: QQC2.AbstractButton.TextBesideIcon
                        checkable: true
                        checked: rating > 0
                        enabled: App.player.currentId.length > 0
                        onClicked: App.setRating(App.player.currentId, "songs",
                                                 rating > 0 ? 0 : 1)
                    }
                    QQC2.ToolButton {
                        text: page.currentItem.inLibrary ? i18n("In Library")
                                                         : i18n("Add to Library")
                        icon.name: page.currentItem.inLibrary ? "checkmark"
                                                              : "list-add"
                        display: QQC2.AbstractButton.TextBesideIcon
                        enabled: App.player.currentId.length > 0
                        onClicked: App.setInLibrary(
                                       page.currentItem.inLibrary
                                       ? page.currentItem.mediaId
                                       : App.player.currentId,
                                       "songs", !page.currentItem.inLibrary)
                    }
                }

                // Kept narrow and centred: a full-width volume slider read as
                // a second, broken progress bar.
                RowLayout {
                    spacing: Kirigami.Units.smallSpacing
                    Layout.alignment: Qt.AlignHCenter
                    Layout.bottomMargin: Kirigami.Units.smallSpacing

                    Kirigami.Icon {
                        source: App.player.volume === 0 ? "audio-volume-muted"
                                : App.player.volume < 0.5 ? "audio-volume-low"
                                                          : "audio-volume-high"
                        implicitWidth: Kirigami.Units.iconSizes.smallMedium
                        implicitHeight: Kirigami.Units.iconSizes.smallMedium
                    }
                    QQC2.Slider {
                        from: 0
                        to: 1
                        value: App.player.volume
                        onMoved: App.player.volume = value
                        Layout.preferredWidth: Kirigami.Units.gridUnit * 10
                    }
                }
            }

            Kirigami.Separator {
                visible: !page.tabbed
                Layout.fillHeight: true
            }

            // Lyrics and Up Next ------------------------------------------
            ColumnLayout {
                id: sidePanel
                visible: !page.tabbed || tabs.currentIndex > 0
                Layout.fillHeight: true
                Layout.fillWidth: page.tabbed
                Layout.preferredWidth: page.tabbed
                                       ? -1
                                       : Kirigami.Units.gridUnit * 18
                Layout.minimumWidth: page.tabbed
                                     ? 0
                                     : Kirigami.Units.gridUnit * 14
                spacing: Kirigami.Units.smallSpacing

                RowLayout {
                    visible: !page.tabbed
                    Layout.fillWidth: true
                    spacing: Kirigami.Units.smallSpacing

                    Kirigami.Heading {
                        text: i18n("Lyrics")
                        level: 2
                        Layout.fillWidth: true
                    }
                    QQC2.ToolButton {
                        text: page.lyricsExpanded ? i18n("Collapse Lyrics")
                                                  : i18n("Expand Lyrics")
                        icon.name: page.lyricsExpanded ? "arrow-up" : "arrow-down"
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: page.lyricsExpanded = !page.lyricsExpanded
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                    }
                    // The whole header is the target, not just the button.
                    TapHandler {
                        onTapped: page.lyricsExpanded = !page.lyricsExpanded
                    }
                    HoverHandler { cursorShape: Qt.PointingHandCursor }
                }

                QQC2.ScrollView {
                    // Collapsing hides it from the layout entirely, so Up Next
                    // takes the freed height rather than leaving a gap.
                    visible: page.tabbed ? tabs.currentIndex === 1
                                         : page.lyricsExpanded
                    clip: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 18

                    ListView {
                        id: lyricsList
                        readonly property int activeLine: {
                            let active = -1;
                            for (let i = 0; i < App.player.lyrics.length; ++i) {
                                if (App.player.lyrics[i].timeMs <= App.player.positionMs)
                                    active = i;
                                else break;
                            }
                            return active;
                        }
                        model: App.player.lyrics
                        currentIndex: activeLine
                        spacing: Kirigami.Units.smallSpacing

                        delegate: QQC2.Label {
                            required property var modelData
                            required property int index
                            readonly property bool active: index === lyricsList.activeLine

                            width: lyricsList.width
                            text: modelData.text
                            wrapMode: Text.Wrap
                            horizontalAlignment: Text.AlignLeft
                            color: active ? Kirigami.Theme.highlightColor
                                          : Kirigami.Theme.textColor
                            opacity: active ? 1 : 0.6
                            font.weight: active ? Font.DemiBold : Font.Normal
                            padding: Kirigami.Units.smallSpacing
                            Behavior on opacity {
                                NumberAnimation { duration: Kirigami.Units.shortDuration }
                            }

                            // Synchronized lyrics double as a chapter list.
                            TapHandler {
                                enabled: App.player.synchronizedLyrics
                                onTapped: App.player.seek(modelData.timeMs)
                            }
                            HoverHandler {
                                enabled: App.player.synchronizedLyrics
                                cursorShape: Qt.PointingHandCursor
                            }
                        }

                        onCurrentIndexChanged: if (currentIndex >= 0)
                            positionViewAtIndex(currentIndex, ListView.Center)

                        Kirigami.PlaceholderMessage {
                            anchors.centerIn: parent
                            width: parent.width - Kirigami.Units.gridUnit * 2
                            visible: App.player.lyrics.length === 0
                            text: App.player.lyricsStatus.length > 0
                                  ? App.player.lyricsStatus
                                  : i18n("No lyrics available")
                            icon.name: "view-media-lyrics"
                        }
                    }
                }

                Kirigami.Separator {
                    visible: !page.tabbed
                    Layout.fillWidth: true
                }

                Kirigami.Heading {
                    text: i18n("Up Next")
                    level: 2
                    visible: !page.tabbed
                    Layout.fillWidth: true
                }

                QQC2.ScrollView {
                    visible: !page.tabbed || tabs.currentIndex === 2
                    clip: true
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    Layout.preferredHeight: Kirigami.Units.gridUnit * 12

                    ListView {
                        id: queueList
                        model: App.player.queueModel
                        currentIndex: App.player.currentQueueIndex
                        reuseItems: true

                        delegate: MediaDelegate {
                            queueMode: true
                            forceHighlighted: index === App.player.currentQueueIndex
                            past: index < App.player.currentQueueIndex
                            hidePast: true
                            compact: true
                            onPlayRequested: App.player.playQueueIndex(index)
                            onOpenRequested: {}
                        }

                        onCurrentIndexChanged: if (currentIndex >= 0)
                            positionViewAtIndex(currentIndex, ListView.Contain)

                        Kirigami.PlaceholderMessage {
                            anchors.centerIn: parent
                            width: parent.width - Kirigami.Units.gridUnit * 2
                            visible: App.player.queueModel.count === 0
                            text: i18n("The queue is empty")
                            icon.name: "media-playlist-append"
                        }
                    }
                }
            }
        }
    }
}
