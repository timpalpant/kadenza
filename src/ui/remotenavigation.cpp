#include "remotenavigation.h"
#include "spatialnavigation.h"

#include <QCoreApplication>
#include <QKeyEvent>
#include <QMetaObject>

#include <algorithm>

namespace {

bool isA(const QObject *object, const char *className)
{
    return object && object->inherits(className);
}

bool isTextInput(const QQuickItem *item)
{
    return isA(item, "QQuickTextInput") || isA(item, "QQuickTextEdit");
}

bool isButton(const QQuickItem *item)
{
    return isA(item, "QQuickAbstractButton");
}

// The Popup object a popup item belongs to; it is no item itself.
QObject *popupObject(const QQuickItem *popupItem)
{
    for (QObject *owner = popupItem->parent(); owner; owner = owner->parent()) {
        if (owner->inherits("QQuickPopup"))
            return owner;
    }
    return nullptr;
}

QObject *comboPopup(const QQuickItem *item)
{
    return isA(item, "QQuickComboBox") ? item->property("popup").value<QObject *>() : nullptr;
}

// A closed combo box would otherwise take Up and Down as "pick the
// neighbouring value", so one press meant to move past it changed the
// setting instead. Closed, it moves like a button and OK opens it; open, its
// list has the arrows.
bool isClosedComboBox(const QQuickItem *item)
{
    const QObject *popup = comboPopup(item);
    return popup && !popup->property("visible").toBool();
}

bool isArrow(int key)
{
    return key == Qt::Key_Left || key == Qt::Key_Right || key == Qt::Key_Up || key == Qt::Key_Down;
}

bool isActivateKey(int key)
{
    return key == Qt::Key_Return || key == Qt::Key_Enter || key == Qt::Key_Select;
}

bool isVertical(int key)
{
    return key == Qt::Key_Up || key == Qt::Key_Down;
}

// Arrow keys on a keypad carry KeypadModifier; anything else (Shift+arrow to
// select text, Alt+Left for Back) belongs to someone else.
bool unmodified(const QKeyEvent *event)
{
    return (event->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
}

QRectF sceneRect(const QQuickItem *item)
{
    return item->mapRectToScene(QRectF(0, 0, item->width(), item->height()));
}

QQuickItem *nearestFlickable(const QQuickItem *item)
{
    for (QQuickItem *parent = item->parentItem(); parent; parent = parent->parentItem()) {
        if (isA(parent, "QQuickFlickable"))
            return parent;
    }
    return nullptr;
}

// Something the user could bring into view: on screen now, or inside a
// flickable that can scroll to it. Pages Kirigami keeps further down the
// stack sit beside the viewport in no flickable at all, and drop out here.
bool reachable(const QQuickItem *item, const QRectF &windowRect)
{
    if (sceneRect(item).intersects(windowRect))
        return true;
    const QQuickItem *flickable = nearestFlickable(item);
    return flickable && reachable(flickable, windowRect);
}

// Where an item counts as being for spatial navigation: its rect clamped into
// the visible part of every flickable it sits in. Tiles scrolled out of
// view then line up along the edge of the page they would scroll in from,
// rather than behind the player bar that covers the page's bottom edge, so
// Down from the last visible row moves on into the page before the bar.
QRectF navigationRect(const QQuickItem *item)
{
    QRectF rect = sceneRect(item);
    for (const QQuickItem *flickable = nearestFlickable(item); flickable; flickable = nearestFlickable(flickable)) {
        const QRectF viewport = sceneRect(flickable);
        const auto clampRange = [](qreal &low, qreal &high, qreal min, qreal max) {
            low = std::clamp(low, min, max);
            high = std::clamp(high, min, max);
            // A sliver rather than nothing, so it still counts as a target.
            if (high - low < 1) {
                if (high >= max)
                    low = high - 1;
                else
                    high = low + 1;
            }
        };
        qreal top = rect.top(), bottom = rect.bottom(), left = rect.left(), right = rect.right();
        clampRange(top, bottom, viewport.top(), viewport.bottom());
        clampRange(left, right, viewport.left(), viewport.right());
        rect = QRectF(QPointF(left, top), QPointF(right, bottom));
    }
    return rect;
}

SpatialNavigation::Direction directionFor(int key)
{
    switch (key) {
    case Qt::Key_Left:
        return SpatialNavigation::Direction::Left;
    case Qt::Key_Right:
        return SpatialNavigation::Direction::Right;
    case Qt::Key_Up:
        return SpatialNavigation::Direction::Up;
    default:
        return SpatialNavigation::Direction::Down;
    }
}

// Scrolls every flickable between `item` and the window just far enough to
// show it, innermost first: a tile in a grid in a scrolling page.
void ensureVisible(QQuickItem *item)
{
    for (QQuickItem *flickable = nearestFlickable(item); flickable; flickable = nearestFlickable(flickable)) {
        const QRectF rect = item->mapRectToItem(flickable, QRectF(0, 0, item->width(), item->height()));
        // A little of whatever comes next stays visible, so the next press
        // does not land on something half off screen.
        const qreal marginY = qMin(rect.height() / 4, flickable->height() / 8);
        const qreal marginX = qMin(rect.width() / 4, flickable->width() / 8);
        const auto scroll = [flickable](const char *position,
                                        const char *origin,
                                        const char *extent,
                                        const char *before,
                                        const char *after,
                                        qreal viewport,
                                        qreal low,
                                        qreal high,
                                        qreal margin) {
            qreal delta = 0;
            if (low < margin)
                delta = low - margin;
            else if (high > viewport - margin)
                delta = qMin(high - (viewport - margin), low - margin);
            if (delta == 0)
                return;
            const qreal start = flickable->property(origin).toReal() - flickable->property(before).toReal();
            const qreal end
                = flickable->property(origin).toReal() + flickable->property(extent).toReal() + flickable->property(after).toReal() - viewport;
            if (end <= start)
                return; // Everything already fits.
            const qreal current = flickable->property(position).toReal();
            flickable->setProperty(position, std::clamp(current + delta, start, end));
        };
        scroll("contentY", "originY", "contentHeight", "topMargin", "bottomMargin", flickable->height(), rect.top(), rect.bottom(), marginY);
        scroll("contentX", "originX", "contentWidth", "leftMargin", "rightMargin", flickable->width(), rect.left(), rect.right(), marginX);
    }
}

} // namespace

RemoteNavigation::RemoteNavigation(QObject *parent)
    : QObject(parent)
{
    // ShortcutOverride goes to the focused item rather than the window, so
    // only an application-wide filter sees it. It is how Back is claimed
    // from Kirigami's own Back shortcut.
    QCoreApplication::instance()->installEventFilter(this);
}

RemoteNavigation::~RemoteNavigation()
{
    if (auto *app = QCoreApplication::instance())
        app->removeEventFilter(this);
}

QQuickWindow *RemoteNavigation::window() const
{
    return m_window;
}

void RemoteNavigation::setWindow(QQuickWindow *window)
{
    if (m_window == window)
        return;
    if (m_window)
        disconnect(m_window, nullptr, this, nullptr);
    m_window = window;
    if (m_window)
        connect(m_window, &QQuickWindow::activeFocusItemChanged, this, &RemoteNavigation::updateSidebarFocused);
    updateSidebarFocused();
    Q_EMIT windowChanged();
}

bool RemoteNavigation::enabled() const
{
    return m_enabled;
}

void RemoteNavigation::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    Q_EMIT enabledChanged();
}

