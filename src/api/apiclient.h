#pragma once

#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkAccessManager>
#include <QObject>

class ApiClient : public QObject {
  Q_OBJECT

public:
  explicit ApiClient(QObject *parent = nullptr);

  void setTokens(const QString &developerToken, const QString &userToken,
                 const QString &storefront);
  [[nodiscard]] bool authenticated() const;
  [[nodiscard]] QString storefront() const;
  void setStorefront(const QString &storefront);
  void get(const QString &tag, const QString &path);
  void post(const QString &tag, const QString &path,
            const QJsonObject &body = {});
  void put(const QString &tag, const QString &path,
           const QJsonObject &body = {});
  void del(const QString &tag, const QString &path);

signals:
  void succeeded(const QString &tag, const QJsonDocument &document);
  void failed(const QString &tag, int status, const QString &message);

private:
  void request(const QString &tag, const QString &path,
               const QByteArray &method, const QJsonObject &body = {});
  QNetworkAccessManager m_network;
  QString m_developerToken;
  QString m_userToken;
  QString m_storefront = QStringLiteral("us");
};
