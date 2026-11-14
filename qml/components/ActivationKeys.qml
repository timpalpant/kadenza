import QtQuick

// Splits OK into a tap and a hold for a control that has keyboard focus. A
// remote has no right button and usually no Menu key, so holding OK is how
// the context menu is reached; the Menu key works too where there is one.
//
//     ActivationKeys { id: keys; onTapped: open(); onHeld: menu.popup() }
//     Keys.onPressed: event => keys.press(event)
//     Keys.onReleased: event => keys.release(event)
//
// Holding a key makes the compositor send auto-repeated presses and releases;
// those are swallowed so a hold reads as one long press.
Timer {
    id: root

    signal tapped()
    signal held()

    property bool holding: false

    interval: 600

    function activates(event) {
        const modifiers = event.modifiers & ~Qt.KeypadModifier;
        return modifiers === Qt.NoModifier
               && (event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                   || event.key === Qt.Key_Select || event.key === Qt.Key_Space);
    }

    function press(event) {
        if (event.key === Qt.Key_Menu) {
            event.accepted = true;
            root.held();
            return;
        }
        if (!activates(event))
            return;
        event.accepted = true;
        if (event.isAutoRepeat)
            return;
        root.holding = true;
        root.restart();
    }

    function release(event) {
        if (!activates(event))
            return;
        event.accepted = true;
        if (event.isAutoRepeat || !root.holding)
            return;
        root.holding = false;
        if (root.running) {
            root.stop();
            root.tapped();
        }
    }

    onTriggered: {
        root.holding = false;
        root.held();
    }
}
