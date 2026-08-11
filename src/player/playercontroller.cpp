#include "playercontroller.h"

#include <KConfigGroup>
#include <KSharedConfig>

#include <QCoreApplication>
#include <QDir>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QStandardPaths>
#include <algorithm>

namespace {
// The sidecar is respawned with exponential backoff so a permanently broken
// Electron cannot spin forever at a fixed interval.
// The longest preview asset Apple hands out, with a little slack.
constexpr qint64 kLongestPreviewMs = 95000;
constexpr int kMaxSidecarRestarts = 5;
constexpr int kSidecarRestartBaseMs = 1000;
} // namespace

PlayerController::PlayerController(QObject *parent) : QObject(parent) {
  const KConfigGroup playback(KSharedConfig::openConfig(), "Playback");
  m_volume = playback.readEntry("Volume", 1.0);
  m_shuffle = playback.readEntry("Shuffle", false);
  m_repeat = playback.readEntry("Repeat", QStringLiteral("none"));
  m_savedQueueIds = playback.readEntry("Queue", QStringList{});
  m_savedQueueIndex = playback.readEntry("QueueIndex", 0);
  m_savedPositionMs = playback.readEntry<qint64>("PositionMs", 0);
  m_restorePending = !m_savedQueueIds.isEmpty();
  m_persistTimer.setInterval(5000);
  connect(&m_persistTimer, &QTimer::timeout, this,
          [this] { persistPlayerState(); });
  m_persistTimer.start();
  m_process.setProcessChannelMode(QProcess::SeparateChannels);
  connect(&m_process, &QProcess::readyReadStandardOutput, this, [this] {
    m_stdoutBuffer += m_process.readAllStandardOutput();
    qsizetype newline;
    while ((newline = m_stdoutBuffer.indexOf('\n')) >= 0) {
      const auto line = m_stdoutBuffer.left(newline).trimmed();
      m_stdoutBuffer.remove(0, newline + 1);
      if (!line.isEmpty())
        handleLine(line);
    }
  });
  connect(&m_process, &QProcess::readyReadStandardError, this, [this] {
    const auto output = m_process.readAllStandardError().trimmed();
    if (!output.isEmpty())
      qInfo().noquote() << "kanzi sidecar:" << output;
  });
  connect(
      &m_process, &QProcess::errorOccurred, this,
      [this](QProcess::ProcessError) { setError(m_process.errorString()); });
  connect(&m_process, qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
          this, [this](int, QProcess::ExitStatus) {
            m_available = false;
            m_ready = false;
            Q_EMIT availableChanged();
            Q_EMIT readyChanged();
            Q_EMIT sidecarExited();
            if (m_shuttingDown)
              return;
            if (m_restartAttempts >= kMaxSidecarRestarts) {
              setError(tr("The playback helper keeps stopping. Use Retry in "
                          "Settings once the problem is resolved."));
              return;
            }
            const int delay = kSidecarRestartBaseMs * (1 << m_restartAttempts);
            ++m_restartAttempts;
            m_restorePending = !m_savedQueueIds.isEmpty();
            QTimer::singleShot(delay, this, &PlayerController::start);
          });
}

PlayerController::~PlayerController() {
  m_shuttingDown = true;
  persistPlayerState(true);
  if (m_process.state() != QProcess::NotRunning) {
    send({{QStringLiteral("cmd"), QStringLiteral("quit")}});
    // Closing the pipe is what the sidecar falls back on if the command is
    // never processed, and it is the same signal it would see had Kanzi
    // crashed instead of exited.
    m_process.closeWriteChannel();
    if (!m_process.waitForFinished(1500)) {
      // SIGTERM first: Electron reaps its own helper processes on the way out,
      // whereas SIGKILL leaves the zygote, GPU and network children orphaned.
      m_process.terminate();
      if (!m_process.waitForFinished(1500))
        m_process.kill();
    }
  }
}

