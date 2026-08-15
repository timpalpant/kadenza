#include "mediamodel.h"
#include "mediaitem.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class MediaModelTest : public QObject
{
    Q_OBJECT

private slots:
    void parsesLibrarySong();
    void replacesArtworkTemplate();
    void usesCatalogArtworkForLibraryArtist();
    void exposesPlaybackIds();
    void marksUnreleasedCatalogTrackUnavailable();
    void parsesRelationshipsAndFlags();
    void updatesLibraryStatus();
    void appliesRatingsByLibraryAndCatalogId();
    void insertsAlphabeticallyByTitle();
};

static QJsonObject song(const QString &id, const QString &catalogId)
{
    return {{QStringLiteral("id"), id},
            {QStringLiteral("type"), QStringLiteral("library-songs")},
            {QStringLiteral("attributes"),
             QJsonObject{{QStringLiteral("name"), QStringLiteral("Track")},
                         {QStringLiteral("artistName"), QStringLiteral("Artist")},
                         {QStringLiteral("durationInMillis"), 123000},
                         {QStringLiteral("playParams"), QJsonObject{{QStringLiteral("catalogId"), catalogId}}}}}};
}

void MediaModelTest::parsesLibrarySong()
{
    const auto item = MediaItem::fromJson(song(QStringLiteral("i.123"), QStringLiteral("456")));
    QCOMPARE(item.id, QStringLiteral("i.123"));
    QCOMPARE(item.playbackId(), QStringLiteral("456"));
    QCOMPARE(item.title, QStringLiteral("Track"));
    QCOMPARE(item.durationMs, 123000);
    QVERIFY(item.isSong());
    QVERIFY(item.isPlayable());
}

void MediaModelTest::replacesArtworkTemplate()
{
    QCOMPARE(MediaItem::artwork(QStringLiteral("https://example/{w}x{h}.{f}"), 256), QStringLiteral("https://example/256x256.jpg"));
}

void MediaModelTest::usesCatalogArtworkForLibraryArtist()
{
    const QJsonObject artist{
        {QStringLiteral("id"), QStringLiteral("r.1")},
        {QStringLiteral("type"), QStringLiteral("library-artists")},
        {QStringLiteral("attributes"), QJsonObject{{QStringLiteral("name"), QStringLiteral("Artist")}}},
        {QStringLiteral("relationships"),
         QJsonObject{{QStringLiteral("catalog"),
                      QJsonObject{{QStringLiteral("data"),
                                   QJsonArray{QJsonObject{
                                       {QStringLiteral("attributes"),
                                        QJsonObject{{QStringLiteral("artwork"),
                                                     QJsonObject{{QStringLiteral("url"), QStringLiteral("https://example/{w}x{h}.{f}")}}}}}}}}}}}}};

    QCOMPARE(MediaItem::fromJson(artist).artworkUrl, QStringLiteral("https://example/512x512.jpg"));
}

void MediaModelTest::exposesPlaybackIds()
{
    MediaModel model;
    model.replace(QJsonArray{song(QStringLiteral("i.1"), QStringLiteral("11")), song(QStringLiteral("i.2"), QStringLiteral("22"))});
    QCOMPARE(model.rowCount(), 2);
    QCOMPARE(model.playbackIds(), QStringList({QStringLiteral("11"), QStringLiteral("22")}));
}

void MediaModelTest::marksUnreleasedCatalogTrackUnavailable()
{
    const auto track = [](const QString &id, bool released) {
        QJsonObject attributes{{QStringLiteral("name"), QStringLiteral("Track")}};
        if (released) {
            attributes.insert(QStringLiteral("playParams"),
                              QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("kind"), QStringLiteral("song")}});
        }
        return QJsonObject{{QStringLiteral("id"), id}, {QStringLiteral("type"), QStringLiteral("songs")}, {QStringLiteral("attributes"), attributes}};
    };
    MediaModel model;
    model.replace({track(QStringLiteral("available"), true), track(QStringLiteral("coming-soon"), false)});

    QCOMPARE(model.playableCount(), 1);
    QCOMPARE(model.playbackIds(), QStringList{QStringLiteral("available")});
    QVERIFY(model.get(0).value(QStringLiteral("playable")).toBool());
    QVERIFY(!model.get(1).value(QStringLiteral("playable")).toBool());
}

