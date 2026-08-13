import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: App.detailTitle
    readonly property bool artistPage: App.detailType === "artists" || App.detailType === "library-artists"
    // Curators, Apple Curators, Activities and Record Labels have no track
    // list of their own — each is just a header plus a grid of the playlists
    // or albums found inside it, so they share one simple layout distinct
    // from both the artist page's many shelves and the plain track list.
    readonly property bool collectionShelfPage: ["curators", "apple-curators",
                                                 "activities", "record-labels"]
                                                 .indexOf(App.detailType) >= 0

    actions: [
        Kirigami.Action {
            visible: (page.artistPage || page.collectionShelfPage) && !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        }
    ]

    // A titled row of tiles, used for each of the artist views.
    component ArtistShelf: ColumnLayout {
        id: shelf
        required property string heading
        required property var shelfModel

        spacing: Kirigami.Units.smallSpacing
        Layout.fillWidth: true

        Kirigami.Heading { text: shelf.heading; level: 2 }
        MediaGrid {
            model: shelf.shelfModel
            interactive: false
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        GridLayout {
            columns: applicationWindow().compactMode ? 1 : 2
            rowSpacing: Kirigami.Units.largeSpacing
            columnSpacing: Kirigami.Units.largeSpacing
            Layout.fillWidth: true
            ArtworkImage {
                source: App.detailArtwork
                fallbackIcon: page.artistPage ? "view-media-artist" : "media-album-cover"
                circular: page.artistPage
                Layout.preferredWidth: Kirigami.Units.gridUnit * (page.artistPage ? 7 : 10)
                Layout.preferredHeight: Layout.preferredWidth
                Layout.alignment: applicationWindow().compactMode ? Qt.AlignHCenter : Qt.AlignTop
            }
            ColumnLayout {
                Layout.fillWidth: true
                Kirigami.Heading { text: App.detailTitle; level: 1; wrapMode: Text.Wrap; Layout.fillWidth: true }
                LinkLabel {
                    text: App.detailSubtitle
                    visible: text.length > 0
                    // The header itself has only a name; the tracks sometimes
                    // carry the artist relationship, so borrow the first
                    // track's id when there is one and fall back to the name.
                    linked: !page.artistPage && !page.collectionShelfPage && App.detailSubtitle.length > 0
                    onActivated: applicationWindow().openArtist(
                                     App.detailTracksModel.count > 0
                                     ? App.detailTracksModel.get(0).artistId : "",
                                     App.detailSubtitle)
                    Layout.fillWidth: true
                }
                LinkLabel {
                    text: App.detailCuratorName.length > 0
                          ? i18n("Curated by %1", App.detailCuratorName)
                          : i18n("Curated by another user")
                    visible: !page.collectionShelfPage && App.detailCuratorId.length > 0
                    linked: true
                    onActivated: applicationWindow().openDetail(
                                     App.detailCuratorId, App.detailCuratorId, "curators",
                                     App.detailCuratorName, "", "")
                    Layout.fillWidth: true
                }
                LinkLabel {
                    text: App.detailRecordLabelName.length > 0
                          ? i18n("More from %1", App.detailRecordLabelName)
                          : i18n("More from this label")
                    visible: !page.collectionShelfPage && App.detailRecordLabelId.length > 0
                    linked: true
                    onActivated: applicationWindow().openDetail(
                                     App.detailRecordLabelId, App.detailRecordLabelId, "record-labels",
                                     App.detailRecordLabelName, "", "")
                    Layout.fillWidth: true
                }
                RowLayout {
                    visible: App.detailRatable || App.detailCollectable
                    // Icon-only, with the state carried by the checked look and
                    // spelled out in the tooltip.
                    QQC2.ToolButton {
                        visible: App.detailRatable
                        text: App.detailRating > 0 ? i18n("Loved") : i18n("Love")
                        icon.name: "love"
                        display: QQC2.AbstractButton.IconOnly
                        checkable: true
                        checked: App.detailRating > 0
                        onClicked: App.rateDetail(App.detailRating > 0 ? 0 : 1)
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                    }
                    QQC2.ToolButton {
                        visible: App.detailRatable
                        text: App.detailRating < 0 ? i18n("Disliked") : i18n("Dislike")
                        icon.name: "dialog-cancel"
                        display: QQC2.AbstractButton.IconOnly
                        checkable: true
                        checked: App.detailRating < 0
                        onClicked: App.rateDetail(App.detailRating < 0 ? 0 : -1)
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                    }
                    QQC2.ToolButton {
                        visible: App.detailCollectable
                        text: App.detailInLibrary ? i18n("Remove from Library")
                                                  : i18n("Add to Library")
                        icon.name: App.detailInLibrary ? "list-remove" : "list-add"
                        display: QQC2.AbstractButton.IconOnly
                        onClicked: App.toggleDetailInLibrary()
                        QQC2.ToolTip.visible: hovered
                        QQC2.ToolTip.text: text
                    }
                }
                RowLayout {
                    visible: !page.artistPage && !page.collectionShelfPage
                    QQC2.Button {
                        text: i18n("Play")
                        icon.name: "media-playback-start"
                        highlighted: true
                        enabled: App.detailTracksModel.playableCount > 0
                        onClicked: App.playDetail(0)
                    }
                    QQC2.Button {
                        text: i18n("Shuffle")
                        icon.name: "media-playlist-shuffle"
                        enabled: App.detailTracksModel.playableCount > 0
                        onClicked: {
                            App.player.shuffle = true;
                            App.playDetail(0);
                        }
                    }
                }
                RowLayout {
                    visible: page.artistPage
                    QQC2.Button {
                        text: i18n("Play Top Songs")
                        icon.name: "media-playback-start"
                        highlighted: true
                        enabled: App.artistTopSongsModel.playableCount > 0
                        onClicked: App.playModel(App.artistTopSongsModel, 0)
                    }
                    QQC2.Button {
                        text: i18n("Shuffle")
                        icon.name: "media-playlist-shuffle"
                        enabled: App.artistTopSongsModel.playableCount > 0
                        onClicked: {
                            App.player.shuffle = true;
                            App.playModel(App.artistTopSongsModel, 0);
                        }
                    }
                }
            }
        }

        // Album and playlist tracks -------------------------------------
        ListView {
            model: App.detailTracksModel
            visible: !page.artistPage && !page.collectionShelfPage
            interactive: false
            reuseItems: true
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            delegate: MediaDelegate {
                onPlayRequested: App.playDetail(index)
                onOpenRequested: {}
            }
        }

        // Curator, Apple Curator, Activity and Record Label pages: a grid of
        // the playlists or albums found inside, not a track list.
        MediaGrid {
            model: App.detailTracksModel
            visible: page.collectionShelfPage
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.PlaceholderMessage {
            visible: page.collectionShelfPage && !App.detailTracksModel.loading
                     && App.detailTracksModel.count === 0
            text: i18n("Nothing here yet")
            icon.name: "view-media-playlist"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        // Artist page ----------------------------------------------------
        ArtistShelf {
            heading: i18n("Latest Release")
            shelfModel: App.artistLatestModel
            visible: page.artistPage && App.artistLatestModel.count > 0
        }

        ColumnLayout {
            visible: page.artistPage && App.artistTopSongsModel.count > 0
            spacing: Kirigami.Units.smallSpacing
            Layout.fillWidth: true
            Kirigami.Heading { text: i18n("Top Songs"); level: 2 }
            ListView {
                model: App.artistTopSongsModel
                interactive: false
                reuseItems: true
                Layout.fillWidth: true
                Layout.preferredHeight: contentHeight
                delegate: MediaDelegate {
                    onPlayRequested: App.playModel(App.artistTopSongsModel, index)
                    onOpenRequested: {}
                }
            }
        }

        ArtistShelf {
            heading: i18n("Albums")
            shelfModel: App.artistAlbumsModel
            visible: page.artistPage && App.artistAlbumsModel.count > 0
        }
        ArtistShelf {
            heading: i18n("Singles and EPs")
            shelfModel: App.artistSinglesModel
            visible: page.artistPage && App.artistSinglesModel.count > 0
        }
        ArtistShelf {
            heading: i18n("Similar Artists")
            shelfModel: App.artistSimilarModel
            visible: page.artistPage && App.artistSimilarModel.count > 0
        }

        // Library artists have no catalog counterpart to draw views from, so
        // they still show their plain album relationship.
        MediaGrid {
            model: App.detailTracksModel
            interactive: false
            visible: page.artistPage && App.detailTracksModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        QQC2.BusyIndicator {
            visible: App.detailTracksModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