QString PlayerController::locateSidecar() const {
  const QString override = qEnvironmentVariable("KANZI_SIDECAR");
  const QStringList candidates = {
      override,
      QStandardPaths::locate(QStandardPaths::GenericDataLocation,
                             QStringLiteral("kanzi/sidecar"),
                             QStandardPaths::LocateDirectory),
      QString::fromUtf8(KANZI_SOURCE_SIDECAR),
  };
  for (const auto &candidate : candidates) {
    if (!candidate.isEmpty() &&
        QFileInfo::exists(candidate + QStringLiteral("/main.js")))
      return candidate;
  }
  return {};
}

QString PlayerController::locateElectron(const QString &sidecar) const {
  const QString override = qEnvironmentVariable("KANZI_ELECTRON");
  if (!override.isEmpty() && QFileInfo::exists(override))
    return override;
  const QString bundled =
      sidecar + QStringLiteral("/node_modules/electron/dist/electron");
  if (QFileInfo::exists(bundled))
    return bundled;
  return {};
}

void PlayerController::start() {
  if (m_process.state() != QProcess::NotRunning)
    return;
  const QString sidecar = locateSidecar();
  const QString electron = locateElectron(sidecar);
  if (sidecar.isEmpty()) {
    setError(tr("The playback sidecar could not be found."));
    return;
  }
  if (electron.isEmpty()) {
    setError(tr("The Widevine-enabled Electron runtime is not installed. Run "
                "npm install in %1.")
                 .arg(sidecar));
    return;
  }
  m_process.setWorkingDirectory(sidecar);
  m_process.start(electron, {QStringLiteral(".")});
  if (!m_process.waitForStarted(3000)) {
    setError(m_process.errorString());
    return;
  }
  m_available = true;
  Q_EMIT availableChanged();
}

void PlayerController::setDemoState() {
  m_available = true;
  m_ready = true;
  m_authenticated = true;
  m_state = QStringLiteral("paused");
  m_title = QStringLiteral("Afterglow");
  m_artist = QStringLiteral("Mira Vale");
  m_album = QStringLiteral("Violet Hours");
  m_artwork = QStringLiteral(
      "qrc:/qt/qml/io/github/timpalpant/kanzi/data/demo/violet-hours.svg");
  m_positionMs = 142000;
  m_durationMs = 238000;
  m_volume = 0.72;
  m_currentQueueIndex = 0;
  m_currentId = QStringLiteral("demo-1");
  m_lyrics = {
      QVariantMap{{QStringLiteral("timeMs"), 0},
                  {QStringLiteral("text"),
                   QStringLiteral("Streetlights dissolve into violet hours")}},
      QVariantMap{{QStringLiteral("timeMs"), 42000},
                  {QStringLiteral("text"),
                   QStringLiteral("Every window holds a different sky")}},
      QVariantMap{{QStringLiteral("timeMs"), 93000},
                  {QStringLiteral("text"),
                   QStringLiteral("We follow the afterglow home")}},
      QVariantMap{{QStringLiteral("timeMs"), 151000},
                  {QStringLiteral("text"),
                   QStringLiteral("Before the colors leave the night")}}};
  m_synchronizedLyrics = true;

  QList<MediaItem> queue;
  for (const auto &[id, title, artist, album, artwork, duration] :
       std::initializer_list<
           std::tuple<const char *, const char *, const char *, const char *,
                      const char *, qint64>>{
           {"demo-1", "Afterglow", "Mira Vale", "Violet Hours",
            "violet-hours.svg", 238000},
           {"demo-2", "Glass Gardens", "North Arcade", "Night Transit",
            "night-transit.svg", 213000},
           {"demo-3", "Blue Current", "The Quiet Shapes", "Tidal Memory",
            "tidal-memory.svg", 266000}}) {
    MediaItem item;
    item.id = QString::fromLatin1(id);
    item.catalogId = item.id;
    item.type = QStringLiteral("songs");
    item.title = QString::fromLatin1(title);
    item.subtitle = QString::fromLatin1(artist);
    item.album = QString::fromLatin1(album);
    item.artworkUrl =
        QStringLiteral("qrc:/qt/qml/io/github/timpalpant/kanzi/data/demo/") +
        QString::fromLatin1(artwork);
    item.durationMs = duration;
    item.streamable = true;
    queue.push_back(std::move(item));
  }
  m_queue.replaceItems(std::move(queue));
  Q_EMIT availableChanged();
  Q_EMIT readyChanged();
  Q_EMIT authenticatedChanged();
  Q_EMIT playbackChanged();
  Q_EMIT nowPlayingChanged();
  Q_EMIT positionChanged();
  Q_EMIT queuePositionChanged();
  Q_EMIT volumeChanged();
  Q_EMIT lyricsChanged();
}

