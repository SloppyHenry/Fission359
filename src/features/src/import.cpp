// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include <cmath>
#include <memory>
#include <mutex>

#include "fission/features/builtin.h"
#include "fission/geo/error.h"
#include "fission/geo/exchange.h"

#include "primitives.h"

namespace fission::features {

namespace {

using namespace fission::core;

constexpr const char* kStep = "step";
constexpr const char* kIges = "iges";

// A body from a file (STEP or IGES) that is embedded in the project: the file is named in the
// word `asset`, its type in `format`, and `part` says which of its bodies this step is (a file
// with three parts makes three steps). The body is exact but has no history; it is moved by x, y,
// z from where the file puts it. `unitFactor` multiplies every length of the file (25.4 when the
// file said nothing about its unit and was in inches). The faces are named face1, face2, ... in
// the order of the file (ADR-0003).
class ImportType final : public FeatureType {
public:
    std::string_view id() const override { return "import"; }
    int version() const override { return 1; }

    std::vector<ParamSpec> parameters() const override {
        std::vector<ParamSpec> own = {{"part", QuantityKind::plain, "1", true},
                                      {"unitFactor", QuantityKind::plain, "1", true}};
        for (const char* name : {"x", "y", "z"}) {
            own.push_back({name, QuantityKind::length, "0"});
        }
        return own;
    }

    std::vector<TextFieldSpec> texts() const override { return {{"asset", ""}, {"format", kStep}}; }

    std::optional<FeatureIssue> validate(const ResolvedParams& params) const override {
        if (auto issue = requirePositive(params, {"unitFactor"})) {
            return issue;
        }
        const double part = valueOf(params, "part");
        if (!std::isfinite(part) || part < 1.0 || part != std::floor(part)) {
            FeatureIssue issue;
            issue.code = FeatureIssueCode::parameterOutOfRange;
            issue.subject = "part";
            return issue;
        }
        return std::nullopt;
    }

    FeatureResult execute(const ExecuteInput& input, const ExecuteContext& context) const override {
        const std::string& name = textOf(input.feature, "asset", "");
        const auto file =
            context.assets != nullptr ? context.assets->find(name) : AssetMap::const_iterator();
        if (context.assets == nullptr || file == context.assets->end()) {
            throw geo::GeometryError(geo::GeometryErrorCode::importUnreadable,
                                     "import: the embedded file " + name + " is missing");
        }
        const std::string& format = textOf(input.feature, "format", kStep);
        if (format != kStep && format != kIges) {
            throw geo::GeometryError(geo::GeometryErrorCode::importUnreadable,
                                     "import: unknown file format " + format);
        }
        const double factor = valueOf(input.params, "unitFactor");
        const std::shared_ptr<const geo::ImportResult> read =
            readOnce(context.kernel, file->second, format == kIges, factor);
        const auto index = static_cast<std::size_t>(valueOf(input.params, "part")) - 1;
        if (index >= read->bodies.size()) {
            throw geo::GeometryError(geo::GeometryErrorCode::importEmpty,
                                     "import: the file has fewer bodies than this step wants");
        }
        geo::NamedSolid made = read->bodies[index].body;
        const geo::Point3d move{valueOf(input.params, "x"), valueOf(input.params, "y"),
                                valueOf(input.params, "z")};
        if (move != geo::Point3d{}) {
            made = context.kernel.translate(made, move);
        }
        FeatureResult result;
        result.outputs.push_back(
            {input.feature.outputs.at(0), std::move(made.solid), std::move(made.faces)});
        return result;
    }

    Description describe(const ResolvedParams& params) const override {
        return {"import", {params.at("part")}};
    }

    std::vector<HandleSpec> handles(const ResolvedParams&) const override { return {}; }

    // The size handles of the interface scale values; a file's body has none that are lengths,
    // and its unit factor is a correction, not a size.
    std::vector<std::string> sizeParameters() const override { return {}; }

private:
    // A file with many bodies makes many steps that read the same file: the last reading is
    // kept so that it happens once. Keyed by the embedded content (never changes under a name),
    // the format and the unit factor.
    std::shared_ptr<const geo::ImportResult> readOnce(const geo::GeometryKernel& kernel,
                                                      const std::shared_ptr<const Bytes>& bytes,
                                                      bool iges, double factor) const {
        const std::lock_guard<std::mutex> guard(lock_);
        if (last_ && lastBytes_ == bytes && lastIges_ == iges && lastFactor_ == factor) {
            return last_;
        }
        geo::ImportOptions options;
        options.lengthFactor = factor;
        auto read = std::make_shared<const geo::ImportResult>(kernel.importFile(
            iges ? geo::ImportFormat::iges : geo::ImportFormat::step, *bytes, options));
        last_ = read;
        lastBytes_ = bytes;
        lastIges_ = iges;
        lastFactor_ = factor;
        return read;
    }

    mutable std::mutex lock_;
    mutable std::shared_ptr<const geo::ImportResult> last_;
    mutable std::shared_ptr<const Bytes> lastBytes_;
    mutable bool lastIges_ = false;
    mutable double lastFactor_ = 1.0;
};

} // namespace

namespace detail {
std::unique_ptr<const core::FeatureType> makeImportType() {
    return std::make_unique<ImportType>();
}
} // namespace detail

} // namespace fission::features
