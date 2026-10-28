// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/io/fission_file.h"

#include <algorithm>
#include <chrono>
#include <ctime>
#include <fcntl.h>
#include <fstream>
#include <iterator>
#include <map>
#include <memory>
#include <string_view>
#include <unistd.h>
#include <utility>

#include "fission/core/id.h"

#include "atomic_file.h"
#include <minizip-ng/mz.h>
#include <minizip-ng/mz_crypt.h>
#include <minizip-ng/mz_strm.h>
#include <minizip-ng/mz_strm_mem.h>
#include <minizip-ng/mz_zip.h>
#include <minizip-ng/mz_zip_rw.h>
#include <nlohmann/json.hpp>

namespace fission::io {

namespace {

using Bytes = std::vector<std::uint8_t>;
using Json = nlohmann::json;

constexpr const char* manifestName = "manifest.json";
constexpr const char* documentName = "document.json";
constexpr const char* previewName = "preview.png";

// --- small RAII wrappers around minizip-ng's C handles -------------------------------------

struct MemoryStream {
    void* handle = mz_stream_mem_create();
    ~MemoryStream() {
        mz_stream_close(handle);
        mz_stream_mem_delete(&handle);
    }
    MemoryStream() = default;
    MemoryStream(const MemoryStream&) = delete;
    MemoryStream& operator=(const MemoryStream&) = delete;
};

struct ZipWriter {
    void* handle = mz_zip_writer_create();
    ~ZipWriter() { mz_zip_writer_delete(&handle); }
    ZipWriter() = default;
    ZipWriter(const ZipWriter&) = delete;
    ZipWriter& operator=(const ZipWriter&) = delete;
};

struct ZipReader {
    void* handle = mz_zip_reader_create();
    ~ZipReader() { mz_zip_reader_delete(&handle); }
    ZipReader() = default;
    ZipReader(const ZipReader&) = delete;
    ZipReader& operator=(const ZipReader&) = delete;
};

std::string sha256Hex(const Bytes& data) {
    void* sha = mz_crypt_sha_create();
    mz_crypt_sha_set_algorithm(sha, MZ_HASH_SHA256);
    mz_crypt_sha_begin(sha);
    mz_crypt_sha_update(sha, data.data(), static_cast<std::int32_t>(data.size()));
    std::uint8_t digest[MZ_HASH_SHA256_SIZE] = {};
    mz_crypt_sha_end(sha, digest, sizeof digest);
    mz_crypt_sha_delete(&sha);
    static constexpr char digits[] = "0123456789abcdef";
    std::string text = "sha256:";
    for (const std::uint8_t byte : digest) {
        text += digits[byte >> 4];
        text += digits[byte & 0xF];
    }
    return text;
}

Bytes bytesOf(const std::string& text) {
    return Bytes(text.begin(), text.end());
}

std::string nowIso() {
    const std::time_t now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
    std::tm parts{};
    gmtime_r(&now, &parts);
    char buffer[32];
    std::strftime(buffer, sizeof buffer, "%Y-%m-%dT%H:%M:%SZ", &parts);
    return buffer;
}

// --- building and reading the ZIP in memory ------------------------------------------------

bool addEntry(void* writer, const char* name, const Bytes& data) {
    mz_zip_file info{};
    info.filename = name; // always with "/" as separator; ours have none
    info.modified_date = std::time(nullptr);
    info.compression_method = MZ_COMPRESS_METHOD_DEFLATE;
    info.flag = MZ_ZIP_FLAG_UTF8;
    return mz_zip_writer_add_buffer(writer, const_cast<std::uint8_t*>(data.data()),
                                    static_cast<std::int32_t>(data.size()), &info) == MZ_OK;
}

std::optional<Bytes> buildZip(const std::vector<std::pair<std::string, Bytes>>& entries) {
    MemoryStream stream;
    ZipWriter writer;
    if (mz_stream_open(stream.handle, nullptr, MZ_OPEN_MODE_CREATE | MZ_OPEN_MODE_WRITE) != MZ_OK ||
        mz_zip_writer_open(writer.handle, stream.handle, 0) != MZ_OK) {
        return std::nullopt;
    }
    mz_zip_writer_set_compress_method(writer.handle, MZ_COMPRESS_METHOD_DEFLATE);
    for (const auto& [name, data] : entries) {
        if (!addEntry(writer.handle, name.c_str(), data)) {
            return std::nullopt;
        }
    }
    if (mz_zip_writer_close(writer.handle) != MZ_OK) {
        return std::nullopt;
    }
    const void* buffer = nullptr;
    const std::int32_t length = mz_stream_mem_get_buffer(stream.handle, &buffer);
    std::int32_t size = 0;
    mz_stream_mem_get_buffer_length(stream.handle, &size);
    (void)length;
    if (buffer == nullptr || size <= 0) {
        return std::nullopt;
    }
    const auto* begin = static_cast<const std::uint8_t*>(buffer);
    return Bytes(begin, begin + size);
}

/// Reads every entry of a ZIP held in memory. nullopt when it is not a readable ZIP.
std::optional<std::map<std::string, Bytes>> readZip(const Bytes& zip) {
    ZipReader reader;
    if (zip.empty() ||
        mz_zip_reader_open_buffer(reader.handle, const_cast<std::uint8_t*>(zip.data()),
                                  static_cast<std::int32_t>(zip.size()), 0) != MZ_OK) {
        return std::nullopt;
    }
    std::map<std::string, Bytes> entries;
    std::int32_t status = mz_zip_reader_goto_first_entry(reader.handle);
    while (status == MZ_OK) {
        mz_zip_file* info = nullptr;
        if (mz_zip_reader_entry_get_info(reader.handle, &info) != MZ_OK || info == nullptr) {
            mz_zip_reader_close(reader.handle);
            return std::nullopt;
        }
        if (mz_zip_reader_entry_is_dir(reader.handle) != MZ_OK) {
            const std::string name = info->filename;
            Bytes data(static_cast<std::size_t>(info->uncompressed_size));
            if (mz_zip_reader_entry_open(reader.handle) != MZ_OK) {
                mz_zip_reader_close(reader.handle);
                return std::nullopt;
            }
            const std::int32_t got =
                data.empty() ? 0
                             : mz_zip_reader_entry_read(reader.handle, data.data(),
                                                        static_cast<std::int32_t>(data.size()));
            const bool closed = mz_zip_reader_entry_close(reader.handle) == MZ_OK;
            if (got != static_cast<std::int32_t>(data.size()) || !closed) {
                mz_zip_reader_close(reader.handle);
                return std::nullopt; // cut off or CRC error inside the ZIP
            }
            entries[name] = std::move(data);
        }
        status = mz_zip_reader_goto_next_entry(reader.handle);
    }
    mz_zip_reader_close(reader.handle);
    if (status != MZ_END_OF_LIST) {
        return std::nullopt;
    }
    return entries;
}

std::optional<Bytes> readFile(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    Bytes data((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    if (stream.bad()) {
        return std::nullopt;
    }
    return data;
}

SaveResult refused(FileProblemCode code) {
    return {false, FileProblem{code, false}};
}

} // namespace

SaveResult save(const core::Document& document, const std::filesystem::path& path,
                const SaveExtras& extras, const SaveOptions& options) {
    const Bytes documentBytes = bytesOf(core::toJson(document));
    Json checksums = {{documentName, sha256Hex(options.wrongChecksum ? Bytes{0} : documentBytes)}};
    if (!extras.previewPng.empty()) {
        checksums[previewName] = sha256Hex(extras.previewPng);
    }
    const std::string now = nowIso();
    const Json manifest = {{"formatVersion", core::currentFormatVersion},
                           {"appVersion", extras.appVersion},
                           {"created", extras.created.empty() ? now : extras.created},
                           {"modified", now},
                           {"checksums", std::move(checksums)}};

    std::vector<std::pair<std::string, Bytes>> entries;
    entries.emplace_back(manifestName, bytesOf(manifest.dump(2)));
    entries.emplace_back(documentName, documentBytes);
    if (!extras.previewPng.empty()) {
        entries.emplace_back(previewName, extras.previewPng);
    }
    const auto zip = buildZip(entries);
    if (!zip || !writeFileAtomically(path, *zip, options.failAfterBytes)) {
        return refused(FileProblemCode::cannotWrite);
    }
    return {true, std::nullopt};
}

OpenResult open(const std::filesystem::path& path, const OpenOptions& options) {
    OpenResult result;
    const auto fail = [&](FileProblemCode code, bool readableAnyway = false) {
        result.document.reset();
        result.problems.push_back({code, readableAnyway});
        return std::move(result);
    };

    const auto file = readFile(path);
    if (!file) {
        return fail(FileProblemCode::cannotRead);
    }
    const auto entries = readZip(*file);
    if (!entries || !entries->contains(manifestName) || !entries->contains(documentName)) {
        return fail(FileProblemCode::notAFissionFile);
    }

    const Bytes& manifestBytes = entries->at(manifestName);
    const Json manifest = Json::parse(manifestBytes.begin(), manifestBytes.end(), nullptr, false);
    if (!manifest.is_object() || !manifest.contains("checksums") ||
        !manifest["checksums"].is_object()) {
        return fail(FileProblemCode::notAFissionFile);
    }
    const auto text = [&](const char* key) {
        const auto found = manifest.find(key);
        return found != manifest.end() && found->is_string() ? found->get<std::string>()
                                                             : std::string();
    };
    result.info.formatVersion = manifest.value("formatVersion", 0);
    result.info.appVersion = text("appVersion");
    result.info.created = text("created");
    result.info.modified = text("modified");

    // Every part that has a checksum must match it, and the document must have one.
    bool damaged = !manifest["checksums"].contains(documentName);
    for (const auto& [name, sum] : manifest["checksums"].items()) {
        const auto found = entries->find(name);
        if (found == entries->end() || !sum.is_string() ||
            sum.get<std::string>() != sha256Hex(found->second)) {
            damaged = true;
        }
    }

    const Bytes& documentBytes = entries->at(documentName);
    core::LoadResult loaded = core::fromJson(std::string_view(
        reinterpret_cast<const char*>(documentBytes.data()), documentBytes.size()));
    result.documentProblems = loaded.problems;
    const bool readable = loaded.document && !loaded.hasFatalProblem();
    if (damaged) {
        result.problems.push_back({FileProblemCode::damaged, readable});
    }
    if (!readable) {
        result.problems.push_back({FileProblemCode::documentUnreadable, false});
        return result;
    }
    if (damaged && !options.openDamaged) {
        return result; // no document: the person decides about "Open anyway"
    }
    if (std::any_of(loaded.problems.begin(), loaded.problems.end(), [](const core::LoadProblem& p) {
            return p.code == core::LoadProblemCode::newerFormat;
        })) {
        result.problems.push_back({FileProblemCode::newerFormat, false});
    }
    if (const auto preview = entries->find(previewName); preview != entries->end()) {
        result.previewPng = preview->second;
    }
    result.document = std::move(loaded.document);
    return result;
}

std::vector<std::uint8_t> readPreview(const std::filesystem::path& path) {
    const auto file = readFile(path);
    if (!file) {
        return {};
    }
    const auto entries = readZip(*file);
    if (!entries) {
        return {};
    }
    const auto preview = entries->find(previewName);
    return preview == entries->end() ? Bytes{} : preview->second;
}

} // namespace fission::io
