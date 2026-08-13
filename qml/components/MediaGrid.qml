import QtQuick
import org.kde.kirigami as Kirigami

// The album/artist/playlist grid used by Listen Now, the library pages and
// artist detail. Tile size follows the global artwork-size setting.
GridView {
    id: root

    // Artists read better a little smaller than square album art.
    property real sizeScale: 1.0
    // Room under each tile for the title and subtitle labels.
    property real labelHeight: Kirigami.Units.gridUnit * 3

    readonly property real minimumCellWidth: Kirigami.Units.gridUnit
                                             * App.artworkSize * sizeScale
    readonly property int columns: Math.max(1, Math.floor(width / minimumCellWidth))
    readonly property int rowCount: Math.ceil(count / Math.max(1, columns))
    // Height that shows every row at once, for grids embedded in a page that
    // scrolls as a whole. Derived from `columns` rather than re-deriving it
    // from cellWidth, which rounding could put a column out.
    readonly property real fullHeight: rowCount * cellHeight

    cellWidth: width / columns
    cellHeight: cellWidth + labelHeight
    clip: true
    reuseItems: true

    delegate: MediaGridDelegate {
        // A station is an endless stream with no track list to show, so
        // opening one just starts it. A folder has no detail page of its own;
        // it navigates one level deeper into the folder browser instead.
        onOpenRequested: mediaType === "stations"
                         ? App.player.playStation(catalogId || mediaId)
                         : mediaType === "library-playlist-folders"
                           ? applicationWindow().openPlaylistFolder(mediaId)
                           : applicationWindow().openDetail(mediaId, catalogId,
                                                            mediaType, title,
                                                            subtitle, artwork)
        onPlayRequested: mediaType === "stations"
                         ? App.player.playStation(catalogId || mediaId)
                         : App.playCollection(mediaId, catalogId, mediaType)
    }
}
