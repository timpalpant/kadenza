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

        DetailTrackList {
            model: App.detailTracksModel
            artistPage: page.artistPage
            collectionShelfPage: page.collectionShelfPage
        }

        ArtistDetail { visible: page.artistPage }

        QQC2.BusyIndicator {
            visible: App.detailTracksModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
