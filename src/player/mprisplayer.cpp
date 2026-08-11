#include "mprisplayer.h"

#include "playercontroller.h"

#include <QCoreApplication>
#include <QDBusConnection>
#include <QDBusError>
#include <QDBusMessage>
#include <QGuiApplication>
#include <QWindow>
#include <algorithm>

MprisRootAdaptor::MprisRootAdaptor(QObject *parent)
    : QDBusAbstractAdaptor(parent) {}

void MprisRootAdaptor::Raise() {
  for (auto *window : QGuiApplication::topLevelWindows()) {
    window->show();
    window->raise();
    window->requestActivate();
    break;
  }
}

void MprisRootAdaptor::Quit() { QCoreApplication::quit(); }

MprisPlayerAdaptor::MprisPlayerAdaptor(PlayerController *player,
                                       QObject *parent)
    : QDBusAbstractAdaptor(parent), m_player(player) {
  // A Qt NOTIFY signal is not what D-Bus clients listen to. Nothing turns it
  // into org.freedesktop.DBus.Properties.PropertiesChanged on its own, so
  // Plasma's media applet read the properties once when Kanzi appeared on the
  // bus and was never told about anything afterwards: no track, no progress,
  // dead buttons. Each change now announces exactly what it touched.
  connect(player, &PlayerController::playbackChanged, this, [this] {
    announce({{QStringLiteral("PlaybackStatus"), playbackStatus()}});
  });
  connect(player, &PlayerController::nowPlayingChanged, this, [this] {
    announce({{QStringLiteral("Metadata"), metadata()},
              {QStringLiteral("PlaybackStatus"), playbackStatus()}});
  });
  connect(player, &PlayerController::volumeChanged, this,
          [this] { announce({{QStringLiteral("Volume"), volume()}}); });
  connect(player, &PlayerController::modesChanged, this, [this] {
    announce({{QStringLiteral("Shuffle"), shuffle()},
              {QStringLiteral("LoopStatus"), loopStatus()}});
  });
  // Position is deliberately absent above: the spec excludes it from
  // PropertiesChanged, because a player ticking once a second would flood the
  // bus. Clients extrapolate it themselves and resynchronise on Seeked, which
  // only a jump warrants.
  connect(player, &PlayerController::positionChanged, this, [this] {
    const qlonglong now = position();
    const qlonglong expected = m_lastPositionUs;
    m_lastPositionUs = now;
    if (std::llabs(now - expected) > kSeekToleranceUs)
      Q_EMIT Seeked(now);
  });
}

void MprisPlayerAdaptor::announce(const QVariantMap &properties) {
  QDBusMessage changed = QDBusMessage::createSignal(
      QStringLiteral("/org/mpris/MediaPlayer2"),
      QStringLiteral("org.freedesktop.DBus.Properties"),
      QStringLiteral("PropertiesChanged"));
  changed << QStringLiteral("org.mpris.MediaPlayer2.Player") << properties
          << QStringList();
  QDBusConnection::sessionBus().send(changed);
}

QString MprisPlayerAdaptor::playbackStatus() const {
  if (m_player->playing())
    return QStringLiteral("Playing");
  return m_player->title().isEmpty() ? QStringLiteral("Stopped")
                                     : QStringLiteral("Paused");
}

QString MprisPlayerAdaptor::loopStatus() const {
  if (m_player->repeatMode() == QStringLiteral("one"))
    return QStringLiteral("Track");
  if (m_player->repeatMode() == QStringLiteral("all"))
    return QStringLiteral("Playlist");
  return QStringLiteral("None");
}

void MprisPlayerAdaptor::setLoopStatus(const QString &status) {
  const QString wanted =
      status == QStringLiteral("Track")      ? QStringLiteral("one")
      : status == QStringLiteral("Playlist") ? QStringLiteral("all")
                                             : QStringLiteral("none");
  m_player->setRepeatMode(wanted);
}

bool MprisPlayerAdaptor::shuffle() const { return m_player->shuffle(); }
void MprisPlayerAdaptor::setShuffle(bool shuffle) {
  m_player->setShuffle(shuffle);
}
double MprisPlayerAdaptor::volume() const { return m_player->volume(); }
void MprisPlayerAdaptor::setVolume(double volume) {
  m_player->setVolume(volume);
}
qlonglong MprisPlayerAdaptor::position() const {
  return m_player->positionMs() * 1000;
}

// The spec wants a distinct path per track, and clients use a change of it to
// notice that the track changed. A single fixed path told them every track was
// the same one. Apple's ids carry dots, which object paths do not allow.
QDBusObjectPath MprisPlayerAdaptor::trackId() const {
  const QString id = m_player->currentId();
  QString path;
  path.reserve(id.size());
  for (const QChar character : id) {
    const char16_t code = character.unicode();
    const bool usable = (code >= u'a' && code <= u'z') ||
                        (code >= u'A' && code <= u'Z') ||
                        (code >= u'0' && code <= u'9') || code == u'_';
    path += usable ? character : QLatin1Char('_');
  }
  if (path.isEmpty())
    return QDBusObjectPath(QStringLiteral("/io/github/timpalpant/kanzi/none"));
  return QDBusObjectPath(QStringLiteral("/io/github/timpalpant/kanzi/track/") +
                         path);
}

QVariantMap MprisPlayerAdaptor::metadata() const {
  QVariantMap result;
  result.insert(QStringLiteral("mpris:trackid"),
                QVariant::fromValue(trackId()));
  result.insert(QStringLiteral("mpris:length"), m_player->durationMs() * 1000);
  result.insert(QStringLiteral("xesam:title"), m_player->title());
  result.insert(QStringLiteral("xesam:artist"),
                QStringList{m_player->artist()});
  result.insert(QStringLiteral("xesam:album"), m_player->album());
  if (!m_player->artwork().isEmpty())
    result.insert(QStringLiteral("mpris:artUrl"), m_player->artwork());
  return result;
}

void MprisPlayerAdaptor::Next() { m_player->next(); }
void MprisPlayerAdaptor::Previous() { m_player->previous(); }
void MprisPlayerAdaptor::Pause() {
  if (m_player->playing())
    m_player->playPause();
}
void MprisPlayerAdaptor::PlayPause() { m_player->playPause(); }
void MprisPlayerAdaptor::Stop() { m_player->stop(); }
void MprisPlayerAdaptor::Play() {
  if (!m_player->playing())
    m_player->playPause();
}
void MprisPlayerAdaptor::Seek(qlonglong offset) {
  m_player->seek(
      std::max<qlonglong>(0, m_player->positionMs() + offset / 1000));
  Q_EMIT Seeked(position());
}
void MprisPlayerAdaptor::SetPosition(const QDBusObjectPath &,
                                     qlonglong newPosition) {
  m_player->seek(std::max<qlonglong>(0, newPosition / 1000));
  Q_EMIT Seeked(position());
}
void MprisPlayerAdaptor::OpenUri(const QString &) {}

MprisPlayer::MprisPlayer(PlayerController *player, QObject *parent)
    : QObject(parent) {
  new MprisRootAdaptor(this);
  new MprisPlayerAdaptor(player, this);
  auto bus = QDBusConnection::sessionBus();
  if (!bus.registerObject(QStringLiteral("/org/mpris/MediaPlayer2"), this,
                          QDBusConnection::ExportAdaptors))
    qWarning() << "Could not export the Kanzi MPRIS object:" << bus.lastError();
  if (!bus.registerService(QStringLiteral("org.mpris.MediaPlayer2.kanzi")))
    qWarning() << "Could not register the Kanzi MPRIS service:"
               << bus.lastError();
}
