import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.Page {
    id: page
    title: i18n("Sign in")

    ColumnLayout {
        anchors.centerIn: parent
        width: Math.min(parent.width - Kirigami.Units.gridUnit * 4, Kirigami.Units.gridUnit * 25)
        spacing: Kirigami.Units.largeSpacing

        Kirigami.Icon {
            source: Qt.resolvedUrl("../../data/icons/io.github.timpalpant.kadenza.svg")
            Layout.alignment: Qt.AlignHCenter
            Layout.preferredWidth: Kirigami.Units.iconSizes.huge
            Layout.preferredHeight: Kirigami.Units.iconSizes.huge
        }
        Kirigami.Heading {
            text: i18n("Kadenza")
            level: 1
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        QQC2.Label {
            text: i18n("Apple Music for KDE Plasma")
            color: Kirigami.Theme.disabledTextColor
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        QQC2.Button {
            text: i18n("Sign in to Apple Music")
            icon.name: "user-identity"
            enabled: App.player.available && App.player.ready
                     && !App.player.restoringSession
            highlighted: true
            onClicked: App.player.signIn()
            Layout.alignment: Qt.AlignHCenter
        }
        QQC2.Label {
            text: i18n("Restoring your Apple Music session…")
            visible: App.player.restoringSession
            color: Kirigami.Theme.disabledTextColor
            horizontalAlignment: Text.AlignHCenter
            Layout.fillWidth: true
        }
        QQC2.BusyIndicator {
            visible: (!App.player.ready || App.player.restoringSession)
                     && App.player.error.length === 0
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
        Kirigami.InlineMessage {
            visible: App.player.error.length > 0
            text: App.player.error
            type: Kirigami.MessageType.Error
            Layout.fillWidth: true
        }
    }
}
