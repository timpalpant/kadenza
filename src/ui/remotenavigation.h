#pragma once

#include <QObject>
#include <QPointer>
#include <QQmlEngine>
#include <QQuickItem>
#include <QQuickWindow>

class QKeyEvent;

/**
 * Makes the window usable from a TV remote or any other D-pad: arrows,
 * OK/Enter/Select and Back, with no pointer and no Tab key.
 *
 * Qt Quick only moves focus with Tab. Arrow keys are left to whichever
 * control has focus, and most of Kadenza's controls are tiles and rows that
 * live in separate views, so the arrows had nowhere to go between one shelf
 * and the next, or from a page to the sidebar. This filter supplies the
 * missing piece once for the whole window instead of wiring KeyNavigation
 * into every page:
 *
 * - Arrows move focus to the nearest focusable control in that direction
 *   (see SpatialNavigation). On buttons and tiles that happens first; on
 *   controls that use arrows themselves (text fields, lists, combo boxes,
 *   menus) the control gets the key first and spatial movement only applies
 *   if it ignored it, at the end of a field or a list.
 * - OK clicks the focused button when the button does not handle the key
 *   itself. Qt Quick Controls only press on Space by default, and remotes
 *   send Enter or Select.
 * - Back, Escape and Backspace emit backPressed() so the window can decide
 *   what Back means at the current depth. Escape and Backspace are left to
 *   text fields, and every key is left to popups such as menus and dialogs,
 *   which close themselves on Escape/Back.
 *
 * The filter only acts while `enabled` is set; the desktop layout keeps
 * Qt's and Kirigami's own keyboard behaviour.
 */
class RemoteNavigation : public QObject
{
    Q_OBJECT
    QML_ELEMENT
    Q_PROPERTY(QQuickWindow *window READ window WRITE setWindow NOTIFY windowChanged)
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    /// Content of the navigation drawer. The drawer is a popup, which would
    /// otherwise keep focus out of it (and in it), like any other popup.
    Q_PROPERTY(QQuickItem *sidebar READ sidebar WRITE setSidebar NOTIFY sidebarChanged)
    /// The current page, where focus lands when nothing useful has it.
    Q_PROPERTY(QQuickItem *content READ content WRITE setContent NOTIFY contentChanged)
    Q_PROPERTY(bool sidebarFocused READ sidebarFocused NOTIFY sidebarFocusedChanged)

public:
    explicit RemoteNavigation(QObject *parent = nullptr);
    ~RemoteNavigation() override;

    QQuickWindow *window() const;
    void setWindow(QQuickWindow *window);
    bool enabled() const;
    void setEnabled(bool enabled);
    QQuickItem *sidebar() const;
    void setSidebar(QQuickItem *sidebar);
    QQuickItem *content() const;
    void setContent(QQuickItem *content);
    bool sidebarFocused() const;

    /// Focuses the first control inside `scope`: a checked one if there is
    /// one (the current sidebar entry), otherwise the top-left one, never a
    /// text field. Leaves focus alone if it is already on a control inside
    /// `scope`.
    /// Returns false when `scope` holds nothing focusable yet.
    Q_INVOKABLE bool focusFirst(QQuickItem *scope);

signals:
    void windowChanged();
    void enabledChanged();
    void sidebarChanged();
    void contentChanged();
    void sidebarFocusedChanged();
    void backPressed();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    bool claimsBack(const QKeyEvent *event) const;
    bool closePopup();
    bool forwardToOpenComboBox(QKeyEvent *event);
    bool handleArrow(QKeyEvent *event);
    bool handleActivate(QKeyEvent *event);
    bool deliver(QKeyEvent *event);
    bool moveFocus(int key);
    bool returnsToPage(const QQuickItem *scope) const;
    void focus(QQuickItem *item);
    QList<QQuickItem *> candidates(QQuickItem *scope) const;
    QQuickItem *popupOf(const QQuickItem *item) const;
    bool inSidebar(const QQuickItem *item) const;
    bool isCandidate(const QQuickItem *item) const;
    void updateSidebarFocused();

    QPointer<QQuickWindow> m_window;
    QPointer<QQuickItem> m_sidebar;
    QPointer<QQuickItem> m_content;
    QPointer<QQuickItem> m_lastPageFocus;
    bool m_enabled = false;
    bool m_delivering = false;
    bool m_sidebarFocused = false;
    int m_swallowRelease = 0;
};
