import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Replay")

    actions: [
        Kirigami.Action {
            visible: !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: App.loadReplay(true)
        }
    ]

    Component.onCompleted: App.loadReplay()

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            text: i18n("Top Artists")
            level: 2
            visible: App.replayTopArtistsModel.count > 0
        }
        MediaGrid {
            model: App.replayTopArtistsModel
            sizeScale: 0.75
            interactive: false
            visible: App.replayTopArtistsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.Heading {
            text: i18n("Top Albums")
            level: 2
            visible: App.replayTopAlbumsModel.count > 0
        }
        MediaGrid {
            model: App.replayTopAlbumsModel
            interactive: false
            visible: App.replayTopAlbumsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.Heading {
            text: i18n("Top Songs")
            level: 2
            visible: App.replayTopSongsModel.count > 0
        }
        ListView {
            model: App.replayTopSongsModel
            interactive: false
            reuseItems: true
            visible: App.replayTopSongsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            delegate: MediaDelegate {
                onPlayRequested: App.playModel(App.replayTopSongsModel, index)
                onOpenRequested: {}
            }
        }

        Kirigami.PlaceholderMessage {
            visible: !App.replayTopSongsModel.loading
                     && App.replayTopSongsModel.count === 0
            text: App.replayTopSongsModel.error.length > 0
                  ? App.replayTopSongsModel.error
                  : i18n("Not enough listening history yet for a Replay summary")
            icon.name: "view-calendar-year"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        QQC2.BusyIndicator {
            visible: App.replayTopSongsModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
