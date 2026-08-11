#pragma once

#include <QSqlDatabase>
#include <QString>

/**
 * Owns the on-disk SQLite cache and its schema migrations.
 *
 * The cache mirrors the user's Apple Music library so the library and
 * Recently Added views can be rendered from disk immediately, instead of
 * waiting for every page of a paginated API walk.
 */
class Database {
public:
  static Database &instance();

  bool open(const QString &path = {});
  void close();

  [[nodiscard]] QSqlDatabase db() const;
  [[nodiscard]] bool isOpen() const;

  /// Drops every cached resource and its sync bookkeeping, used on sign-out.
  void wipe();

  [[nodiscard]] QString lastError() const { return m_lastError; }

private:
  Database() = default;
  bool createSchema();
  int schemaVersion();
  void setSchemaVersion(int version);

  QString m_connectionName = QStringLiteral("kanzi");
  QString m_lastError;
  bool m_open = false;
};
