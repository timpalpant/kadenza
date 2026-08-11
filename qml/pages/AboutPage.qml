import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Everything shown here still comes from the KAboutData built in main.cpp, so
// the page cannot drift out of sync with the metadata the rest of KDE reads.
//
// Kirigami.AboutPage renders that data for us, but it pours every field into a
// single FormLayout: entries carrying a label land in the label column while
// the homepage link and the licence name are centred in the value column, so
// the page ends up with three competing alignments and an uneven rhythm. Laying
// it out here keeps one left edge and one spacing unit throughout.
Kirigami.ScrollablePage {
    id: page
    title: i18n("About %1", AboutData.displayName)

    // Almost always a single licence; joining avoids a repeater that would
    // have to emit two cells per row into the grid below.
    readonly property string licenseNames:
        AboutData.licenses.map(license => license.name).join(", ")

    readonly property string slipmatUrl: "https://github.com/SoftARV/Slipmat"

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        // Identity ---------------------------------------------------------
        RowLayout {
            spacing: Kirigami.Units.largeSpacing * 2
            Layout.fillWidth: true
            Layout.bottomMargin: Kirigami.Units.largeSpacing

            Kirigami.Icon {
                source: AboutData.programLogo
                        || Kirigami.Settings.applicationWindowIcon
                        || AboutData.desktopFileName
                implicitWidth: Kirigami.Units.iconSizes.enormous
                implicitHeight: implicitWidth
                Layout.alignment: Qt.AlignTop
            }
            ColumnLayout {
                spacing: Kirigami.Units.smallSpacing
                Layout.fillWidth: true

                Kirigami.Heading {
                    text: AboutData.displayName
                    level: 1
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: AboutData.shortDescription
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                QQC2.Label {
                    text: AboutData.copyrightStatement
                    visible: text.length > 0
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // Metadata ---------------------------------------------------------
        // A plain two-column grid rather than a FormLayout: FormLayout centres
        // its label/value pair in the available width, which floated this block
        // into the middle of the page while every other section stayed against
        // the left edge.
        GridLayout {
            columns: 2
            columnSpacing: Kirigami.Units.largeSpacing
            rowSpacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                text: i18n("Version:")
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.Label { text: AboutData.version }

            QQC2.Label {
                text: i18n("License:")
                color: Kirigami.Theme.disabledTextColor
            }
            QQC2.Label { text: page.licenseNames }

            QQC2.Label {
                text: i18n("Website:")
                color: Kirigami.Theme.disabledTextColor
                visible: homepageLink.visible
            }
            Kirigami.UrlButton {
                id: homepageLink
                url: AboutData.homepage
                visible: url.length > 0
            }

            QQC2.Label {
                text: i18n("Report a bug:")
                color: Kirigami.Theme.disabledTextColor
                visible: bugLink.visible
            }
            Kirigami.UrlButton {
                id: bugLink
                url: AboutData.bugAddress
                visible: url.length > 0
            }
        }

        Kirigami.Separator { Layout.fillWidth: true }

        // Disclaimer -------------------------------------------------------
        QQC2.Label {
            // KAboutData carries this as plain text, because everything else
            // that reads it — the command line, the crash handler — wants it
            // plain. The project name is turned into a link here rather than
            // there, so only the rendering knows about markup.
            text: AboutData.otherText
                  .replace(/\n\n/g, "<br/><br/>")
                  .replace("Slipmat",
                           "<a href=\"" + page.slipmatUrl + "\">Slipmat</a>")
            textFormat: Text.StyledText
            visible: text.length > 0
            wrapMode: Text.Wrap
            linkColor: Kirigami.Theme.linkColor
            onLinkActivated: link => Qt.openUrlExternally(link)
            Layout.fillWidth: true
            // Long prose is hard to read across a maximised window.
            Layout.maximumWidth: Kirigami.Units.gridUnit * 34

            HoverHandler {
                enabled: parent.hoveredLink.length > 0
                cursorShape: Qt.PointingHandCursor
            }
        }

        // Libraries --------------------------------------------------------
        Kirigami.Heading {
            text: i18n("Libraries in use")
            level: 3
            Layout.fillWidth: true
            Layout.topMargin: Kirigami.Units.largeSpacing
        }
        Repeater {
            model: Kirigami.Settings.information
            delegate: QQC2.Label {
                required property string modelData
                text: modelData
                wrapMode: Text.Wrap
                Layout.fillWidth: true
            }
        }
        Repeater {
            model: AboutData.components
            delegate: ColumnLayout {
                required property var modelData
                spacing: 0
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.smallSpacing

                QQC2.Label {
                    text: modelData.version.length === 0
                          ? modelData.name
                          : i18nc("Component name and version", "%1 %2",
                                  modelData.name, modelData.version)
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                // Kirigami drops the description entirely, which left the
                // entry reading as a bare product name with no explanation of
                // why Kanzi ships it.
                QQC2.Label {
                    text: modelData.description
                    visible: text.length > 0
                    font: Kirigami.Theme.smallFont
                    color: Kirigami.Theme.disabledTextColor
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }

        Item { Layout.fillHeight: true }
    }
}