void PlayerController::send(const QJsonObject &command) {
  if (m_process.state() == QProcess::Running) {
    m_process.write(QJsonDocument(command).toJson(QJsonDocument::Compact) +
                    '\n');
  }
}

void PlayerController::handleLine(const QByteArray &line) {
  QJsonParseError error;
  const auto document = QJsonDocument::fromJson(line, &error);
  if (error.error != QJsonParseError::NoError || !document.isObject()) {
    qWarning().noquote() << "kanzi: invalid sidecar message:" << line;
    return;
  }
  handleEvent(document.object());
}

void PlayerController::handleEvent(const QJsonObject &object) {
  const QString event = object.value(QStringLiteral("event")).toString();
  if (event == QStringLiteral("widevine-ready") ||
      event == QStringLiteral("ready")) {
    // The process is alive, but MusicKit is not hooked yet and any command
    // sent now would only be queued. Readiness is claimed at hook-ready.
    m_restartAttempts = 0;
  } else if (event == QStringLiteral("session-cookie")) {
    qInfo().noquote() << "Kanzi Apple session cookie:"
                      << object.value(QStringLiteral("detail")).toString();
  } else if (event == QStringLiteral("session-reauthorizing")) {
    if (!m_restoringSession) {
      m_restoringSession = true;
      Q_EMIT restoringSessionChanged();
    }
  } else if (event == QStringLiteral("session-restore-failed") ||
             event == QStringLiteral("authorization-settled")) {
    if (m_restoringSession) {
      m_restoringSession = false;
      Q_EMIT restoringSessionChanged();
    }
  } else if (event == QStringLiteral("hook-ready")) {
    m_ready = true;
    m_restartAttempts = 0;
    Q_EMIT readyChanged();
    // A freshly reloaded MusicKit instance can report isAuthorized=false for
    // a moment after authorize() has already returned a usable user token.
    // Hook readiness may promote a session, but must never demote one.
    if (object.value(QStringLiteral("authorized")).toBool() &&
        !m_authenticated) {
      m_authenticated = true;
      Q_EMIT authenticatedChanged();
    }
  } else if (event == QStringLiteral("tokens")) {
    const QString developerToken =
        object.value(QStringLiteral("developerToken")).toString();
    const QString userToken =
        object.value(QStringLiteral("musicUserToken")).toString();
    const bool hasTokenPair = !developerToken.isEmpty() && !userToken.isEmpty();
    const bool promote = hasTokenPair && !m_authenticated;
    // Ignore developer-token-only snapshots after sign-in. Apple emits these
    // transiently during web-player reloads; replacing the valid user token
    // with an empty value would send the UI back to onboarding.
    if (hasTokenPair) {
      Q_EMIT tokensChanged(developerToken, userToken,
                           object.value(QStringLiteral("storefront"))
                               .toString(QStringLiteral("us")));
      send({{QStringLiteral("cmd"), QStringLiteral("hide")}});
      restorePlayerState();
    }
    if (promote) {
      m_authenticated = true;
      Q_EMIT authenticatedChanged();
    }
  } else if (event == QStringLiteral("authorization")) {
    // isAuthorized is eventually consistent in the Apple web player. A true
    // event may promote, while explicit `signed-out` below owns demotion.
    if (object.value(QStringLiteral("authorized")).toBool() &&
        !m_authenticated) {
      m_authenticated = true;
      Q_EMIT authenticatedChanged();
    }
  } else if (event == QStringLiteral("playbackState")) {
    m_state = object.value(QStringLiteral("state")).toString();
    Q_EMIT playbackChanged();
  } else if (event == QStringLiteral("nowPlaying")) {
    const auto item = object.value(QStringLiteral("item")).toObject();
    m_title = item.value(QStringLiteral("title")).toString();
    m_artist = item.value(QStringLiteral("artist")).toString();
    m_album = item.value(QStringLiteral("album")).toString();
    m_artwork = MediaItem::artwork(
        item.value(QStringLiteral("artworkTemplate")).toString());
    m_currentId = item.value(QStringLiteral("catalogId")).toString();
    if (m_currentId.isEmpty())
      m_currentId = item.value(QStringLiteral("id")).toString();
    m_catalogDurationMs =
        item.value(QStringLiteral("catalogDurationMs")).toInteger();
    m_lyrics.clear();
    m_lyricsStatus = tr("Loading lyrics…");
    m_synchronizedLyrics = false;
    Q_EMIT lyricsChanged();
    Q_EMIT nowPlayingChanged();
    const auto queue = object.value(QStringLiteral("queue")).toObject();
    if (!queue.isEmpty())
      handleEvent(
          {{QStringLiteral("event"), QStringLiteral("queue")},
           {QStringLiteral("items"), queue.value(QStringLiteral("items"))},
           {QStringLiteral("position"),
            queue.value(QStringLiteral("position"))}});
    requestLyrics();
  } else if (event == QStringLiteral("playbackDiagnostics")) {
    // Logged rather than shown: this is the evidence needed to tell a
    // preview-locked player apart from an unauthorized or unsubscribed one.
    qInfo().noquote()
        << "Kanzi playback:"
        << "previewOnly=" << object.value(QStringLiteral("previewOnly")).toVariant()
        << "supported=" << object.value(QStringLiteral("previewOnlySupported")).toBool()
        << "authorized=" << object.value(QStringLiteral("authorized")).toBool()
        << "userToken=" << object.value(QStringLiteral("hasUserToken")).toBool()
        << "subscription="
        << object.value(QStringLiteral("subscriptionStatus")).toString()
        << "asset="
        << object.value(QStringLiteral("playbackDurationMs")).toInteger()
        << "catalog="
        << object.value(QStringLiteral("catalogDurationMs")).toInteger();
    const QString previewError =
        object.value(QStringLiteral("previewOnlyError")).toString();
    if (!previewError.isEmpty())
      qWarning().noquote() << "Kanzi could not clear preview mode:" << previewError;
  } else if (event == QStringLiteral("position")) {
    m_positionMs = object.value(QStringLiteral("positionMs")).toInteger();
    m_durationMs = object.value(QStringLiteral("durationMs")).toInteger();
    m_savedPositionMs = m_positionMs;
    m_stateDirty = true;
    const bool wasPreview = m_previewDetected;
    // Apple serves both 30-second and 90-second previews depending on the
    // storefront and the track, so a 45-second ceiling never caught the long
    // ones: the user just got a track that stopped at 1:30 with no
    // explanation. What actually identifies a preview is a playable asset
    // materially shorter than the catalog duration of the same track.
    const bool preview = m_durationMs > 0 && m_durationMs <= kLongestPreviewMs &&
                         m_catalogDurationMs > m_durationMs + 15000;
    if (preview != m_previewDetected) {
      m_previewDetected = preview;
      Q_EMIT previewDetectedChanged();
    }
    if (preview) {
      setError(tr("Apple Music supplied a preview instead of the full track. "
                  "Check that this account has an active subscription, then "
                  "sign out and back in if necessary."));
    } else if (wasPreview) {
      setError({});
    }
    Q_EMIT positionChanged();
  } else if (event == QStringLiteral("volume")) {
    m_volume = object.value(QStringLiteral("volume")).toDouble(1.0);
    m_stateDirty = true;
    Q_EMIT volumeChanged();
  } else if (event == QStringLiteral("modes")) {
    m_shuffle = object.value(QStringLiteral("shuffle")).toBool();
    m_repeat =
        object.value(QStringLiteral("repeat")).toString(QStringLiteral("none"));
    m_stateDirty = true;
    Q_EMIT modesChanged();
  } else if (event == QStringLiteral("queue")) {
    const int position = object.value(QStringLiteral("position")).toInt(-1);
    if (position != m_currentQueueIndex) {
      m_currentQueueIndex = position;
      m_savedQueueIndex = qMax(0, position);
      m_stateDirty = true;
      Q_EMIT queuePositionChanged();
    }
    QList<MediaItem> items;
    QStringList ids;
    for (const auto &value : object.value(QStringLiteral("items")).toArray()) {
      const auto source = value.toObject();
      MediaItem item;
      item.id = source.value(QStringLiteral("id")).toString();
      item.catalogId = source.value(QStringLiteral("catalogId")).toString();
      item.artistId = source.value(QStringLiteral("artistId")).toString();
      item.albumId = source.value(QStringLiteral("albumId")).toString();
      item.type = QStringLiteral("songs");
      item.title = source.value(QStringLiteral("title")).toString();
      item.subtitle = source.value(QStringLiteral("artist")).toString();
      item.album = source.value(QStringLiteral("album")).toString();
      item.durationMs = source.value(QStringLiteral("durationMs")).toInteger();
      item.streamable = !item.playbackId().isEmpty();
      item.artworkUrl = MediaItem::artwork(
          source.value(QStringLiteral("artworkTemplate")).toString());
      items.push_back(std::move(item));
      const QString playbackId =
          source.value(QStringLiteral("catalogId")).toString().isEmpty()
              ? source.value(QStringLiteral("id")).toString()
              : source.value(QStringLiteral("catalogId")).toString();
      if (!playbackId.isEmpty())
        ids.push_back(playbackId);
    }
    m_queue.replaceItems(std::move(items));
    if (!ids.isEmpty() && ids != m_savedQueueIds) {
      m_savedQueueIds = ids;
      m_stateDirty = true;
    }
  } else if (event == QStringLiteral("lyrics")) {
    m_lyrics = object.value(QStringLiteral("lines")).toArray().toVariantList();
    m_synchronizedLyrics =
        object.value(QStringLiteral("synchronized")).toBool();
    m_lyricsStatus = object.value(QStringLiteral("status")).toString();
    Q_EMIT lyricsChanged();
  } else if (event == QStringLiteral("library-write")) {
    Q_EMIT actionResult(object.value(QStringLiteral("kind")).toString(),
                        object.value(QStringLiteral("id")).toString(),
                        object.value(QStringLiteral("ok")).toBool(),
                        object.value(QStringLiteral("detail")).toString());
  } else if (event == QStringLiteral("signed-out")) {
    m_authenticated = false;
    m_queue.clear();
    Q_EMIT authenticatedChanged();
    Q_EMIT tokensChanged({}, {}, QStringLiteral("us"));
  } else if (event == QStringLiteral("hook-failed") ||
             event == QStringLiteral("error")) {
    setError(object.value(QStringLiteral("detail"))
                 .toString(tr("Apple Music playback failed.")));
  }
}

