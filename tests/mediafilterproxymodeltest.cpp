#include "mediafilterproxymodel.h"
#include "mediamodel.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QSignalSpy>
#include <QTest>

class MediaFilterProxyModelTest : public QObject
{
    Q_OBJECT

private slots:
    void acceptsEverythingWithoutFilter();
    void matchesTitleSubtitleAndAlbum();
    void isCaseInsensitive();
    void emptyFilterShowsAll();
    void reactsToFilterChanges();
};

static QJsonObject song(const QString &name, const QString &artist, const QString &album)
{
    return {{QStringLiteral("id"), name},
            {QStringLiteral("type"), QStringLiteral("library-songs")},
            {QStringLiteral("attributes"),
             QJsonObject{{QStringLiteral("name"), name},
                         {QStringLiteral("artistName"), artist},
                         {QStringLiteral("albumName"), album},
                         {QStringLiteral("playParams"), QJsonObject{{QStringLiteral("catalogId"), name}}}}}};
}

void MediaFilterProxyModelTest::acceptsEverythingWithoutFilter()
{
    MediaModel source;
    source.replace(QJsonArray{song(QStringLiteral("Alpha"), QStringLiteral("Artist A"), QStringLiteral("Album A")),
                              song(QStringLiteral("Beta"), QStringLiteral("Artist B"), QStringLiteral("Album B"))});
    MediaFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    QCOMPARE(proxy.rowCount(), 2);
}

void MediaFilterProxyModelTest::matchesTitleSubtitleAndAlbum()
{
    MediaModel source;
    source.replace(QJsonArray{song(QStringLiteral("Alpha"), QStringLiteral("Artist A"), QStringLiteral("Album A")),
                              song(QStringLiteral("Beta"), QStringLiteral("Artist B"), QStringLiteral("Album B"))});
    MediaFilterProxyModel proxy;
    proxy.setSourceModel(&source);

    proxy.setFilterString(QStringLiteral("Alp"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), MediaModel::TitleRole).toString(), QStringLiteral("Alpha"));

    proxy.setFilterString(QStringLiteral("Artist B"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), MediaModel::TitleRole).toString(), QStringLiteral("Beta"));

    proxy.setFilterString(QStringLiteral("Album A"));
    QCOMPARE(proxy.rowCount(), 1);
    QCOMPARE(proxy.data(proxy.index(0, 0), MediaModel::TitleRole).toString(), QStringLiteral("Alpha"));
}

void MediaFilterProxyModelTest::isCaseInsensitive()
{
    MediaModel source;
    source.replace(QJsonArray{song(QStringLiteral("Alpha"), QStringLiteral("Artist A"), QStringLiteral("Album A"))});
    MediaFilterProxyModel proxy;
    proxy.setSourceModel(&source);

    proxy.setFilterString(QStringLiteral("alpha"));
    QCOMPARE(proxy.rowCount(), 1);
}

void MediaFilterProxyModelTest::emptyFilterShowsAll()
{
    MediaModel source;
    source.replace(QJsonArray{song(QStringLiteral("Alpha"), QStringLiteral("Artist A"), QStringLiteral("Album A"))});
    MediaFilterProxyModel proxy;
    proxy.setSourceModel(&source);
    proxy.setFilterString(QStringLiteral("   "));
    QCOMPARE(proxy.rowCount(), 1);
}

void MediaFilterProxyModelTest::reactsToFilterChanges()
{
    MediaModel source;
    source.replace(QJsonArray{song(QStringLiteral("Alpha"), QStringLiteral("Artist A"), QStringLiteral("Album A")),
                              song(QStringLiteral("Beta"), QStringLiteral("Artist B"), QStringLiteral("Album B"))});
    MediaFilterProxyModel proxy;
    proxy.setSourceModel(&source);

    QSignalSpy countSpy(&proxy, &MediaFilterProxyModel::countChanged);
    proxy.setFilterString(QStringLiteral("Alpha"));
    QCOMPARE(proxy.count(), 1);
    QCOMPARE(countSpy.count(), 1);

    proxy.setFilterString(QStringLiteral(""));
    QCOMPARE(proxy.count(), 2);
    QCOMPARE(countSpy.count(), 2);
}

QTEST_GUILESS_MAIN(MediaFilterProxyModelTest)
#include "mediafilterproxymodeltest.moc"