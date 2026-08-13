import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

// Reached only via applicationWindow().openPlaylistFolder(), which already
// calls App.loadPlaylistFolder() before pushing this page — mirroring
// DetailPage, this page only ever reads App's reactive properties and never
// fetches on its own, so re-entering it never clobbers the folder id the
// caller just set.
Kirigami.ScrollablePage {
    id: page
    title: App.playlistFolderTitle.length > 0 ? App.playlistFolderTitle : i18n("Playlists")

    actions: [
        Kirigami.Action {
            visible: !applicationWindow().compactMode
            displayHint: Kirigami.DisplayHint.KeepVisible
            displayComponent: ArtworkSizeSlider {}
        },
        Kirigami.Action {
            text: i18n("Refresh")
            icon.name: "view-refresh"
            onTriggered: App.loadPlaylistFolder(App.playlistFolderId)
        }
    ]

    ColumnLayout {
        width: page.availableWidth
        spacing: Kirigami.Units.largeSpacing

        MediaGrid {
            model: App.playlistFolderModel
            visible: App.playlistFolderModel.count > 0
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }

        Kirigami.PlaceholderMessage {
            visible: !App.playlistFolderModel.loading && App.playlistFolderModel.count === 0
            text: App.playlistFolderModel.error.length > 0
                  ? App.playlistFolderModel.error
                  : i18n("This folder is empty")
            icon.name: "folder"
            Layout.alignment: Qt.AlignHCenter
            Layout.topMargin: Kirigami.Units.gridUnit * 6
        }

        QQC2.BusyIndicator {
            visible: App.playlistFolderModel.loading
            running: visible
            Layout.alignment: Qt.AlignHCenter
        }
    }
}
