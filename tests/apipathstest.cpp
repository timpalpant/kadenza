#include "apipaths.h"

#include <QTest>

class ApiPathsTest : public QObject
{
    Q_OBJECT

private slots:
    void addsCatalogIncludeToPaginationLinks();
    void leavesExistingIncludeAlone();
    void handlesEmptyAndAbsoluteLinks();
};

void ApiPathsTest::addsCatalogIncludeToPaginationLinks()
{
    // The shape Apple actually returns: the offset survives, our parameters do
    // not. Without the include, artists past the first page arrive with no
    // artwork, because a library artist carries none of its own.
    const QString next = ApiPaths::withLibraryIncludes(QStringLiteral("/v1/me/library/artists?offset=100"));
    QVERIFY(next.contains(QStringLiteral("offset=100")));
    QVERIFY(next.contains(QStringLiteral("include=catalog")));
    QVERIFY(next.startsWith(QStringLiteral("/v1/me/library/artists?")));
}

void ApiPathsTest::leavesExistingIncludeAlone()
{
    const QString already = QStringLiteral("/v1/me/library/albums?offset=200&include=catalog");
    QCOMPARE(ApiPaths::withLibraryIncludes(already), already);

    // A different include is Apple's to decide; do not second-guess it.
    const QString other = QStringLiteral("/v1/me/library/albums?offset=200&include=tracks");
    QCOMPARE(ApiPaths::withLibraryIncludes(other), other);
}

void ApiPathsTest::handlesEmptyAndAbsoluteLinks()
{
    QCOMPARE(ApiPaths::withLibraryIncludes(QString()), QString());

    const QString absolute = ApiPaths::withLibraryIncludes(QStringLiteral("https://api.music.apple.com/v1/me/library/artists?offset=100"));
    QVERIFY(absolute.startsWith(QStringLiteral("https://api.music.apple.com/")));
    QVERIFY(absolute.contains(QStringLiteral("include=catalog")));
}

QTEST_GUILESS_MAIN(ApiPathsTest)
#include "apipathstest.moc"