bool PlayerController::available() const { return m_available; }
bool PlayerController::ready() const { return m_ready; }
bool PlayerController::authenticated() const { return m_authenticated; }
bool PlayerController::restoringSession() const { return m_restoringSession; }
bool PlayerController::playing() const {
  return m_state == QStringLiteral("playing") ||
         m_state == QStringLiteral("seeking");
}
bool PlayerController::busy() const {
  return m_state == QStringLiteral("loading") ||
         m_state == QStringLiteral("waiting") ||
         m_state == QStringLiteral("stalled");
}
QString PlayerController::state() const { return m_state; }
QString PlayerController::error() const { return m_error; }
QString PlayerController::title() const { return m_title; }
QString PlayerController::artist() const { return m_artist; }
QString PlayerController::album() const { return m_album; }
QString PlayerController::artwork() const { return m_artwork; }
qint64 PlayerController::positionMs() const { return m_positionMs; }
qint64 PlayerController::durationMs() const { return m_durationMs; }
double PlayerController::volume() const { return m_volume; }
bool PlayerController::shuffle() const { return m_shuffle; }
QString PlayerController::repeatMode() const { return m_repeat; }
MediaModel *PlayerController::queueModel() { return &m_queue; }
int PlayerController::currentQueueIndex() const { return m_currentQueueIndex; }
QString PlayerController::currentId() const { return m_currentId; }
QVariantList PlayerController::lyrics() const { return m_lyrics; }
QString PlayerController::lyricsStatus() const { return m_lyricsStatus; }
bool PlayerController::synchronizedLyrics() const {
  return m_synchronizedLyrics;
}
bool PlayerController::previewDetected() const { return m_previewDetected; }

