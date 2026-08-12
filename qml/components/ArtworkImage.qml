import QtQuick
import org.kde.kirigami as Kirigami

// Artwork with rounded corners, or a circle for artist portraits.
//
// Qt Quick clipping is rectangular, so the picture itself is rounded by
// Kirigami.ShadowedImage, which masks the texture in a shader. The placeholder
// sits on top and fades out once the picture has loaded, which reads as the
// artwork fading in without ever animating the masked item's opacity —
// animating that leaves tiles stuck part-way faded.
Item {
    id: root

    property url source
    property string fallbackIcon: "media-album-cover"
    property real radius: Kirigami.Units.smallSpacing
    /// Artist portraits are circular, following Apple Music's own convention.
    property bool circular: false

    readonly property real cornerRadius: circular ? Math.min(width, height) / 2
                                                  : radius
    // The software scene graph has no shader pipeline, so the masking
    // primitive renders wrong there. Rather than show broken artwork, fall
    // back to a plain square picture. Only machines without working graphics
    // acceleration take this path.
    readonly property bool masked: GraphicsInfo.api !== GraphicsInfo.Software
    readonly property int status: artwork.item ? artwork.item.status : Image.Null

    // Artwork URLs are always requested at 512px (MediaItem::artwork), which a
    // grid tile a fifth of that size then pays for twice over: the JPEG is
    // decoded at full size and uploaded as a 512² texture. Handing the decoder
    // a target size lets it scale during decode instead, which for JPEG is
    // most of the work saved. Quantised so that dragging the artwork-size
    // slider does not re-decode every picture on every frame.
    readonly property int decodeSize: {
        const drawn = Math.max(width, height) * Screen.devicePixelRatio;
        return Math.max(64, Math.min(512, Math.ceil(drawn / 64) * 64));
    }

    implicitWidth: Kirigami.Units.iconSizes.huge
    implicitHeight: implicitWidth

    Loader {
        id: artwork
        anchors.fill: parent
        sourceComponent: root.masked ? maskedArtwork : plainArtwork
    }

    Component {
        id: maskedArtwork
        Kirigami.ShadowedImage {
            source: root.source
            radius: root.cornerRadius
            // The placeholder in front covers this until the picture arrives.
            color: "transparent"
            asynchronous: true
            sourceSize.width: root.decodeSize
            sourceSize.height: root.decodeSize
            // Left at the default stretch: artwork is always requested square
            // (MediaItem::artwork), and PreserveAspectCrop would paint outside
            // the bounds the mask is built from.
        }
    }

    Component {
        id: plainArtwork
        Image {
            source: root.source
            asynchronous: true
            cache: true
            fillMode: Image.PreserveAspectCrop
            sourceSize.width: root.decodeSize
            sourceSize.height: root.decodeSize
        }
    }

    Rectangle {
        id: placeholder
        anchors.fill: parent
        // Rounding the placeholder like the artwork only makes sense when the
        // artwork is actually rounded.
        radius: root.masked ? root.cornerRadius : root.radius
        color: Kirigami.Theme.alternateBackgroundColor
        opacity: root.status === Image.Ready ? 0 : 1
        visible: opacity > 0
        Behavior on opacity {
            NumberAnimation { duration: Kirigami.Units.longDuration }
        }

        Rectangle {
            anchors.fill: parent
            radius: parent.radius
            visible: root.status === Image.Loading
            color: Kirigami.Theme.highlightColor
            opacity: 0.12
            SequentialAnimation on opacity {
                running: parent.visible
                loops: Animation.Infinite
                NumberAnimation { to: 0.28; duration: 650 }
                NumberAnimation { to: 0.08; duration: 650 }
            }
        }

        Kirigami.Icon {
            anchors.centerIn: parent
            width: Math.min(parent.width, parent.height) * 0.45
            height: width
            source: root.fallbackIcon
            color: Kirigami.Theme.disabledTextColor
        }
    }
}