void MediaModelTest::parsesRelationshipsAndFlags()
{
    QJsonObject object = song(QStringLiteral("i.123"), QStringLiteral("456"));
    auto attributes = object.value(QStringLiteral("attributes")).toObject();
    attributes.insert(QStringLiteral("inFavorites"), true);
    object.insert(QStringLiteral("attributes"), attributes);
    object.insert(QStringLiteral("relationships"),
                  QJsonObject{{QStringLiteral("artists"),
                               QJsonObject{{QStringLiteral("data"), QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("artist-1")}}}}}},
                              {QStringLiteral("albums"),
                               QJsonObject{{QStringLiteral("data"), QJsonArray{QJsonObject{{QStringLiteral("id"), QStringLiteral("album-1")}}}}}}});

    const auto item = MediaItem::fromJson(object);
    QCOMPARE(item.artistId, QStringLiteral("artist-1"));
    QCOMPARE(item.albumId, QStringLiteral("album-1"));
    QVERIFY(item.favorite);
    QVERIFY(item.inLibrary);
}

void MediaModelTest::updatesLibraryStatus()
{
    MediaModel model;
    model.replace(QJsonArray{song(QStringLiteral("i.123"), QStringLiteral("456"))});
    model.setFavorite(QStringLiteral("456"), true);
    model.setInLibrary(QStringLiteral("456"), false);
    QCOMPARE(model.indexOfId(QStringLiteral("456")), 0);
    QVERIFY(model.get(0).value(QStringLiteral("favorite")).toBool());
    QVERIFY(!model.get(0).value(QStringLiteral("inLibrary")).toBool());
}

void MediaModelTest::appliesRatingsByLibraryAndCatalogId()
{
    MediaModel model;
    model.replace(QJsonArray{song(QStringLiteral("i.1"), QStringLiteral("cat-1")),
                             song(QStringLiteral("i.2"), QStringLiteral("cat-2")),
                             song(QStringLiteral("i.3"), QStringLiteral("cat-3"))});

    QSignalSpy changed(&model, &MediaModel::dataChanged);
    // A ratings reply is keyed by whichever id Apple was asked about, so both
    // spellings have to land, and rows the reply says nothing about stay put.
    model.setRatings({{QStringLiteral("cat-1"), 1}, {QStringLiteral("i.2"), -1}});

    QCOMPARE(model.get(0).value(QStringLiteral("rating")).toInt(), 1);
    QCOMPARE(model.get(1).value(QStringLiteral("rating")).toInt(), -1);
    QCOMPARE(model.get(2).value(QStringLiteral("rating")).toInt(), 0);
    // Only the two rows that actually changed are announced.
    QCOMPARE(changed.count(), 2);

    // Re-applying the same ratings is a no-op, so nothing is announced again.
    changed.clear();
    model.setRatings({{QStringLiteral("cat-1"), 1}, {QStringLiteral("i.2"), -1}});
    QCOMPARE(changed.count(), 0);
}

void MediaModelTest::insertsAlphabeticallyByTitle()
{
    const auto item = [](const QString &id, const QString &title) {
        MediaItem media;
        media.id = id;
        media.catalogId = id;
        media.type = QStringLiteral("library-songs");
        media.title = title;
        return media;
    };
    MediaModel model;
    model.replaceItems({item(QStringLiteral("i.1"), QStringLiteral("Alpha")), item(QStringLiteral("i.2"), QStringLiteral("Zulu"))});

    model.insertAlphabetically(item(QStringLiteral("i.3"), QStringLiteral("Mike")));
    model.insertAlphabetically(item(QStringLiteral("i.4"), QStringLiteral("alpha")));

    QCOMPARE(model.get(0).value(QStringLiteral("title")).toString(), QStringLiteral("Alpha"));
    QCOMPARE(model.get(1).value(QStringLiteral("title")).toString(), QStringLiteral("alpha"));
    QCOMPARE(model.get(2).value(QStringLiteral("title")).toString(), QStringLiteral("Mike"));
    QCOMPARE(model.get(3).value(QStringLiteral("title")).toString(), QStringLiteral("Zulu"));
}

QTEST_GUILESS_MAIN(MediaModelTest)
#include "mediamodeltest.moc"
