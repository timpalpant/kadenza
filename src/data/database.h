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
class Database
{
public:
    static Database &instance();

    bool open(const QString &path = {});
    void close();

    /// This thread's handle on the cache, opened on first use.
    ///
    /// SQLite handles cannot be shared between threads, so each thread that
    /// touches the cache gets its own connection to the same file. WAL is what
    /// makes that worth doing: the sync worker can write while the GUI thread
    /// reads the rows it is displaying.
    [[nodiscard]] QSqlDatabase db() const;
    [[nodiscard]] bool isOpen() const;

    /// Closes and forgets the calling thread's handle. A worker thread must
    /// call this before it exits; a QSqlDatabase that outlives its thread is a
    /// leak Qt complains about at shutdown.
    static void releaseThreadConnection();

    /// Drops every cached resource and its sync bookkeeping, used on sign-out.
    void wipe();

    [[nodiscard]] QString lastError() const { return m_lastError; }

private:
    Database() = default;
    bool createSchema();
    int schemaVersion();
    void setSchemaVersion(int version);
    [[nodiscard]] static QString connectionName();

    QString m_path;
    QString m_lastError;
    bool m_open = false;
};
