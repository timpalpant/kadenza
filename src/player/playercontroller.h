#pragma once

#include "mediamodel.h"

#include <QJsonObject>
#include <QObject>
#include <QProcess>
#include <QTimer>
#include <QVariantList>
#include <qqmlintegration.h>

class PlayerController : public QObject
{
    Q_OBJECT
    // QML_UNCREATABLE only qualifies a registration; without QML_ELEMENT the
    // type is never registered at all, so every App.player.* access in QML was
    // unresolvable to qmllint. It stays uncreatable: App owns the one instance.
    QML_ELEMENT
    QML_UNCREATABLE("Owned by App")
    Q_PROPERTY(bool available READ available NOTIFY availableChanged)
    Q_PROPERTY(bool ready READ ready NOTIFY readyChanged)
    Q_PROPERTY(bool authenticated READ authenticated NOTIFY authenticatedChanged)
    Q_PROPERTY(bool restoringSession READ restoringSession NOTIFY restoringSessionChanged)
    Q_PROPERTY(bool playing READ playing NOTIFY playbackChanged)
    Q_PROPERTY(bool busy READ busy NOTIFY playbackChanged)
    Q_PROPERTY(QString state READ state NOTIFY playbackChanged)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    Q_PROPERTY(QString title READ title NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString artist READ artist NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString album READ album NOTIFY nowPlayingChanged)
    Q_PROPERTY(QString artwork READ artwork NOTIFY nowPlayingChanged)
    Q_PROPERTY(qint64 positionMs READ positionMs NOTIFY positionChanged)
    Q_PROPERTY(qint64 durationMs READ durationMs NOTIFY positionChanged)
    Q_PROPERTY(double volume READ volume WRITE setVolume NOTIFY volumeChanged)
    Q_PROPERTY(bool shuffle READ shuffle WRITE setShuffle NOTIFY modesChanged)
    Q_PROPERTY(QString repeatMode READ repeatMode NOTIFY modesChanged)
    Q_PROPERTY(MediaModel *queueModel READ queueModel CONSTANT)
    Q_PROPERTY(int currentQueueIndex READ currentQueueIndex NOTIFY queuePositionChanged)
    Q_PROPERTY(QString currentId READ currentId NOTIFY nowPlayingChanged)
    Q_PROPERTY(QVariantList lyrics READ lyrics NOTIFY lyricsChanged)
    Q_PROPERTY(QString lyricsStatus READ lyricsStatus NOTIFY lyricsChanged)
    Q_PROPERTY(bool synchronizedLyrics READ synchronizedLyrics NOTIFY lyricsChanged)
    Q_PROPERTY(bool previewDetected READ previewDetected NOTIFY previewDetectedChanged)

public:
    explicit PlayerController(QObject *parent = nullptr);
    ~PlayerController() override;

    void start();
    void setDemoState();
    bool available() const;
    bool ready() const;
    bool authenticated() const;
    bool restoringSession() const;
    bool playing() const;
    bool busy() const;
    QString state() const;
    QString error() const;
    QString title() const;
    QString artist() const;
    QString album() const;
    QString artwork() const;
    qint64 positionMs() const;
    qint64 durationMs() const;
    double volume() const;
    void setVolume(double volume);
    bool shuffle() const;
    void setShuffle(bool shuffle);
    QString repeatMode() const;
    MediaModel *queueModel();
    int currentQueueIndex() const;
    QString currentId() const;
    QVariantList lyrics() const;
    QString lyricsStatus() const;
    bool synchronizedLyrics() const;
    bool previewDetected() const;

    Q_INVOKABLE void signIn();
    Q_INVOKABLE void signOut();
    Q_INVOKABLE void playPause();
    Q_INVOKABLE void stop();
    Q_INVOKABLE void next();
    Q_INVOKABLE void previous();
    Q_INVOKABLE void seek(qint64 positionMs);
    Q_INVOKABLE void cycleRepeat();
    Q_INVOKABLE void setRepeatMode(const QString &mode);
    Q_INVOKABLE void playSongs(const QStringList &ids, int startIndex = 0);
    Q_INVOKABLE void playNext(const QString &id);
    Q_INVOKABLE void playLater(const QString &id);
    Q_INVOKABLE void playStation(const QString &id);
    Q_INVOKABLE void playQueueIndex(int index);
    Q_INVOKABLE void moveQueueItem(int from, int to);
    Q_INVOKABLE void removeQueueItem(int index);
    Q_INVOKABLE void requestLyrics();
    Q_INVOKABLE void clearError();
    Q_INVOKABLE void restartSidecar();

signals:
    void tokensChanged(const QString &developerToken, const QString &userToken, const QString &storefront);
    void availableChanged();
    void readyChanged();
    void authenticatedChanged();
    void restoringSessionChanged();
    void playbackChanged();
    void nowPlayingChanged();
    void positionChanged();
    void volumeChanged();
    void modesChanged();
    void errorChanged();
    void sidecarExited();
    void queuePositionChanged();
    void lyricsChanged();
    void previewDetectedChanged();

private:
    void send(const QJsonObject &command);
    void handleLine(const QByteArray &line);
    void handleEvent(const QJsonObject &event);
    void setError(const QString &error);
    QString locateSidecar() const;
    QString locateElectron(const QString &sidecar) const;
    void restorePlayerState();
    void persistPlayerState(bool force = false);

    QProcess m_process;
    bool m_trace = false;
    QTimer m_persistTimer;
    QByteArray m_stdoutBuffer;
    MediaModel m_queue;
    bool m_available = false;
    bool m_ready = false;
    bool m_authenticated = false;
    // True while Apple is being asked to turn a saved session back into a token.
    bool m_restoringSession = false;
    QString m_state = QStringLiteral("none");
    QString m_error;
    QString m_title;
    QString m_artist;
    QString m_album;
    QString m_artwork;
    qint64 m_positionMs = 0;
    qint64 m_durationMs = 0;
    double m_volume = 1.0;
    bool m_shuffle = false;
    QString m_repeat = QStringLiteral("none");
    int m_currentQueueIndex = -1;
    QString m_currentId;
    QVariantList m_lyrics;
    QString m_lyricsStatus;
    bool m_synchronizedLyrics = false;
    bool m_previewDetected = false;
    qint64 m_catalogDurationMs = 0;
    QStringList m_savedQueueIds;
    int m_savedQueueIndex = 0;
    qint64 m_savedPositionMs = 0;
    bool m_restorePending = false;
    bool m_shuttingDown = false;
    // Set whenever a persisted value actually changes, so an idle or paused
    // Kadenza stops rewriting an identical config file every few seconds.
    bool m_stateDirty = false;
    int m_restartAttempts = 0;
};
