#pragma once

#include <QDBusAbstractAdaptor>
#include <QDBusObjectPath>
#include <QVariantMap>

class PlayerController;

class MprisRootAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2")
    Q_PROPERTY(bool CanQuit READ canQuit CONSTANT)
    Q_PROPERTY(bool CanRaise READ canRaise CONSTANT)
    Q_PROPERTY(bool HasTrackList READ hasTrackList CONSTANT)
    Q_PROPERTY(QString Identity READ identity CONSTANT)
    Q_PROPERTY(QString DesktopEntry READ desktopEntry CONSTANT)
    Q_PROPERTY(QStringList SupportedUriSchemes READ supportedUriSchemes CONSTANT)
    Q_PROPERTY(QStringList SupportedMimeTypes READ supportedMimeTypes CONSTANT)

public:
    explicit MprisRootAdaptor(QObject *parent);
    bool canQuit() const { return true; }
    bool canRaise() const { return true; }
    bool hasTrackList() const { return false; }
    QString identity() const { return QStringLiteral("Kadenza"); }
    QString desktopEntry() const { return QStringLiteral("io.github.timpalpant.kadenza"); }
    QStringList supportedUriSchemes() const { return {}; }
    QStringList supportedMimeTypes() const { return {}; }

public slots:
    void Raise();
    void Quit();
};

class MprisPlayerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.mpris.MediaPlayer2.Player")
    Q_PROPERTY(QString PlaybackStatus READ playbackStatus NOTIFY propertiesChanged)
    Q_PROPERTY(QString LoopStatus READ loopStatus WRITE setLoopStatus NOTIFY propertiesChanged)
    Q_PROPERTY(double Rate READ rate WRITE setRate)
    Q_PROPERTY(bool Shuffle READ shuffle WRITE setShuffle NOTIFY propertiesChanged)
    Q_PROPERTY(QVariantMap Metadata READ metadata NOTIFY propertiesChanged)
    Q_PROPERTY(double Volume READ volume WRITE setVolume NOTIFY propertiesChanged)
    Q_PROPERTY(qlonglong Position READ position NOTIFY propertiesChanged)
    Q_PROPERTY(double MinimumRate READ minimumRate CONSTANT)
    Q_PROPERTY(double MaximumRate READ maximumRate CONSTANT)
    Q_PROPERTY(bool CanGoNext READ canGoNext CONSTANT)
    Q_PROPERTY(bool CanGoPrevious READ canGoPrevious CONSTANT)
    Q_PROPERTY(bool CanPlay READ canPlay CONSTANT)
    Q_PROPERTY(bool CanPause READ canPause CONSTANT)
    Q_PROPERTY(bool CanSeek READ canSeek CONSTANT)
    Q_PROPERTY(bool CanControl READ canControl CONSTANT)

public:
    MprisPlayerAdaptor(PlayerController *player, QObject *parent);
    QString playbackStatus() const;
    QString loopStatus() const;
    void setLoopStatus(const QString &status);
    double rate() const { return 1.0; }
    void setRate(double) {}
    bool shuffle() const;
    void setShuffle(bool shuffle);
    QVariantMap metadata() const;
    QDBusObjectPath trackId() const;
    double volume() const;
    void setVolume(double volume);
    qlonglong position() const;
    double minimumRate() const { return 1.0; }
    double maximumRate() const { return 1.0; }
    bool canGoNext() const { return true; }
    bool canGoPrevious() const { return true; }
    bool canPlay() const { return true; }
    bool canPause() const { return true; }
    bool canSeek() const { return true; }
    bool canControl() const { return true; }

public slots:
    void Next();
    void Previous();
    void Pause();
    void PlayPause();
    void Stop();
    void Play();
    void Seek(qlonglong offset);
    void SetPosition(const QDBusObjectPath &trackId, qlonglong position);
    void OpenUri(const QString &uri);

public:
    /// Sends org.freedesktop.DBus.Properties.PropertiesChanged for the Player
    /// interface. Qt does not do this for Q_PROPERTY NOTIFY signals.
    void announce(const QVariantMap &properties);

signals:
    void Seeked(qlonglong position);
    void propertiesChanged();

private:
    // Playback advancing normally must not look like a seek. Anything further
    // from the expected position than this is a jump worth announcing.
    static constexpr qlonglong kSeekToleranceUs = 2000000;

    PlayerController *m_player;
    qlonglong m_lastPositionUs = 0;
};

class MprisPlayer : public QObject
{
    Q_OBJECT

public:
    explicit MprisPlayer(PlayerController *player, QObject *parent = nullptr);
};
