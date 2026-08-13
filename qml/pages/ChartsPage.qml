import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Charts")

    property string selectedGenre: ""

    actions: [
        Kirigami.Action {
            visible: !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: App.loadCharts(true, page.selectedGenre)
        }
    ]

    Component.onCompleted: {
        App.loadCharts();
        App.loadGenres();
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        GenreFilter {
            Layout.fillWidth: true
            model: App.genres
            selectedId: page.selectedGenre
            onGenreSelected: id => {
                page.selectedGenre = id;
                App.loadCharts(true, id);
            }
        }

        Kirigami.Heading {
            text: i18n("Top Songs")
            level: 2
            visible: App.chartSongsModel.count > 0
        }
        ListView {
            model: App.chartSongsModel
            interactive: false
            reuseItems: true
            visible: App.chartSongsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            delegate: MediaDelegate {
                onPlayRequested: App.playModel(App.chartSongsModel, index)
                onOpenRequested: {}
            }
        }

        Kirigami.Heading {
            text: i18n("Top Albums")
            level: 2
            visible: App.chartAlbumsModel.count > 0
        }
        MediaGrid {
            model: App.chartAlbumsModel
            interactive: false
            visible: App.chartAlbumsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.Heading {
            text: i18n("Top Playlists")
            level: 2
            visible: App.chartPlaylistsModel.count > 0
        }
        MediaGrid {
            model: App.chartPlaylistsModel
            interactive: false
            visible: App.chartPlaylistsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.PlaceholderMessage {
            visible: !App.chartSongsModel.loading
                     && App.chartSongsModel.count === 0
            text: App.chartSongsModel.error.length > 0
                  ? App.chartSongsModel.error
                  : i18n("Charts are not available right now")
            icon.name: "office-chart-bar"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        QQC2.BusyIndicator {
            visible: App.chartSongsModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
