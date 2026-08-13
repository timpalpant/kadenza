#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

class ApiClient : public QObject
{
    Q_OBJECT

public:
    explicit ApiClient(QObject *parent = nullptr);

    void setTokens(const QString &developerToken, const QString &userToken, const QString &storefront);
    [[nodiscard]] bool authenticated() const;
    [[nodiscard]] QString storefront() const;
    void setStorefront(const QString &storefront);
    void get(const QString &tag, const QString &path);
    /// A GET whose reply arrives as undecoded bytes, via succeededRaw().
    ///
    /// The library walk moves megabytes of JSON per sync and decoding it is
    /// the expensive half. Handing the bytes over undecoded lets the caller
    /// parse them somewhere other than the GUI thread.
    void getRaw(const QString &tag, const QString &path);
    void post(const QString &tag, const QString &path, const QJsonObject &body = {});
    void put(const QString &tag, const QString &path, const QJsonObject &body = {});
    void del(const QString &tag, const QString &path);

signals:
    void succeeded(const QString &tag, const QJsonDocument &document);
    void succeededRaw(const QString &tag, const QByteArray &body);
    void failed(const QString &tag, int status, const QString &message);

private:
    void request(const QString &tag, const QString &path, const QByteArray &method, const QJsonObject &body = {}, bool raw = false);
    QNetworkAccessManager m_network;
    QString m_developerToken;
    QString m_userToken;
    QString m_storefront = QStringLiteral("us");
};