void PlayerController::setError(const QString &error) {
  if (m_error == error)
    return;
  m_error = error;
  Q_EMIT errorChanged();
}

void PlayerController::signIn() {
  if (m_restoringSession) {
    m_restoringSession = false;
    Q_EMIT restoringSessionChanged();
  }
  send({{QStringLiteral("cmd"), QStringLiteral("showLogin")}});
  send({{QStringLiteral("cmd"), QStringLiteral("authorize")}});
}
void PlayerController::signOut() {
  send({{QStringLiteral("cmd"), QStringLiteral("signOut")}});
}
void PlayerController::playPause() {
  send({{QStringLiteral("cmd"), QStringLiteral("playPause")}});
}
void PlayerController::stop() {
  send({{QStringLiteral("cmd"), QStringLiteral("stop")}});
}
void PlayerController::next() {
  send({{QStringLiteral("cmd"), QStringLiteral("next")}});
}
void PlayerController::previous() {
  send({{QStringLiteral("cmd"), QStringLiteral("previous")}});
}
void PlayerController::seek(qint64 positionMs) {
  send({{QStringLiteral("cmd"), QStringLiteral("seek")},
        {QStringLiteral("positionMs"), positionMs}});
}
void PlayerController::setVolume(double volume) {
  volume = std::clamp(volume, 0.0, 1.0);
  send({{QStringLiteral("cmd"), QStringLiteral("setVolume")},
        {QStringLiteral("volume"), volume}});
}
void PlayerController::setShuffle(bool shuffle) {
  send({{QStringLiteral("cmd"), QStringLiteral("setShuffle")},
        {QStringLiteral("shuffle"), shuffle}});
}
void PlayerController::cycleRepeat() {
  const QString nextMode =
      m_repeat == QStringLiteral("none")  ? QStringLiteral("all")
      : m_repeat == QStringLiteral("all") ? QStringLiteral("one")
                                          : QStringLiteral("none");
  setRepeatMode(nextMode);
}
void PlayerController::setRepeatMode(const QString &mode) {
  const QString safeMode =
      mode == QStringLiteral("one") || mode == QStringLiteral("all")
          ? mode
          : QStringLiteral("none");
  send({{QStringLiteral("cmd"), QStringLiteral("setRepeat")},
        {QStringLiteral("mode"), safeMode}});
}
void PlayerController::playSongs(const QStringList &ids, int startIndex) {
  QJsonArray songs;
  for (const auto &id : ids)
    if (!id.isEmpty())
      songs.append(id);
  if (songs.isEmpty())
    return;
  m_savedQueueIds = ids;
  m_savedQueueIndex =
      std::clamp(startIndex, 0, static_cast<int>(songs.size()) - 1);
  m_savedPositionMs = 0;
  m_restorePending = false;
  m_stateDirty = true;
  send({{QStringLiteral("cmd"), QStringLiteral("setQueue")},
        {QStringLiteral("songs"), songs},
        {QStringLiteral("startPosition"),
         std::clamp(startIndex, 0, static_cast<int>(songs.size()) - 1)},
        {QStringLiteral("startPlaying"), true}});
}
void PlayerController::playNext(const QString &id) {
  if (!id.isEmpty())
    send({{QStringLiteral("cmd"), QStringLiteral("playNext")},
          {QStringLiteral("songs"), QJsonArray{id}}});
}
void PlayerController::playLater(const QString &id) {
  if (!id.isEmpty())
    send({{QStringLiteral("cmd"), QStringLiteral("playLater")},
          {QStringLiteral("songs"), QJsonArray{id}}});
}
void PlayerController::playStation(const QString &id) {
  if (id.isEmpty())
    return;
  // A station is an endless stream, so there is no saved queue to restore
  // over it.
  m_savedQueueIds.clear();
  m_savedQueueIndex = 0;
  m_savedPositionMs = 0;
  m_restorePending = false;
  m_stateDirty = true;
  send({{QStringLiteral("cmd"), QStringLiteral("playStation")},
        {QStringLiteral("id"), id}});
}