QQuickItem *RemoteNavigation::sidebar() const
{
    return m_sidebar;
}

void RemoteNavigation::setSidebar(QQuickItem *sidebar)
{
    if (m_sidebar == sidebar)
        return;
    m_sidebar = sidebar;
    updateSidebarFocused();
    Q_EMIT sidebarChanged();
}

QQuickItem *RemoteNavigation::content() const
{
    return m_content;
}

void RemoteNavigation::setContent(QQuickItem *content)
{
    if (m_content == content)
        return;
    m_content = content;
    Q_EMIT contentChanged();
}

bool RemoteNavigation::sidebarFocused() const
{
    return m_sidebarFocused;
}

void RemoteNavigation::updateSidebarFocused()
{
    QQuickItem *focusItem = m_window ? m_window->activeFocusItem() : nullptr;
    // Remembered so that leaving the sidebar returns to where the user was
    // rather than to the top of the page.
    if (isCandidate(focusItem) && !inSidebar(focusItem) && !popupOf(focusItem))
        m_lastPageFocus = focusItem;
    const bool focused = inSidebar(focusItem);
    if (focused == m_sidebarFocused)
        return;
    m_sidebarFocused = focused;
    Q_EMIT sidebarFocusedChanged();
}

bool RemoteNavigation::focusFirst(QQuickItem *scope)
{
    // Focus already on a control in there stays put: a page that put it on
    // its own main control knows better than reading order.
    QQuickItem *focusItem = m_window ? m_window->activeFocusItem() : nullptr;
    if (scope && isCandidate(focusItem) && scope->isAncestorOf(focusItem))
        return true;
    // Back into a page the user already moved around in: where they left off.
    if (scope && returnsToPage(scope)) {
        focus(m_lastPageFocus);
        return true;
    }
    QList<QQuickItem *> items = candidates(scope);
    // Never a text field: on a TV, focusing one raises the on-screen
    // keyboard over the page. The arrows can still move into one.
    items.removeIf([](const QQuickItem *item) { return isTextInput(item); });
    if (items.isEmpty())
        return false;
    for (QQuickItem *item : items) {
        if (item->property("checked").toBool()) {
            focus(item);
            return true;
        }
    }
    // Reading order: top to bottom, then left to right within a band, so a
    // row whose controls are a pixel or two out of line still reads as one.
    const qreal band = 8;
    QQuickItem *first = *std::min_element(items.cbegin(), items.cend(), [band](const QQuickItem *a, const QQuickItem *b) {
        const QRectF ra = sceneRect(a);
        const QRectF rb = sceneRect(b);
        if (qAbs(ra.top() - rb.top()) > band)
            return ra.top() < rb.top();
        return ra.left() < rb.left();
    });
    focus(first);
    return true;
}

