#pragma once

#include <QString>

namespace ApiPaths {

/**
 * Restores the query parameters Apple drops from a pagination link.
 *
 * A `next` link carries the offset and nothing else. That is fine for albums
 * and songs, whose library resources describe their own artwork, but a library
 * artist has no artwork attribute at all — the picture only exists on the
 * catalog resource reached through `include=catalog`. Following `next`
 * verbatim therefore produced artist images for the first page of the walk and
 * none for any page after it.
 *
 * Anything already present on the link is left alone, so this stays correct if
 * Apple starts echoing our parameters back.
 */
[[nodiscard]] QString withLibraryIncludes(const QString &next);

} // namespace ApiPaths
