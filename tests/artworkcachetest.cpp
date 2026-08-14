#include "artworkcache.h"

#include <QBuffer>
#include <QDir>
#include <QImage>
#include <QNetworkAccessManager>
#include <QNetworkDiskCache>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QTest>

namespace {

/// The smallest HTTP server that will satisfy Qt's image loader, so the test
/// never depends on Apple's CDN being reachable.
class ImageServer : public QTcpServer
{
public:
    int requests = 0;

    explicit ImageServer(QObject *parent = nullptr)
        : QTcpServer(parent)
    {
        QImage image(2, 2, QImage::Format_RGB32);
        image.fill(Qt::blue);
        QBuffer buffer(&m_png);
        buffer.open(QIODevice::WriteOnly);
        image.save(&buffer, "PNG");
    }

protected:
    void incomingConnection(qintptr handle) override
    {
        auto *socket = new QTcpSocket(this);
        socket->setSocketDescriptor(handle);
        connect(socket, &QTcpSocket::readyRead, this, [this, socket] {
            if (!socket->readAll().contains("\r\n\r\n"))
                return;
            ++requests;
            // Explicitly cacheable: QNetworkDiskCache stores nothing without a
            // freshness signal, which is the behaviour under test.
            QByteArray header = "HTTP/1.1 200 OK\r\n"
                                "Content-Type: image/png\r\n"
                                "Cache-Control: max-age=3600\r\n"
                                "Content-Length: "
                                + QByteArray::number(m_png.size()) + "\r\n\r\n";
            socket->write(header);
            socket->write(m_png);
            socket->flush();
        });
    }

private:
    QByteArray m_png;
};

} // namespace

class ArtworkCacheTest : public QObject
{
    Q_OBJECT

private slots:
    void storesQmlImageLoadsOnDisk();
    void givesEveryManagerItsOwnCacheDirectory();
};

void ArtworkCacheTest::givesEveryManagerItsOwnCacheDirectory()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    ArtworkCachingFactory factory(dir.filePath(QStringLiteral("artwork")), 16 * 1024 * 1024);

    // Qt asks the factory for a manager per thread that loads images — at
    // minimum the QML engine's and QQuickPixmapReader's. Two QNetworkDiskCache
    // instances sharing one directory corrupt each other's entries, which
    // surfaced as "Unable to read image data" on artwork that downloaded fine.
    QObject owner;
    auto *first = factory.create(&owner);
    auto *second = factory.create(&owner);
    auto *firstCache = qobject_cast<QNetworkDiskCache *>(first->cache());
    auto *secondCache = qobject_cast<QNetworkDiskCache *>(second->cache());
    QVERIFY(firstCache);
    QVERIFY(secondCache);
    QVERIFY(!firstCache->cacheDirectory().isEmpty());
    QVERIFY(firstCache->cacheDirectory() != secondCache->cacheDirectory());
}

void ArtworkCacheTest::storesQmlImageLoadsOnDisk()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    ImageServer server;
    QVERIFY(server.listen(QHostAddress::LocalHost));
    const QString url = QStringLiteral("http://127.0.0.1:%1/artwork.png").arg(server.serverPort());

    ArtworkCachingFactory factory(dir.filePath(QStringLiteral("artwork")), 16 * 1024 * 1024);

    const auto loadOnce = [&] {
        QQmlApplicationEngine engine;
        engine.setNetworkAccessManagerFactory(&factory);
        QQmlComponent component(&engine);
        component.setData(QStringLiteral("import QtQuick\nImage { source: \"%1\" }").arg(url).toUtf8(), QUrl());
        std::unique_ptr<QObject> image(component.create());
        QVERIFY(image);
        QTRY_COMPARE(image->property("status").toInt(), 1 /* Image.Ready */);
    };

    loadOnce();
    QCOMPARE(server.requests, 1);

    // The factory is consulted and the response is written through to disk.
    QVERIFY(!QDir(factory.cacheDirectory()).isEmpty());

    // A second engine starts with an empty in-memory pixmap cache, so without
    // the disk cache this would go back to the server. This is the regression
    // that made revisiting a library page re-download every tile.
    loadOnce();
    QCOMPARE(server.requests, 1);
}

QTEST_MAIN(ArtworkCacheTest)
#include "artworkcachetest.moc"
