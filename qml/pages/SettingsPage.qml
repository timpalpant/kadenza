import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Settings")

    // Kirigami.FormLayout gives the aligned label/control columns and section
    // headers every other KDE settings page uses, and it collapses to a single
    // column on narrow windows on its own.
    Kirigami.FormLayout {
        width: page.availableWidth

        Item {
            Kirigami.FormData.label: i18n("Playback Helper")
            Kirigami.FormData.isSection: true
        }

        RowLayout {
            Kirigami.FormData.label: i18n("Status:")
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                source: App.player.ready ? "dialog-ok" : "dialog-warning"
                implicitWidth: Kirigami.Units.iconSizes.small
                implicitHeight: Kirigami.Units.iconSizes.small
            }
            QQC2.Label {
                text: App.player.ready ? i18n("Running")
                      : App.player.available ? i18n("Starting…")
                                             : i18n("Not running")
            }
            QQC2.Button {
                text: i18n("Retry")
                icon.name: "system-reboot"
                visible: !App.player.ready
                onClicked: App.player.restartSidecar()
            }
        }
        QQC2.Label {
            text: i18n("Playback runs in a separate Widevine-enabled Electron process. Kanzi restarts it automatically a few times; use Retry after fixing the underlying problem.")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.Wrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        }

        Item {
            Kirigami.FormData.label: i18n("Shortcuts")
            Kirigami.FormData.isSection: true
        }

        QQC2.Label {
            Kirigami.FormData.label: i18n("Media keys:")
            text: i18n("Play/Pause, Next and Previous control Kanzi.")
            wrapMode: Text.Wrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        }
        QQC2.Label {
            Kirigami.FormData.label: i18n("Show Kanzi:")
            text: i18nc("Keyboard shortcut", "Meta+Alt+K")
            font.family: "monospace"
        }
        QQC2.Label {
            text: i18n("Both can be reassigned in System Settings under Shortcuts.")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.Wrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        }

        Item {
            Kirigami.FormData.label: i18n("Account")
            Kirigami.FormData.isSection: true
        }

        QQC2.Button {
            Kirigami.FormData.label: i18n("Apple Music:")
            text: i18n("Sign Out")
            icon.name: "system-log-out"
            onClicked: signOutPrompt.open()
        }
        QQC2.Label {
            text: i18n("Signing out removes Apple Music cookies and local web storage from Kanzi's isolated playback profile.")
            font: Kirigami.Theme.smallFont
            color: Kirigami.Theme.disabledTextColor
            wrapMode: Text.Wrap
            Layout.maximumWidth: Kirigami.Units.gridUnit * 24
        }
    }

    // Signing out throws away the Electron profile, so it should not happen on
    // a single stray click.
    Kirigami.PromptDialog {
        id: signOutPrompt
        title: i18n("Sign Out of Apple Music?")
        subtitle: i18n("Kanzi will forget this Apple Music session and stop playback. You can sign in again at any time.")
        standardButtons: Kirigami.Dialog.Cancel
        customFooterActions: [
            Kirigami.Action {
                text: i18n("Sign Out")
                icon.name: "system-log-out"
                onTriggered: {
                    App.player.signOut();
                    signOutPrompt.close();
                }
            }
        ]
    }
}
