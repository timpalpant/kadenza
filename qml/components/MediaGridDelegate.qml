import QtQuick
import QtQuick.Controls as QQC2
import QtQuick.Layouts
import org.kde.kirigami as Kirigami

QQC2.ItemDelegate {
    id: root
    required property int index
    required property string mediaId
    required property string catalogId
    required property string mediaType
    required property string title
    required property string subtitle
    required property string artwork
    required property string artistId
    required property int rating
    required property bool favorite
    required property bool inLibrary
    signal openRequested()
    signal playRequested()

    readonly property bool artistTile: root.mediaType.indexOf("artist") >= 0
    // Curators, activities, record labels and playlist folders are pure
    // browse nodes: nothing to play, rate, favorite, or add to the library as
    // a whole, only the playlists/albums/playlists found inside them.
    readonly property bool browseOnlyTile: root.mediaType.indexOf("curator") >= 0
                                           || root.mediaType.indexOf("activities") >= 0
                                           || root.mediaType.indexOf("record-label") >= 0
                                           || root.mediaType.indexOf("playlist-folder") >= 0
    // Artists resolve to a list of albums rather than something playable, so
    // they get no play affordance.
    readonly property bool playableCollection: !root.artistTile && !root.browseOnlyTile
    // Apple rates songs, albums, playlists and stations, but not artists.
    readonly property bool ratable: !root.artistTile && !root.browseOnlyTile

    width: GridView.view ? GridView.view.cellWidth : Kirigami.Units.gridUnit * 10
    height: GridView.view ? GridView.view.cellHeight : Kirigami.Units.gridUnit * 13
    padding: Kirigami.Units.smallSpacing
    onClicked: openRequested()
    onPressAndHold: menuLoader.popup()
    TapHandler {
        acceptedButtons: Qt.RightButton
        onTapped: menuLoader.popup()
    }

    // A tile's context menu is a Popup carrying its own panel, background and
    // one control per entry — more to build than everything else in the tile
    // combined, for something most tiles never show. Artist detail lays out
    // every row of every shelf at once (see DetailPage's ArtistShelf), so
    // eagerly building these dominated the page's load. Build on first use.
    Loader {
        id: menuLoader
        active: false
        sourceComponent: menuComponent
        onLoaded: item.popup()
        function popup() {
            if (active) {
                item.popup();
            } else {
                active = true; // onLoaded pops it
            }
        }
    }

    Component {
        id: menuComponent
        QQC2.Menu {
            QQC2.MenuItem {
                text: i18n("Play")
                icon.name: "media-playback-start"
                visible: root.playableCollection
                onTriggered: root.playRequested()
            }
            QQC2.MenuItem {
                text: i18n("Open")
                onTriggered: root.openRequested()
            }
            QQC2.MenuSeparator { visible: root.ratable }
            QQC2.MenuItem {
                visible: root.ratable
                text: root.rating > 0 ? i18n("Loved") : i18n("Love")
                icon.name: "love"
                checkable: true
                checked: root.rating > 0
                onTriggered: App.setRating(root.catalogId || root.mediaId,
                                           root.mediaType, root.rating > 0 ? 0 : 1)
            }
            QQC2.MenuItem {
                visible: root.ratable
                text: root.rating < 0 ? i18n("Disliked") : i18n("Dislike")
                icon.name: "dialog-cancel"
                checkable: true
                checked: root.rating < 0
                onTriggered: App.setRating(root.catalogId || root.mediaId,
                                           root.mediaType, root.rating < 0 ? 0 : -1)
            }
            QQC2.MenuSeparator { visible: root.ratable }
            QQC2.MenuItem {
                visible: root.mediaType.indexOf("playlist") < 0 && !root.browseOnlyTile
                text: root.favorite ? i18n("Remove from Favorites") : i18n("Add to Favorites")
                onTriggered: App.setFavorite(root.catalogId || root.mediaId, root.mediaType, !root.favorite)
            }
            QQC2.MenuItem {
                visible: root.mediaType.indexOf("playlist") < 0 && !root.browseOnlyTile
                text: root.inLibrary ? i18n("Remove from Library") : i18n("Add to Library")
                onTriggered: App.setInLibrary(root.inLibrary ? root.mediaId : (root.catalogId || root.mediaId), root.mediaType, !root.inLibrary)
            }
        }
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.smallSpacing
        ArtworkImage {
            id: artwork
            source: root.artwork
            fallbackIcon: root.artistTile ? "view-media-artist" : "media-album-cover"
            circular: root.artistTile
            Layout.fillWidth: true
            Layout.preferredHeight: width

            // A small lift on hover, so the pointer target reads as a tile
            // rather than a flat picture.
            scale: root.hovered ? 1.03 : 1
            Behavior on scale {
                NumberAnimation {
                    duration: Kirigami.Units.shortDuration
                    easing.type: Easing.OutCubic
                }
            }

            // The badge and the play button are built on demand. Most tiles are
            // unrated and never hovered, and a RoundButton is a full control
            // with a background of its own — building both for every tile cost
            // a third of the delegate's construction time for something the
            // user usually never sees.
            Loader {
                anchors.left: parent.left
                anchors.top: parent.top
                anchors.margins: Kirigami.Units.smallSpacing
                active: root.rating !== 0
                sourceComponent: Rectangle {
                    width: Kirigami.Units.iconSizes.small + Kirigami.Units.smallSpacing
                    height: width
                    radius: width / 2
                    color: Qt.rgba(0, 0, 0, 0.55)
                    Kirigami.Icon {
                        anchors.centerIn: parent
                        width: Kirigami.Units.iconSizes.small
                        height: width
                        source: root.rating > 0 ? "love" : "dialog-cancel"
                        color: "white"
                    }
                }
            }
            Loader {
                anchors.right: parent.right
                anchors.bottom: parent.bottom
                anchors.margins: Kirigami.Units.smallSpacing
                active: root.playableCollection && root.hovered
                sourceComponent: QQC2.RoundButton {
                    text: i18n("Play")
                    icon.name: "media-playback-start"
                    display: QQC2.AbstractButton.IconOnly
                    highlighted: true
                    onClicked: root.playRequested()
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.text: i18n("Play %1", root.title)
                }
            }
        }
        QQC2.Label {
            text: root.title
            font.weight: Font.Medium
            elide: Text.ElideRight
            maximumLineCount: 1
            // Centred under the circle, the way Apple Music sets artists.
            horizontalAlignment: root.artistTile ? Text.AlignHCenter
                                                 : Text.AlignLeft
            Layout.fillWidth: true
        }
        LinkLabel {
            text: root.subtitle
            maximumLineCount: 1
            horizontalAlignment: root.artistTile ? Text.AlignHCenter
                                                 : Text.AlignLeft
            // Artist tiles already lead to the artist. Album tiles carry an
            // artist name worth following; on playlists and stations the
            // subtitle is a curator or a genre, which leads nowhere.
            linked: root.mediaType.indexOf("albums") >= 0
                    && root.subtitle.length > 0
            onActivated: applicationWindow().openArtist(root.artistId, root.subtitle)
            Layout.fillWidth: true
        }
    }
}
