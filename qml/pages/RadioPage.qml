import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Radio")

    actions: [
        Kirigami.Action {
            visible: !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: App.loadStations(true)
        }
    ]

    Component.onCompleted: App.loadStations()

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

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
            visible: !App.stationsModel.loading && App.stationsModel.count === 0
            text: App.stationsModel.error.length > 0
                  ? App.stationsModel.error
                  : i18n("No stations yet")
            explanation: i18n("Stations you listen to in Apple Music appear here.")
            icon.name: "radio"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        QQC2.BusyIndicator {
            visible: App.stationsModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
