import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

ColumnLayout {
    id: root
    spacing: Kirigami.Units.largeSpacing

    component ArtistShelf: ColumnLayout {
        required property string heading
        required property var shelfModel
        spacing: Kirigami.Units.smallSpacing
        Layout.fillWidth: true
        Kirigami.Heading { text: heading; level: 2 }
        MediaGrid {
            model: shelfModel
            interactive: false
            Layout.fillWidth: true
            Layout.preferredHeight: fullHeight
        }
    }

    ArtistShelf {
        heading: i18n("Latest Release")
        shelfModel: App.artistLatestModel
        visible: App.artistLatestModel.count > 0
    }

    ColumnLayout {
        visible: App.artistTopSongsModel.count > 0
        spacing: Kirigami.Units.smallSpacing
        Layout.fillWidth: true
        Kirigami.Heading { text: i18n("Top Songs"); level: 2 }
        ListView {
            model: App.artistTopSongsModel
            interactive: false
            reuseItems: true
            Layout.fillWidth: true
            Layout.preferredHeight: contentHeight
            delegate: MediaDelegate {
                onPlayRequested: App.playModel(App.artistTopSongsModel, index)
                onOpenRequested: {}
            }
        }
    }

    ArtistShelf {
        heading: i18n("Albums")
        shelfModel: App.artistAlbumsModel
        visible: App.artistAlbumsModel.count > 0
    }
    ArtistShelf {
        heading: i18n("Singles and EPs")
        shelfModel: App.artistSinglesModel
        visible: App.artistSinglesModel.count > 0
    }
    ArtistShelf {
        heading: i18n("Similar Artists")
        shelfModel: App.artistSimilarModel
        visible: App.artistSimilarModel.count > 0
    }

    MediaGrid {
        model: App.detailTracksModel
        interactive: false
        visible: App.detailTracksModel.count > 0
        Layout.fillWidth: true
        Layout.preferredHeight: fullHeight
    }
}
