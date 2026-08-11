#include "mediaitem.h"

#include <QJsonArray>
#include <QJsonObject>

QString MediaItem::playbackId() const
{
    return catalogId.isEmpty() ? id : catalogId;
}

bool MediaItem::isSong() const
{
    return type == QStringLiteral("songs") || type == QStringLiteral("library-songs");
}

bool MediaItem::isStation() const
{
    return type == QStringLiteral("stations");
}

bool MediaItem::isPlayable() const
{
    return isSong() && streamable && !playbackId().isEmpty();
}

QString MediaItem::artwork(const QString &templateUrl, int size)
{
    QString value = templateUrl;
    value.replace(QStringLiteral("{w}"), QString::number(size));
    value.replace(QStringLiteral("{h}"), QString::number(size));
    value.replace(QStringLiteral("{f}"), QStringLiteral("jpg"));
    return value;
}

MediaItem MediaItem::fromJson(const QJsonObject &object)
{
    MediaItem item;
    item.id = object.value(QStringLiteral("id")).toString();
    item.type = object.value(QStringLiteral("type")).toString();
    item.url = object.value(QStringLiteral("href")).toString();

    const auto attributes = object.value(QStringLiteral("attributes")).toObject();
    const auto playParams = attributes.value(QStringLiteral("playParams")).toObject();
    item.catalogId = playParams.value(QStringLiteral("catalogId")).toString();
    const QString playId = playParams.value(QStringLiteral("id")).toString();
    const bool libraryResource = item.type.startsWith(QStringLiteral("library-")) || playParams.value(QStringLiteral("isLibrary")).toBool();
    item.inLibrary = libraryResource;
    item.favorite = attributes.value(QStringLiteral("inFavorites")).toBool() || attributes.value(QStringLiteral("isFavorited")).toBool();
    // Catalog tracks that have not been released yet remain in an album's
    // relationship, but Apple omits their playParams. Library resources need a
    // catalogId; their own i.* id cannot be handed to MusicKit.
    item.streamable = libraryResource ? !item.catalogId.isEmpty() : !playId.isEmpty();
    item.title = attributes.value(QStringLiteral("name")).toString();
    item.album = attributes.value(QStringLiteral("albumName")).toString();
    item.durationMs = attributes.value(QStringLiteral("durationInMillis")).toInteger();
    item.trackNumber = attributes.value(QStringLiteral("trackNumber")).toInt();
    item.dateAdded = attributes.value(QStringLiteral("dateAdded")).toString();
    item.explicitContent = attributes.value(QStringLiteral("contentRating")).toString() == QStringLiteral("explicit");

    const auto relationships = object.value(QStringLiteral("relationships")).toObject();
    const auto relationshipId = [&relationships](const QString &name) {
        const auto data = relationships.value(name).toObject().value(QStringLiteral("data")).toArray();
        return data.isEmpty() ? QString() : data.first().toObject().value(QStringLiteral("id")).toString();
    };
    item.artistId = relationshipId(QStringLiteral("artists"));
    item.albumId = relationshipId(QStringLiteral("albums"));

    // include=catalog attaches the catalog twin of a library resource. Apple
    // puts a catalogId in playParams for library songs but not for library
    // albums or artists, so without this the catalog id of every album in the
    // library was lost — and with it any way to tell that the album already on
    // screen is one the user owns.
    const auto catalogData = relationships.value(QStringLiteral("catalog")).toObject().value(QStringLiteral("data")).toArray();
    const QJsonObject catalogObject = catalogData.isEmpty() ? QJsonObject() : catalogData.first().toObject();
    if (item.catalogId.isEmpty()) {
        item.catalogId = catalogObject.value(QStringLiteral("id")).toString();
        // Streamability was decided above from a catalog id that had not been
        // resolved yet.
        if (libraryResource && !item.catalogId.isEmpty())
            item.streamable = true;
    }

    const QString artist = attributes.value(QStringLiteral("artistName")).toString();
    const QString curator = attributes.value(QStringLiteral("curatorName")).toString();
    if (!artist.isEmpty()) {
        item.subtitle = artist;
    } else if (!curator.isEmpty()) {
        item.subtitle = curator;
    } else {
        const auto genres = attributes.value(QStringLiteral("genreNames")).toArray();
        item.subtitle = genres.isEmpty() ? QString() : genres.first().toString();
    }
    QString artworkTemplate = attributes.value(QStringLiteral("artwork")).toObject().value(QStringLiteral("url")).toString();
    // Library artists generally do not carry artwork themselves.  Apple exposes
    // the corresponding catalog artist (and its artwork) through this
    // relationship when the request uses include=catalog.
    if (!catalogObject.isEmpty()) {
        const auto catalogAttributes = catalogObject.value(QStringLiteral("attributes")).toObject();
        if (artworkTemplate.isEmpty()) {
            artworkTemplate = catalogAttributes.value(QStringLiteral("artwork")).toObject().value(QStringLiteral("url")).toString();
        }
        if (item.artistId.isEmpty()) {
            const auto artists = catalogObject.value(QStringLiteral("relationships"))
                                     .toObject()
                                     .value(QStringLiteral("artists"))
                                     .toObject()
                                     .value(QStringLiteral("data"))
                                     .toArray();
            if (!artists.isEmpty())
                item.artistId = artists.first().toObject().value(QStringLiteral("id")).toString();
        }
        item.favorite = item.favorite || catalogAttributes.value(QStringLiteral("inFavorites")).toBool();
    }
    item.artworkUrl = artwork(artworkTemplate);
    return item;
}