bool RemoteNavigation::eventFilter(QObject *watched, QEvent *event)
{
    const QEvent::Type type = event->type();
    if (type != QEvent::KeyPress && type != QEvent::KeyRelease && type != QEvent::ShortcutOverride)
        return false;
    if (!m_enabled || !m_window || m_delivering)
        return false;
    auto *keyEvent = static_cast<QKeyEvent *>(event);

    if (type == QEvent::KeyRelease) {
        if (watched != m_window)
            return false;
        // The release of an OK press that opened a combo box would reach the
        // combo box next and close the list again straight away.
        if (m_swallowRelease != 0 && keyEvent->key() == m_swallowRelease) {
            if (!keyEvent->isAutoRepeat())
                m_swallowRelease = 0;
            return true;
        }
        return forwardToOpenComboBox(keyEvent);
    }

    if (type == QEvent::ShortcutOverride) {
        // Sent to the focused item (or the window when nothing has focus)
        // before shortcuts run. Accepting it turns the key back into a plain
        // key press, which lands below.
        auto *item = qobject_cast<QQuickItem *>(watched);
        const bool ours = watched == m_window || (item && item->window() == m_window);
        if (ours && claimsBack(keyEvent)) {
            keyEvent->accept();
            return true;
        }
        return false;
    }

    // A key press reaches the window before any item in it.
    if (watched != m_window)
        return false;
    if (keyEvent->key() == Qt::Key_Back && unmodified(keyEvent) && closePopup())
        return true;
    if (claimsBack(keyEvent)) {
        if (!keyEvent->isAutoRepeat())
            Q_EMIT backPressed();
        return true;
    }
    if (forwardToOpenComboBox(keyEvent))
        return true;
    if (isArrow(keyEvent->key()) && unmodified(keyEvent))
        return handleArrow(keyEvent);
    if (isActivateKey(keyEvent->key()) && unmodified(keyEvent))
        return handleActivate(keyEvent);
    return false;
}

// An open combo box list takes focus into its popup, but the arrows and OK
// that move and accept its highlight are handled by the combo box itself,
// which no longer sees them. They go to it directly, presses and releases
// alike, since it accepts the highlighted entry on release.
bool RemoteNavigation::forwardToOpenComboBox(QKeyEvent *event)
{
    if (!isArrow(event->key()) && !isActivateKey(event->key()))
        return false;
    QQuickItem *focusItem = m_window->activeFocusItem();
    QQuickItem *popupItem = isA(focusItem, "QQuickPopupItem") ? focusItem : popupOf(focusItem);
    QObject *popup = popupItem ? popupObject(popupItem) : nullptr;
    auto *comboBox = popup ? popup->property("parent").value<QQuickItem *>() : nullptr;
    if (!comboBox || comboPopup(comboBox) != popup)
        return false;
    m_delivering = true;
    QCoreApplication::sendEvent(comboBox, event);
    m_delivering = false;
    return true;
}

