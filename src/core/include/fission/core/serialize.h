// SPDX-FileCopyrightText: Fission3D contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "fission/core/document.h"

#include <nlohmann/json_fwd.hpp>

namespace fission::core {

/// The version of `document.json` this program writes (docs/03-architektur.md §7).
inline constexpr int currentFormatVersion = 1;

/// What went wrong or looked odd while reading. Codes only: the interface turns them into
/// sentences (rule 5), nothing is built as text here.
enum class LoadProblemCode {
    notJson,           ///< Not readable as JSON (empty, cut off, garbage).
    wrongType,         ///< A value has the wrong kind (a number where text belongs).
    missingField,      ///< A required field is absent.
    unknownField,      ///< A field this version does not know (file from a newer version); ignored.
    unknownValue,      ///< A word out of a known list that is not on it; fatal.
    newerFormat,       ///< `formatVersion` is newer than this program; read as far as possible.
    unsupportedFormat, ///< `formatVersion` is missing, zero or negative without a migration.
    inconsistent,      ///< Ids repeat or are empty, or the marker is out of range; fatal.
};

struct LoadProblem {
    LoadProblemCode code = LoadProblemCode::notJson;
    /// Where, as a JSON pointer like "/features/2/params/width" (for the log and for support).
    std::string path;

    bool operator==(const LoadProblem&) const = default;
};

struct LoadResult {
    /// Empty when the file could not be used; `problems` then says why. A document with only
    /// harmless problems (unknown fields) is still returned.
    std::optional<Document> document;
    std::vector<LoadProblem> problems;

    /// True when a problem makes the content untrustworthy (everything but unknown fields and a
    /// newer format version).
    bool hasFatalProblem() const;
};

/// The whole document as `document.json` text (UTF-8). Every field that is content is written
/// (title, settings, measures, steps, bodies, step counter, time marker); the undo list, the
/// revision and the id generator are not. Expressions stay text, exactly as typed.
std::string toJson(const Document& document);

/// Reads `document.json` text. Never throws and never crashes on bad input. Ids, numbers and
/// order are kept as written. Older format versions are lifted step by step to the current one.
LoadResult fromJson(std::string_view text);

/// Lifts a parsed document one format version up: `migrations[v]` turns version v into v + 1,
/// in place. The table of the program is empty while the format is at version 1.
using MigrationTable = std::map<int, std::function<void(nlohmann::json&)>>;

/// Like `fromJson`, with a table of migrations to use instead of the built-in one. For tests
/// that run a made-up migration from a made-up older version.
LoadResult fromJsonWithMigrations(std::string_view text, const MigrationTable& migrations,
                                  int targetVersion);

} // namespace fission::core
