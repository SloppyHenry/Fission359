// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

#include "fission/core/document.h"
#include "fission/core/serialize.h"

namespace fission::io {

/// Why saving or opening did not work. Codes only; the interface turns them into sentences
/// from the message catalogue (rule 5).
enum class FileProblemCode {
    cannotWrite,        ///< The target folder or file cannot be written (or the disk is full).
    cannotRead,         ///< The file cannot be read.
    notAFissionFile,    ///< Not a ZIP, or no `manifest.json` / `document.json` inside.
    damaged,            ///< A checksum does not match: the file was changed or is broken.
    documentUnreadable, ///< `document.json` is not usable; `documentProblems` says why.
    newerFormat,        ///< Saved by a newer program; opened as far as understood.
};

struct FileProblem {
    FileProblemCode code = FileProblemCode::cannotRead;
    /// For `damaged`: true when `document.json` can be read anyway, so the interface may offer
    /// "Open anyway" (`OpenOptions::openDamaged`).
    bool readableAnyway = false;
};

/// What goes into the file besides the document.
struct SaveExtras {
    std::vector<std::uint8_t> previewPng; ///< `preview.png`; empty = none.
    std::string appVersion;               ///< Written to the manifest.
    /// When the file was first saved (ISO 8601, UTC); empty = now. Saving over a file passes the
    /// old value on so that it stays.
    std::string created;
};

/// Switches for tests.
struct SaveOptions {
    /// Simulates a crash: only this many bytes of the new file get written, then saving stops
    /// as if the program had died. Never set outside tests.
    std::optional<std::size_t> failAfterBytes;
    /// Writes a wrong checksum for `document.json`, as a damaged file would have. Never set
    /// outside tests.
    bool wrongChecksum = false;
};

struct SaveResult {
    bool ok = false;
    std::optional<FileProblem> problem;
};

/// Writes the document as a `.fission` file: a ZIP with `manifest.json`, `document.json` and
/// `preview.png` (docs/03-architektur.md §7). Atomic: everything goes to a temporary file in the
/// same folder, which is flushed to disk and then renamed over the target. Whatever fails on the
/// way, the old file is untouched and no temporary file stays behind.
SaveResult save(const core::Document& document, const std::filesystem::path& path,
                const SaveExtras& extras, const SaveOptions& options = {});

struct OpenOptions {
    /// Open although a checksum does not match, when the document can be read (the person
    /// chose "Open anyway").
    bool openDamaged = false;
};

/// What the manifest says.
struct FileInfo {
    int formatVersion = 0;
    std::string appVersion;
    std::string created;
    std::string modified;
};

struct OpenResult {
    std::optional<core::Document> document; ///< Empty when the file cannot be used.
    std::vector<FileProblem> problems;
    std::vector<core::LoadProblem> documentProblems; ///< From reading `document.json`.
    FileInfo info;
    std::vector<std::uint8_t> previewPng;
};

OpenResult open(const std::filesystem::path& path, const OpenOptions& options = {});

/// Only the small picture of a `.fission` file (`preview.png`), for the tiles of the start page;
/// empty when the file has none or cannot be read.
std::vector<std::uint8_t> readPreview(const std::filesystem::path& path);

} // namespace fission::io