// Popups close on Escape but, off Android, not on the Back key a remote
// sends, which left a menu opened from the remote with no way to dismiss it.
bool RemoteNavigation::closePopup()
{
    const QQuickItem *focusItem = m_window->activeFocusItem();
    if (!focusItem || inSidebar(focusItem))
        return false;
    const QQuickItem *popupItem = isA(focusItem, "QQuickPopupItem") ? focusItem : popupOf(focusItem);
    QObject *popup = popupItem ? popupObject(popupItem) : nullptr;
    return popup && QMetaObject::invokeMethod(popup, "close");
}

bool RemoteNavigation::claimsBack(const QKeyEvent *event) const
{
    const int key = event->key();
    if (key != Qt::Key_Back && key != Qt::Key_Escape && key != Qt::Key_Backspace)
        return false;
    if (!unmodified(event))
        return false;
    const QQuickItem *focusItem = m_window->activeFocusItem();
    // Menus and dialogs close themselves on Escape; Back is turned into a
    // close by closePopup() before this is asked.
    if ((popupOf(focusItem) || isA(focusItem, "QQuickPopupItem")) && !inSidebar(focusItem))
        return false;
    // Escape and Backspace mean something to a text field.
    if (key != Qt::Key_Back && isTextInput(focusItem))
        return false;
    return true;
}

bool RemoteNavigation::handleArrow(QKeyEvent *event)
{
    QQuickItem *focusItem = m_window->activeFocusItem();
    // Inside a menu or dialog, focus never leaves it. A freshly opened one
    // has focus on the popup itself, which may use the arrows to scroll, so
    // the first press lands on its first control. After that the controls
    // get the key, and spatial movement fills in where they ignore it.
    QQuickItem *popup = inSidebar(focusItem) ? nullptr : isA(focusItem, "QQuickPopupItem") ? focusItem : popupOf(focusItem);
    if (popup) {
        if (!isCandidate(focusItem)) {
            if (!focusFirst(popup))
                deliver(event);
        } else if (!deliver(event)) {
            moveFocus(event->key());
        }
        return true;
    }

    if (!isCandidate(focusItem)) {
        // Nothing useful has focus yet, as after opening a page: the first
        // press only picks a starting point.
        // An empty page (still loading, or nothing in it) hands over to
        // the sidebar's current entry rather than whatever is top-left.
        if (focusFirst(m_content) || focusFirst(m_sidebar) || focusFirst(nullptr))
            return true;
        return deliver(event);
    }

    // Buttons and tiles have no use for arrows, so moving comes first. A
    // horizontal slider uses Left/Right but not Up/Down.
    bool moveFirst = isButton(focusItem) || isClosedComboBox(focusItem);
    if (isA(focusItem, "QQuickSlider")) {
        const bool horizontal = focusItem->property("orientation").toInt() == Qt::Horizontal;
        moveFirst = horizontal == isVertical(event->key());
    }
    if (moveFirst && moveFocus(event->key()))
        return true;
    if (deliver(event))
        return true;
    if (!moveFirst)
        moveFocus(event->key());
    return true;
}

bool RemoteNavigation::handleActivate(QKeyEvent *event)
{
    QQuickItem *focusItem = m_window->activeFocusItem();
    // A closed combo box takes OK without opening (only Space opens it), so
    // it is opened here before it sees the key.
    if (isClosedComboBox(focusItem) && focusItem->isEnabled()) {
        if (!event->isAutoRepeat()) {
            QMetaObject::invokeMethod(comboPopup(focusItem), "open");
            m_swallowRelease = event->key();
        }
        return true;
    }
    if (deliver(event))
        return true;
    // Buttons press on Space; a remote sends Enter or Select.
    if (isButton(focusItem) && focusItem->isEnabled() && !event->isAutoRepeat())
        QMetaObject::invokeMethod(focusItem, "click");
    return true;
}

// Hands the event to the window's items as normal and reports whether one of
// them took it.
bool RemoteNavigation::deliver(QKeyEvent *event)
{
    m_delivering = true;
    event->setAccepted(false);
    QCoreApplication::sendEvent(m_window, event);
    m_delivering = false;
    return event->isAccepted();
}

bool RemoteNavigation::returnsToPage(const QQuickItem *scope) const
{
    // Not a text field, for the same reason focusFirst() never picks one.
    return m_lastPageFocus && !isTextInput(m_lastPageFocus) && scope->isAncestorOf(m_lastPageFocus) && isCandidate(m_lastPageFocus)
           && reachable(m_lastPageFocus, QRectF(0, 0, m_window->width(), m_window->height()));
}