void PlayerController::playQueueIndex(int index) {
  send({{QStringLiteral("cmd"), QStringLiteral("changeToIndex")},
        {QStringLiteral("index"), index}});
}
void PlayerController::moveQueueItem(int from, int to) {
  if (from == to || from < 0 || to < 0)
    return;
  send({{QStringLiteral("cmd"), QStringLiteral("moveInQueue")},
        {QStringLiteral("from"), from},
        {QStringLiteral("to"), to}});
}
void PlayerController::removeQueueItem(int index) {
  if (index < 0)
    return;
  send({{QStringLiteral("cmd"), QStringLiteral("removeFromQueue")},
        {QStringLiteral("index"), index}});
}
void PlayerController::requestLyrics() {
  if (m_currentId.isEmpty()) {
    m_lyricsStatus = tr("No lyrics available");
    Q_EMIT lyricsChanged();
    return;
  }
  send({{QStringLiteral("cmd"), QStringLiteral("getLyrics")},
        {QStringLiteral("id"), m_currentId}});
}
void PlayerController::setFavorite(const QString &id, const QString &type,
                                   bool favorite) {
  if (!id.isEmpty())
    send({{QStringLiteral("cmd"), favorite ? QStringLiteral("favorite")
                                           : QStringLiteral("unfavorite")},
          {QStringLiteral("id"), id},
          {QStringLiteral("type"), type}});
}
void PlayerController::setInLibrary(const QString &id, const QString &type,
                                    bool inLibrary) {
  if (!id.isEmpty())
    send({{QStringLiteral("cmd"), inLibrary
                                      ? QStringLiteral("addToLibrary")
                                      : QStringLiteral("removeFromLibrary")},
          {QStringLiteral("id"), id},
          {QStringLiteral("type"), type}});
}
void PlayerController::clearError() { setError({}); }

