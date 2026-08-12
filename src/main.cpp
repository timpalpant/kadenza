#include "appcontroller.h"
#include "mprisplayer.h"
#include "version.h"

#include <KAboutData>
#include <KGlobalAccel>
#include <KLocalizedQmlContext>
#include <KLocalizedString>
#include <KNotification>

#include <QAction>
#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QTimer>
#include <functional>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain(QByteArrayLiteral("kadenza"));
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }

    KAboutData aboutData(QStringLiteral("kadenza"),
                         i18n("Kadenza"),
                         QStringLiteral(KADENZA_VERSION_STRING),
                         i18n("A native Apple Music client for KDE Plasma"),
                         KAboutLicense::GPL_V3,
                         i18n("© 2026 the Kadenza authors"));
    aboutData.setOtherText(i18n("Kadenza is an unofficial client and is not affiliated with or "
                                "endorsed by Apple. Playback uses Apple Music's MusicKit and "
                                "requires an active subscription.\n\n"
                                "The playback sidecar contains code derived from Slipmat by Miguel "
                                "Rincon, used under the GPL."));
    aboutData.setHomepage(QStringLiteral("https://github.com/timpalpant/kadenza"));
    aboutData.setBugAddress(QByteArrayLiteral("https://github.com/timpalpant/kadenza/issues"));
    aboutData.setDesktopFileName(QStringLiteral(KADENZA_APP_ID));
    // Only genuine runtime dependencies belong here. castLabs' Widevine-enabled
    // Electron is the runtime the sidecar executes on. Slipmat is not listed:
    // its code was adapted into the sidecar rather than linked against, so the
    // attribution lives in otherText and in the sidecar's SPDX headers.
    aboutData.addComponent(i18n("Electron for Content Security"),
                           i18n("castLabs Electron build providing the Widevine CDM"),
                           QString(),
                           QStringLiteral("https://github.com/castlabs/electron-releases"));
    KAboutData::setApplicationData(aboutData);
    const QIcon bundledIcon(QStringLiteral(":/qt/qml/io/github/timpalpant/kadenza/data/icons/"
                                           "io.github.timpalpant.kadenza.svg"));
    QGuiApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral(KADENZA_APP_ID), bundledIcon));

    QCommandLineParser parser;
    aboutData.setupCommandLine(&parser);
    parser.process(app);
    aboutData.processCommandLine(&parser);

    auto *controller = new AppController(&app);
    AppController::setInstance(controller);
    MprisPlayer mpris(controller->player(), &app);

    QQmlApplicationEngine engine;
    KLocalization::setupLocalizedContext(&engine);
    engine.rootContext()->setContextProperty(QStringLiteral("AboutData"), QVariant::fromValue(aboutData));
    engine.loadFromModule("io.github.timpalpant.kadenza", "Main");
    if (engine.rootObjects().isEmpty())
        return 1;
    auto *mainWindow = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    if (!mainWindow)
        return 1;

    const auto showWindow = [mainWindow] {
        mainWindow->show();
        mainWindow->raise();
        mainWindow->requestActivate();
    };

    auto *showShortcut = new QAction(i18n("Show Kadenza"), &app);
    showShortcut->setObjectName(QStringLiteral("show-kadenza"));
    QObject::connect(showShortcut, &QAction::triggered, &app, showWindow);
    KGlobalAccel::setGlobalShortcut(showShortcut, QKeySequence(QStringLiteral("Meta+Alt+K")));
    const auto mediaShortcut = [&app](const QString &name, const QString &text, QKeySequence sequence, const std::function<void()> &handler) {
        auto *action = new QAction(text, &app);
        action->setObjectName(name);
        QObject::connect(action, &QAction::triggered, &app, handler);
        KGlobalAccel::setGlobalShortcut(action, sequence);
    };
    mediaShortcut(QStringLiteral("play-pause"), i18n("Play/Pause"), QKeySequence(Qt::Key_MediaTogglePlayPause), [controller] {
        controller->player()->playPause();
    });
    mediaShortcut(QStringLiteral("next-track"), i18n("Next Track"), QKeySequence(Qt::Key_MediaNext), [controller] { controller->player()->next(); });
    mediaShortcut(QStringLiteral("previous-track"), i18n("Previous Track"), QKeySequence(Qt::Key_MediaPrevious), [controller] {
        controller->player()->previous();
    });

    QObject::connect(controller->player(), &PlayerController::nowPlayingChanged, &app, [controller, mainWindow] {
        const auto *player = controller->player();
        if (player->title().isEmpty())
            return;
        // Nothing to announce while the user is looking at the
        // window that already shows the new track.
        if (mainWindow->isActive())
            return;
        // A named event so the popup can be configured or muted
        // from System Settings like any other KDE notification.
        KNotification::event(QStringLiteral("trackChanged"), player->title(), player->artist());
    });
    const QString screenshotPath = qEnvironmentVariable("KADENZA_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        auto *window = mainWindow;
        bool widthOk = false;
        bool heightOk = false;
        const int screenshotWidth = qEnvironmentVariableIntValue("KADENZA_SCREENSHOT_WIDTH", &widthOk);
        const int screenshotHeight = qEnvironmentVariableIntValue("KADENZA_SCREENSHOT_HEIGHT", &heightOk);
        if (widthOk && heightOk && screenshotWidth > 0 && screenshotHeight > 0)
            window->resize(screenshotWidth, screenshotHeight);
        const QString page = qEnvironmentVariable("KADENZA_DEMO_PAGE", "home");
        // Artwork loads asynchronously and then fades in, so a grab taken too
        // early catches tiles mid-fade. Overridable for slower machines.
        bool delayOk = false;
        const int settle = qEnvironmentVariableIntValue("KADENZA_SCREENSHOT_DELAY", &delayOk);
        const int settleMs = delayOk && settle > 0 ? settle : 2500;
        QTimer::singleShot(250, window, [window, page, screenshotPath, settleMs, &app] {
            QMetaObject::invokeMethod(window, "navigate", Q_ARG(QVariant, page));
            QTimer::singleShot(settleMs, window, [window, screenshotPath, &app] {
                if (!window->grabWindow().save(screenshotPath))
                    qCritical().noquote() << "Could not save screenshot:" << screenshotPath;
                app.quit();
            });
        });
    }
    return app.exec();
}