bool RemoteNavigation::moveFocus(int key)
{
    QQuickItem *focusItem = m_window->activeFocusItem();
    if (!focusItem)
        return false;
    // Right out of the sidebar goes back to where the user was on the page.
    if (key == Qt::Key_Right && inSidebar(focusItem) && m_content && returnsToPage(m_content)) {
        focus(m_lastPageFocus);
        return true;
    }
    // Within a menu or dialog, focus stays inside it. Elsewhere it moves
    // between the page, the player bar and the sidebar.
    QQuickItem *popup = isA(focusItem, "QQuickPopupItem") ? focusItem : popupOf(focusItem);
    const QList<QQuickItem *> items = candidates(popup && !inSidebar(focusItem) ? popup : nullptr);

    // Up and Down keep to their own column: the sidebar runs the full height
    // beside the page, so the end of a list sat a few pixels above some
    // sidebar entry and Down hopped across to it. Left, Right and Back are
    // the ways between the two.
    const bool fromSidebar = inSidebar(focusItem);
    QList<QRectF> rects;
    rects.reserve(items.size());
    for (const QQuickItem *item : items) {
        const bool crossesSidebar = isVertical(key) && inSidebar(item) != fromSidebar;
        rects.append(item == focusItem || crossesSidebar ? QRectF() : navigationRect(item));
    }
    const QRectF from = navigationRect(focusItem);
    const SpatialNavigation::Direction direction = directionFor(key);
    // Sideways moves stay level with the current control, so Right at the
    // end of a shelf stops instead of dropping to the player bar. From the
    // sidebar, whose entries can sit level with a gap between shelves, any
    // control to the right will do.
    int index = SpatialNavigation::nearest(from, rects, direction, !isVertical(key));
    if (index < 0 && !isVertical(key) && fromSidebar)
        index = SpatialNavigation::nearest(from, rects, direction);
    if (index < 0)
        return false;
    focus(items.at(index));
    return true;
}

void RemoteNavigation::focus(QQuickItem *item)
{
    // A keyboard reason, so controls draw their focus frame (visualFocus).
    item->forceActiveFocus(Qt::TabFocusReason);
    ensureVisible(item);
}

QList<QQuickItem *> RemoteNavigation::candidates(QQuickItem *scope) const
{
    QList<QQuickItem *> result;
    if (!m_window)
        return result;
    const QRectF windowRect(0, 0, m_window->width(), m_window->height());
    QQuickItem *sidebarPopup = m_sidebar ? popupOf(m_sidebar) : nullptr;

    // A tree walk rather than the Tab chain: popups are tab fences, so the
    // chain from the window never enters the sidebar.
    QList<QQuickItem *> pending{scope ? scope : m_window->contentItem()};
    while (!pending.isEmpty()) {
        QQuickItem *item = pending.takeLast();
        if (!item->isVisible() || !item->isEnabled() || item->opacity() <= 0)
            continue;
        // Without an explicit scope, other popups are out of bounds.
        if (!scope && isA(item, "QQuickPopupItem") && item != sidebarPopup)
            continue;
        if (item != scope && isCandidate(item) && reachable(item, windowRect))
            result.append(item);
        const QList<QQuickItem *> children = item->childItems();
        for (auto it = children.crbegin(); it != children.crend(); ++it)
            pending.append(*it);
    }
    return result;
}

QQuickItem *RemoteNavigation::popupOf(const QQuickItem *item) const
{
    for (QQuickItem *parent = item ? item->parentItem() : nullptr; parent; parent = parent->parentItem()) {
        if (isA(parent, "QQuickPopupItem"))
            return parent;
    }
    return nullptr;
}

bool RemoteNavigation::inSidebar(const QQuickItem *item) const
{
    if (!item || !m_sidebar)
        return false;
    QQuickItem *sidebarPopup = popupOf(m_sidebar);
    const QQuickItem *root = sidebarPopup ? sidebarPopup : m_sidebar.data();
    return item == root || root->isAncestorOf(item);
}

bool RemoteNavigation::isCandidate(const QQuickItem *item) const
{
    if (!item || !item->activeFocusOnTab() || !item->isVisible() || !item->isEnabled() || item->width() <= 0 || item->height() <= 0)
        return false;
    // Kirigami draws a separator action with the same focusable delegate as
    // a real entry, flagged isSeparator; it should not be a stop on the way
    // down the sidebar.
    return !item->property("isSeparator").toBool();
}
