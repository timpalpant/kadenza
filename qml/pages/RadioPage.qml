import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Radio")

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
            onTriggered: {
                App.loadStations(true);
                App.loadLiveStations(page.selectedGenre);
                App.loadPersonalStation();
            }
        }
    ]

    Component.onCompleted: {
        App.loadStations();
        App.loadLiveStations();
        App.loadPersonalStation();
        App.loadStationGenres();
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Heading {
            text: i18n("Your Station")
            level: 2
            visible: App.personalStationModel.count > 0
        }
        MediaGrid {
            model: App.personalStationModel
            interactive: false
            visible: App.personalStationModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.Heading {
            text: i18n("Apple Music Radio")
            level: 2
            visible: App.liveStationsModel.count > 0
        }
        Flow {
            visible: App.liveStationsModel.count > 0 && App.stationGenres.length > 0
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            QQC2.Button {
                text: i18n("All Genres")
                checkable: true
                checked: page.selectedGenre.length === 0
                onClicked: {
                    page.selectedGenre = "";
                    App.loadLiveStations();
                }
            }
            Repeater {
                model: App.stationGenres
                QQC2.Button {
                    text: modelData.name
                    checkable: true
                    checked: page.selectedGenre === modelData.id
                    onClicked: {
                        page.selectedGenre = modelData.id;
                        App.loadLiveStations(modelData.id);
                    }
                }
            }
        }
        MediaGrid {
            model: App.liveStationsModel
            interactive: false
            visible: App.liveStationsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.Heading {
            text: i18n("Recently Played Stations")
            level: 2
            visible: App.stationsModel.count > 0
        }
        MediaGrid {
            model: App.stationsModel
            interactive: false
            visible: App.stationsModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.PlaceholderMessage {
            visible: !App.stationsModel.loading && !App.liveStationsModel.loading
                     && App.stationsModel.count === 0 && App.liveStationsModel.count === 0
                     && App.personalStationModel.count === 0
            text: App.stationsModel.error.length > 0
                  ? App.stationsModel.error
                  : i18n("No stations yet")
            explanation: i18n("Stations you listen to in Apple Music appear here.")
            icon.name: "radio"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        QQC2.BusyIndicator {
            visible: App.stationsModel.loading || App.liveStationsModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
