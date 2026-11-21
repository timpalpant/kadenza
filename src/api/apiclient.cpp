#include "apiclient.h"

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QUrl>

ApiClient::ApiClient(QObject *parent)
    : QObject(parent)
{}

void ApiClient::setTokens(const QString &developerToken, const QString &userToken, const QString &storefront)
{
    m_developerToken = developerToken;
    m_userToken = userToken;
    if (!storefront.isEmpty())
        m_storefront = storefront;
}

bool ApiClient::authenticated() const
{
    return !m_developerToken.isEmpty() && !m_userToken.isEmpty();
}

QString ApiClient::storefront() const
{
    return m_storefront;
}
void ApiClient::setStorefront(const QString &storefront)
{
    if (!storefront.isEmpty())
        m_storefront = storefront;
}

void ApiClient::get(const QString &tag, const QString &path)
{
    request(tag, path, QByteArrayLiteral("GET"));
}

void ApiClient::getRaw(const QString &tag, const QString &path)
{
    request(tag, path, QByteArrayLiteral("GET"), {}, true);
}

void ApiClient::post(const QString &tag, const QString &path, const QJsonObject &body)
{
    request(tag, path, QByteArrayLiteral("POST"), body);
}

void ApiClient::put(const QString &tag, const QString &path, const QJsonObject &body)
{
    request(tag, path, QByteArrayLiteral("PUT"), body);
}

void ApiClient::del(const QString &tag, const QString &path)
{
    request(tag, path, QByteArrayLiteral("DELETE"));
}

void ApiClient::request(const QString &tag, const QString &path, const QByteArray &method, const QJsonObject &body, bool raw)
{
    QUrl url(path);
    if (url.isRelative())
        url = QUrl(QStringLiteral("https://api.music.apple.com") + (path.startsWith('/') ? path : '/' + path));

    if (qEnvironmentVariableIsSet("KADENZA_TRACE"))
        qInfo().noquote() << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")) << "kadenza: request tag=" << tag << method
                          << url.path();
    QNetworkRequest request(url);
    request.setRawHeader("Authorization", "Bearer " + m_developerToken.toUtf8());
    // Apple answers catalog search with 429 "Request is forbidden" (code
    // 42900) when the web-player developer token is paired with a Music User
    // Token, even on the first request of a session. Catalog search needs no
    // user context, so it goes out with the developer token alone.
    const bool catalogSearch = url.path().startsWith(QStringLiteral("/v1/catalog/")) && url.path().endsWith(QStringLiteral("/search"));
    if (!m_userToken.isEmpty() && !catalogSearch)
        request.setRawHeader("Music-User-Token", m_userToken.toUtf8());
    // The developer token exposed by music.apple.com is scoped to Apple's web
    // player. Apple rejects it unless these browser-origin headers accompany
    // the request, even when the Music User Token is valid.
    request.setRawHeader("Origin", "https://music.apple.com");
    request.setRawHeader("Referer", "https://music.apple.com/");
    request.setRawHeader("Accept", "application/json");
    if (!body.isEmpty())
        request.setHeader(QNetworkRequest::ContentTypeHeader, QStringLiteral("application/json"));

    auto *reply
        = method == QByteArrayLiteral("GET")
              ? m_network.get(request)
              : m_network.sendCustomRequest(request, method, body.isEmpty() ? QByteArray() : QJsonDocument(body).toJson(QJsonDocument::Compact));
    connect(reply, &QNetworkReply::finished, this, [this, reply, tag, raw] {
        const QByteArray body = reply->readAll();
        const int status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
        const auto error = reply->error();
        const QString networkError = reply->errorString();
        reply->deleteLater();

        if (error != QNetworkReply::NoError || status < 200 || status >= 300) {
            // Error bodies are small, so decoding one here costs nothing even
            // on a request that asked for raw bytes.
            QString message = networkError;
            const auto errors = QJsonDocument::fromJson(body).object().value(QStringLiteral("errors")).toArray();
            if (!errors.isEmpty()) {
                const auto object = errors.first().toObject();
                message = object.value(QStringLiteral("detail")).toString(object.value(QStringLiteral("title")).toString(message));
            }
            if (qEnvironmentVariableIsSet("KADENZA_TRACE")) {
                QStringList headers;
                for (const auto &pair : reply->rawHeaderPairs())
                    headers << QString::fromUtf8(pair.first + ": " + pair.second);
                qInfo().noquote() << QDateTime::currentDateTime().toString(QStringLiteral("hh:mm:ss.zzz")) << "kadenza: response tag=" << tag
                                  << "status=" << status << "headers=" << headers.join(QStringLiteral("; "))
                                  << "body=" << QString::fromUtf8(body.left(500));
            }
            Q_EMIT failed(tag, status, message);
            return;
        }

        if (raw) {
            Q_EMIT succeededRaw(tag, body);
            return;
        }

        QJsonParseError parseError;
        const auto document = body.isEmpty() ? QJsonDocument(QJsonObject{}) : QJsonDocument::fromJson(body, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            Q_EMIT failed(tag, status, parseError.errorString());
            return;
        }
        Q_EMIT succeeded(tag, document);
    });
}
