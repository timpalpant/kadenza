import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Listen Now")

    actions: [
        Kirigami.Action {
            visible: !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: App.refreshHome()
        }
    ]

    Component.onCompleted: App.refreshHome()

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        // Apple's own "Made for You" shelves. The set and their titles come
        // from the API, so the number of sections here is not fixed.
        Repeater {
            model: App.recommendations
            delegate: ColumnLayout {
                required property var modelData
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Kirigami.Heading { text: modelData.title; level: 2 }
                MediaGrid {
                    model: modelData.model
                    interactive: false
                    Layout.fillWidth: true
                    Layout.preferredHeight: fullHeight
                }
            }
        }

        Kirigami.Heading {
            text: i18n("Recently Played")
            level: 2
            visible: App.recentModel.count > 0
        }
        MediaGrid {
            model: App.recentModel
            interactive: false
            visible: App.recentModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.Heading {
            text: i18n("Recently Played Songs")
            level: 2
            visible: App.recentTracksModel.count > 0
        }
        ListView {
            model: App.recentTracksModel
            interactive: false
            reuseItems: true
            visible: App.recentTracksModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            delegate: MediaDelegate {
                onPlayRequested: App.playModel(App.recentTracksModel, index)
                onOpenRequested: {}
            }
        }

        Kirigami.Heading {
            text: i18n("Heavy Rotation")
            level: 2
            visible: App.heavyRotationModel.count > 0
        }
        MediaGrid {
            model: App.heavyRotationModel
            interactive: false
            visible: App.heavyRotationModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        QQC2.BusyIndicator {
            visible: App.loading && App.recentModel.count === 0
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
