import QtQuick
import QtQuick.Controls as QQC2
import org.kde.kirigami as Kirigami

// A secondary-text label that behaves like a link when it points somewhere.
// Used for the artist and album names threaded through the track lists, which
// were already clickable but gave no sign of it.
QQC2.Label {
    id: root

    property bool linked: false
    signal activated()

    color: linked && hover.hovered ? Kirigami.Theme.linkColor
                                   : Kirigami.Theme.disabledTextColor
    font.underline: linked && hover.hovered
    elide: Text.ElideRight

    HoverHandler {
        id: hover
        enabled: root.linked
        cursorShape: Qt.PointingHandCursor
    }
    TapHandler {
        enabled: root.linked
        onTapped: root.activated()
    }

    QQC2.ToolTip.visible: root.linked && hover.hovered
    QQC2.ToolTip.text: i18n("Go to %1", root.text)
}
