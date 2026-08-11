#include "database.h"

#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>

namespace {

// Stamped into the file so a future release can tell what it is looking at.
// Nothing has shipped yet, so there is no older shape to migrate from: the
// schema below is simply created as it stands.
constexpr int CurrentSchemaVersion = 1;

const char *const CreateStatements[] = {
    R"(CREATE TABLE IF NOT EXISTS meta (
        key   TEXT PRIMARY KEY,
        value TEXT
    ))",

    // One row per library resource. `kind` partitions the table into the four
    // library collections; `position` preserves the order Apple returned so
    // the plain library views match the web player, while `date_added` backs
    // Recently Added without a second copy of the data.
    R"(CREATE TABLE IF NOT EXISTS library_items (
        kind             TEXT NOT NULL,
        id               TEXT NOT NULL,
        catalog_id       TEXT NOT NULL DEFAULT '',
        artist_id        TEXT NOT NULL DEFAULT '',
        album_id         TEXT NOT NULL DEFAULT '',
        type             TEXT NOT NULL DEFAULT '',
        title            TEXT NOT NULL DEFAULT '',
        subtitle         TEXT NOT NULL DEFAULT '',
        album            TEXT NOT NULL DEFAULT '',
        artwork_url      TEXT NOT NULL DEFAULT '',
        url              TEXT NOT NULL DEFAULT '',
        date_added       TEXT NOT NULL DEFAULT '',
        duration_ms      INTEGER NOT NULL DEFAULT 0,
        track_number     INTEGER NOT NULL DEFAULT 0,
        explicit_content INTEGER NOT NULL DEFAULT 0,
        streamable       INTEGER NOT NULL DEFAULT 0,
        favorite         INTEGER NOT NULL DEFAULT 0,
        rating           INTEGER NOT NULL DEFAULT 0,
        in_library       INTEGER NOT NULL DEFAULT 0,
        position         INTEGER NOT NULL DEFAULT 0,
        sync_epoch       INTEGER NOT NULL DEFAULT 0,
        PRIMARY KEY (kind, id)
    ))",
    "CREATE INDEX IF NOT EXISTS idx_library_position ON library_items(kind, "
    "position)",
    "CREATE INDEX IF NOT EXISTS idx_library_date ON library_items(kind, "
    "date_added DESC)",
    "CREATE INDEX IF NOT EXISTS idx_library_catalog ON "
    "library_items(catalog_id)",
};

} // namespace

Database &Database::instance() {
  static Database self;
  return self;
}

QSqlDatabase Database::db() const {
  return QSqlDatabase::database(m_connectionName);
}

bool Database::isOpen() const { return m_open; }

bool Database::open(const QString &path) {
  QString file = path;
  if (file.isEmpty()) {
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dir);
    file = dir + QStringLiteral("/kanzi.sqlite");
  }

  QSqlDatabase database =
      QSqlDatabase::contains(m_connectionName)
          ? QSqlDatabase::database(m_connectionName)
          : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"),
                                      m_connectionName);
  database.setDatabaseName(file);

  if (!database.open()) {
    m_lastError = database.lastError().text();
    qWarning() << "kanzi: cannot open cache" << file << m_lastError;
    return false;
  }

  QSqlQuery pragma(database);
  // WAL keeps reads responsive while a sync writes, and NORMAL is the right
  // durability trade-off for a cache that can always be rebuilt from Apple.
  pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
  pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));

  m_open = true;
  return createSchema();
}

void Database::close() {
  if (m_open) {
    QSqlDatabase::database(m_connectionName).close();
    m_open = false;
  }
}

int Database::schemaVersion() {
  QSqlQuery query(db());
  if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
    return query.value(0).toInt();
  return 0;
}

void Database::setSchemaVersion(int version) {
  QSqlQuery query(db());
  query.exec(QStringLiteral("PRAGMA user_version=%1").arg(version));
}

bool Database::createSchema() {
  QSqlDatabase database = db();
  database.transaction();

  for (const char *statement : CreateStatements) {
    QSqlQuery query(database);
    if (!query.exec(QLatin1String(statement))) {
      m_lastError = query.lastError().text();
      qWarning() << "kanzi: schema error" << m_lastError
                 << QLatin1String(statement).left(60);
      database.rollback();
      return false;
    }
  }

  database.commit();

  if (schemaVersion() != CurrentSchemaVersion)
    setSchemaVersion(CurrentSchemaVersion);
  return true;
}

void Database::wipe() {
  if (!m_open)
    return;
  QSqlDatabase database = db();
  database.transaction();
  for (const auto &table :
       {QStringLiteral("library_items"), QStringLiteral("meta")}) {
    QSqlQuery query(database);
    query.exec(QStringLiteral("DELETE FROM %1").arg(table));
  }
  database.commit();
}