void PlayerController::restartSidecar() {
  m_restartAttempts = 0;
  setError({});
  if (m_process.state() == QProcess::NotRunning)
    start();
}

void PlayerController::restorePlayerState() {
  send({{QStringLiteral("cmd"), QStringLiteral("setVolume")},
        {QStringLiteral("volume"), m_volume}});
  send({{QStringLiteral("cmd"), QStringLiteral("setShuffle")},
        {QStringLiteral("shuffle"), m_shuffle}});
  send({{QStringLiteral("cmd"), QStringLiteral("setRepeat")},
        {QStringLiteral("mode"), m_repeat}});
  if (!m_restorePending || m_savedQueueIds.isEmpty())
    return;
  m_restorePending = false;
  QJsonArray songs;
  for (const auto &id : std::as_const(m_savedQueueIds))
    songs.append(id);
  send({{QStringLiteral("cmd"), QStringLiteral("setQueue")},
        {QStringLiteral("songs"), songs},
        {QStringLiteral("startPosition"),
         std::clamp(m_savedQueueIndex, 0,
                    static_cast<int>(m_savedQueueIds.size()) - 1)},
        {QStringLiteral("startTimeMs"), m_savedPositionMs},
        {QStringLiteral("startPlaying"), false}});
}

void PlayerController::persistPlayerState(bool force) {
  if (!m_stateDirty && !force)
    return;
  m_stateDirty = false;
  auto config = KSharedConfig::openConfig();
  KConfigGroup playback(config, "Playback");
  playback.writeEntry("Volume", m_volume);
  playback.writeEntry("Shuffle", m_shuffle);
  playback.writeEntry("Repeat", m_repeat);
  playback.writeEntry("Queue", m_savedQueueIds);
  playback.writeEntry("QueueIndex", m_savedQueueIndex);
  playback.writeEntry("PositionMs", m_savedPositionMs);
  config->sync();
}
