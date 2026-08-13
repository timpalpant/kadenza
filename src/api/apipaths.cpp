#include "apipaths.h"

#include <QUrl>
#include <QUrlQuery>

namespace ApiPaths {

QString withLibraryIncludes(const QString &next)
{
    if (next.isEmpty())
        return next;

    QUrl url(next);
    QUrlQuery query(url.query());
    if (query.hasQueryItem(QStringLiteral("include")))
        return next;

    query.addQueryItem(QStringLiteral("include"), QStringLiteral("catalog"));
    url.setQuery(query);
    // Relative links stay relative; ApiClient resolves them against the API
    // host the same way it resolves the first page's path.
    return url.toString();
}

} // namespace ApiPaths
