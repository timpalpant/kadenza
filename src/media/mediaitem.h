#pragma once

#include <QJsonObject>
#include <QString>

struct MediaItem {
  QString id;
  QString catalogId;
  QString artistId;
  QString albumId;
  QString type;
  QString title;
  QString subtitle;
  QString album;
  QString artworkUrl;
  QString url;
  QString dateAdded;
  qint64 durationMs = 0;
  int trackNumber = 0;
  bool explicitContent = false;
  bool streamable = false;
  bool favorite = false;
  bool inLibrary = false;
  /// -1 disliked, 0 unrated, 1 loved.
  int rating = 0;

  [[nodiscard]] QString playbackId() const;
  [[nodiscard]] bool isSong() const;
  [[nodiscard]] bool isStation() const;
  [[nodiscard]] bool isPlayable() const;

  static MediaItem fromJson(const QJsonObject &object);
  static QString artwork(const QString &templateUrl, int size = 512);
};
