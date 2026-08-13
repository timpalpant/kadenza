#include "database.h"

#include <QDebug>
#include <QDir>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QThread>

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
    // `id` is the second column of the primary key, which cannot serve a
    // lookup that does not also name `kind`. The rating, favourite and
    // in-library updates all match on `id OR catalog_id`, and without this
    // every one of them scanned the whole table.
    "CREATE INDEX IF NOT EXISTS idx_library_id ON library_items(id)",
};

/// Connection settings every handle needs, whichever thread opened it.
void applyPragmas(QSqlDatabase &database)
{
    QSqlQuery pragma(database);
    // WAL keeps reads responsive while a sync writes, and NORMAL is the right
    // durability trade-off for a cache that can always be rebuilt from Apple.
    pragma.exec(QStringLiteral("PRAGMA journal_mode=WAL"));
    pragma.exec(QStringLiteral("PRAGMA synchronous=NORMAL"));
    // WAL allows one writer at a time. The GUI thread still writes the odd
    // rating or favourite while the worker walks the library, so let a
    // collision wait rather than fail.
    pragma.exec(QStringLiteral("PRAGMA busy_timeout=5000"));
}

} // namespace

Database &Database::instance()
{
    static Database self;
    return self;
}

QString Database::connectionName()
{
    // One connection per thread, named after it. Qt refuses to hand a handle
    // opened on one thread to another, so the name has to distinguish them.
    return QStringLiteral("kadenza-%1").arg(reinterpret_cast<quintptr>(QThread::currentThread()), 0, 16);
}

QSqlDatabase Database::db() const
{
    const QString name = connectionName();
    if (QSqlDatabase::contains(name))
        return QSqlDatabase::database(name);

    // A thread that has not touched the cache yet opens its own handle. Only
    // possible once the main thread has been through open() and established
    // where the file lives and what schema it holds.
    if (!m_open || m_path.isEmpty())
        return {};

    QSqlDatabase database = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(m_path);
    if (!database.open()) {
        qWarning() << "kadenza: cannot open cache on this thread" << m_path << database.lastError().text();
        QSqlDatabase::removeDatabase(name);
        return {};
    }
    applyPragmas(database);
    return database;
}

void Database::releaseThreadConnection()
{
    const QString name = connectionName();
    if (!QSqlDatabase::contains(name))
        return;
    // The handle has to be out of scope before the connection is removed, or
    // Qt warns that it is still in use.
    {
        QSqlDatabase database = QSqlDatabase::database(name);
        database.close();
    }
    QSqlDatabase::removeDatabase(name);
}

bool Database::isOpen() const
{
    return m_open;
}

bool Database::open(const QString &path)
{
    QString file = path;
    if (file.isEmpty()) {
        const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
        QDir().mkpath(dir);
        file = dir + QStringLiteral("/kadenza.sqlite");
    }

    const QString name = connectionName();
    QSqlDatabase database = QSqlDatabase::contains(name) ? QSqlDatabase::database(name)
                                                         : QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), name);
    database.setDatabaseName(file);

    if (!database.open()) {
        m_lastError = database.lastError().text();
        qWarning() << "kadenza: cannot open cache" << file << m_lastError;
        return false;
    }

    applyPragmas(database);

    m_path = file;
    m_open = true;
    return createSchema();
}

void Database::close()
{
    if (m_open) {
        releaseThreadConnection();
        m_open = false;
    }
}

int Database::schemaVersion()
{
    QSqlQuery query(db());
    if (query.exec(QStringLiteral("PRAGMA user_version")) && query.next())
        return query.value(0).toInt();
    return 0;
}

void Database::setSchemaVersion(int version)
{
    QSqlQuery query(db());
    query.exec(QStringLiteral("PRAGMA user_version=%1").arg(version));
}

bool Database::createSchema()
{
    QSqlDatabase database = db();
    database.transaction();

    for (const char *statement : CreateStatements) {
        QSqlQuery query(database);
        if (!query.exec(QLatin1String(statement))) {
            m_lastError = query.lastError().text();
            qWarning() << "kadenza: schema error" << m_lastError << QLatin1String(statement).left(60);
            database.rollback();
            return false;
        }
    }

    database.commit();

    if (schemaVersion() != CurrentSchemaVersion)
        setSchemaVersion(CurrentSchemaVersion);
    return true;
}

void Database::wipe()
{
    if (!m_open)
        return;
    QSqlDatabase database = db();
    database.transaction();
    for (const auto &table : {QStringLiteral("library_items"), QStringLiteral("meta")}) {
        QSqlQuery query(database);
        query.exec(QStringLiteral("DELETE FROM %1").arg(table));
    }
    database.commit();
}
