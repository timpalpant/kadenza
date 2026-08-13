import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

Kirigami.ScrollablePage {
    id: page
    title: i18n("Search")

    readonly property int resultCount: App.searchArtistsModel.count
                                       + App.searchAlbumsModel.count
                                       + App.searchSongsModel.count
                                       + App.searchPlaylistsModel.count
                                       + App.searchCuratorsModel.count
                                       + App.searchActivitiesModel.count
    readonly property real tileWidth: Kirigami.Units.gridUnit * App.artworkSize
    readonly property real labelHeight: Kirigami.Units.gridUnit * 3

    actions: [
        Kirigami.Action {
            visible: !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        }
    ]

    header: QQC2.ToolBar {
        RowLayout {
            anchors.fill: parent
            anchors.margins: Kirigami.Units.smallSpacing
            // Kirigami's own search field: search affordance, a built-in clear
            // button and debounced accept, so no hand-rolled timer or Clear
            // button is needed.
            Kirigami.SearchField {
                id: field
                placeholderText: App.searchLibrary
                                 ? i18n("Search your library")
                                 : i18n("Artists, albums, songs, and playlists")
                autoAccept: true
                delaySearch: true
                focus: true
                Layout.fillWidth: true
                onAccepted: App.search(text)
                // Apple's autocomplete only covers the catalog.
                onTextChanged: if (!App.searchLibrary)
                    App.requestSearchHints(text)

                Keys.onDownPressed: if (hintList.count > 0) {
                    hintPopup.open();
                    hintList.forceActiveFocus();
                    hintList.currentIndex = 0;
                }

                QQC2.Popup {
                    id: hintPopup
                    y: field.height
                    width: field.width
                    padding: 0
                    // Only suggest while the field is being typed into and the
                    // suggestions are not just echoing an already-run search.
                    visible: field.activeFocus && hintList.count > 0
                             && field.text.length > 0
                    ListView {
                        id: hintList
                        implicitWidth: hintPopup.width
                        implicitHeight: Math.min(contentHeight,
                                                 Kirigami.Units.gridUnit * 12)
                        model: App.searchHints
                        clip: true
                        keyNavigationEnabled: true
                        delegate: QQC2.ItemDelegate {
                            required property string modelData
                            width: hintList.width
                            text: modelData
                            icon.name: "search"
                            onClicked: {
                                field.text = modelData;
                                App.search(modelData);
                                hintPopup.close();
                            }
                            Keys.onReturnPressed: clicked()
                        }
                    }
                }
            }
            QQC2.ToolButton {
                text: App.searchLibrary ? i18n("Searching Your Library")
                                        : i18n("Searching Apple Music")
                icon.name: App.searchLibrary ? "server-database" : "cloud-download"
                display: QQC2.AbstractButton.IconOnly
                checkable: true
                checked: App.searchLibrary
                onToggled: App.searchLibrary = checked
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: App.searchLibrary
                                   ? i18n("Searching your library — click to search Apple Music")
                                   : i18n("Searching Apple Music — click to search your library")
            }
            QQC2.ToolButton {
                text: i18n("Recent Searches")
                icon.name: "document-open-recent"
                display: QQC2.AbstractButton.IconOnly
                enabled: App.searchHistory.length > 0
                onClicked: historyMenu.popup()
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.text: text
                QQC2.Menu {
                    id: historyMenu
                    Repeater {
                        model: App.searchHistory
                        QQC2.MenuItem {
                            required property string modelData
                            text: modelData
                            onTriggered: {
                                field.text = modelData;
                                App.search(modelData);
                            }
                        }
                    }
                }
            }
        }
    }

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        SearchSection {
            heading: i18n("Artists")
            model: App.searchArtistsModel
            tileWidth: page.tileWidth * 0.75
        }
        SearchSection {
            heading: i18n("Albums")
            model: App.searchAlbumsModel
        }
        SearchSection {
            heading: i18n("Songs")
            model: App.searchSongsModel
            horizontal: false
            interactive: false
            delegateConfig: Component { MediaDelegate { onPlayRequested: App.playModel(App.searchSongsModel, index); onOpenRequested: {} } }
        }
        SearchSection {
            heading: i18n("Playlists")
            model: App.searchPlaylistsModel
        }
        SearchSection {
            heading: i18n("Curators")
            model: App.searchCuratorsModel
        }
        SearchSection {
            heading: i18n("Activities")
            model: App.searchActivitiesModel
        }

        Kirigami.PlaceholderMessage {
            visible: page.resultCount === 0 && !App.searchModel.loading
            text: field.text.length === 0
                  ? (App.searchLibrary ? i18n("Search your library")
                                       : i18n("Search Apple Music"))
                  : i18n("No results")
            icon.name: "search"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        QQC2.BusyIndicator {
            visible: App.searchModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
