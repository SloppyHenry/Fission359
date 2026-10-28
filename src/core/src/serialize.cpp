// SPDX-FileCopyrightText: Fission359 contributors
// SPDX-License-Identifier: GPL-3.0-or-later

#include "fission/core/serialize.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <set>
#include <utility>

#include <nlohmann/json.hpp>

namespace fission::core {

namespace {

using Json = nlohmann::json;

// --- words for the enums ------------------------------------------------------------------
// Saved as words, never as numbers: reordering an enum must not change what a file means.

template <class E, std::size_t N> using WordTable = std::array<std::pair<E, const char*>, N>;

constexpr WordTable<QuantityKind, 3> quantityWords{{{QuantityKind::length, "length"},
                                                    {QuantityKind::angle, "angle"},
                                                    {QuantityKind::plain, "plain"}}};
constexpr WordTable<BodyKind, 2> bodyKindWords{
    {{BodyKind::precise, "precise"}, {BodyKind::free, "free"}}};
constexpr WordTable<ElementKind, 2> elementKindWords{
    {{ElementKind::face, "face"}, {ElementKind::edge, "edge"}}};
constexpr WordTable<ElementShape, 10> elementShapeWords{{{ElementShape::unknown, "unknown"},
                                                         {ElementShape::plane, "plane"},
                                                         {ElementShape::cylinder, "cylinder"},
                                                         {ElementShape::cone, "cone"},
                                                         {ElementShape::sphere, "sphere"},
                                                         {ElementShape::torus, "torus"},
                                                         {ElementShape::freeform, "freeform"},
                                                         {ElementShape::line, "line"},
                                                         {ElementShape::circle, "circle"},
                                                         {ElementShape::curve, "curve"}}};

template <class E, std::size_t N> const char* wordOf(const WordTable<E, N>& table, E value) {
    for (const auto& [e, word] : table) {
        if (e == value) {
            return word;
        }
    }
    return table.front().second;
}

// --- writing ------------------------------------------------------------------------------

std::string hexOf(Color color) {
    static constexpr char digits[] = "0123456789abcdef";
    std::string text = "#";
    for (const std::uint8_t channel : {color.red, color.green, color.blue}) {
        text += digits[channel >> 4];
        text += digits[channel & 0xF];
    }
    return text;
}

Json pointJson(const geo::Point3d& point) {
    return Json::array({point.x, point.y, point.z});
}

Json lookJson(const ElementLook& look) {
    Json neighbourLooks = Json::array();
    for (const NeighbourLook& n : look.neighbourLooks) {
        neighbourLooks.push_back({{"name", n.name},
                                  {"shape", wordOf(elementShapeWords, n.shape)},
                                  {"centroid", pointJson(n.centroid)},
                                  {"direction", pointJson(n.direction)},
                                  {"hasDirection", n.hasDirection},
                                  {"directed", n.directed}});
    }
    return {{"kind", wordOf(elementKindWords, look.kind)},
            {"shape", wordOf(elementShapeWords, look.shape)},
            {"centroid", pointJson(look.centroid)},
            {"direction", pointJson(look.direction)},
            {"hasDirection", look.hasDirection},
            {"directed", look.directed},
            {"size", look.size},
            {"radius", look.radius},
            {"neighbours", look.neighbours},
            {"neighbourLooks", std::move(neighbourLooks)}};
}

Json refJson(const PersistentRef& ref) {
    return {{"kind", wordOf(elementKindWords, ref.kind)},
            {"name", ref.name},
            {"look", lookJson(ref.look)}};
}

Json valueJson(const Value& value) {
    return {{"expression", value.expression}, {"kind", wordOf(quantityWords, value.kind)}};
}

Json featureJson(const Feature& feature) {
    Json params = Json::object();
    for (const auto& [name, value] : feature.params) {
        params[name] = valueJson(value);
    }
    Json texts = Json::object();
    for (const auto& [name, text] : feature.texts) {
        texts[name] = text;
    }
    Json refs = Json::object();
    for (const auto& [name, list] : feature.refs) {
        Json array = Json::array();
        for (const PersistentRef& ref : list) {
            array.push_back(refJson(ref));
        }
        refs[name] = std::move(array);
    }
    Json inputs = Json::array();
    for (const BodyId& id : feature.inputs) {
        inputs.push_back(id.value);
    }
    Json outputs = Json::array();
    for (const BodyId& id : feature.outputs) {
        outputs.push_back(id.value);
    }
    return {{"id", feature.id.value},
            {"number", feature.number},
            {"type", feature.type},
            {"typeVersion", feature.typeVersion},
            {"label", feature.label},
            {"params", std::move(params)},
            {"texts", std::move(texts)},
            {"refs", std::move(refs)},
            {"inputs", std::move(inputs)},
            {"outputs", std::move(outputs)},
            {"suppressed", feature.suppressed}};
}

Json bodyJson(const Body& body) {
    Json exceptions = Json::array();
    for (const BodyId& id : body.cutExceptions) {
        exceptions.push_back(id.value);
    }
    return {{"id", body.id.value},
            {"name", body.name},
            {"color", hexOf(body.color)},
            {"visible", body.visible},
            {"hole", body.hole},
            {"kind", wordOf(bodyKindWords, body.kind)},
            {"createdBy", body.createdBy.value},
            {"cutExceptions", std::move(exceptions)}};
}

// --- reading ------------------------------------------------------------------------------

/// A problem that stops the reading.
struct Fatal {
    LoadProblem problem;
};

using Problems = std::vector<LoadProblem>;

std::string childPath(const std::string& path, const std::string& key) {
    // JSON pointer escaping (RFC 6901).
    std::string escaped;
    for (const char c : key) {
        if (c == '~') {
            escaped += "~0";
        } else if (c == '/') {
            escaped += "~1";
        } else {
            escaped += c;
        }
    }
    return path + "/" + escaped;
}

[[noreturn]] void fail(LoadProblemCode code, std::string path) {
    throw Fatal{{code, std::move(path)}};
}

/// Reads the fields of one JSON object; whatever it was not asked for is reported as unknown.
class ObjectReader {
public:
    ObjectReader(const Json& json, std::string path, Problems& problems)
        : json_(json), path_(std::move(path)), problems_(problems) {
        if (!json.is_object()) {
            fail(LoadProblemCode::wrongType, path_);
        }
    }

    const Json& field(const std::string& key) {
        seen_.insert(key);
        const auto found = json_.find(key);
        if (found == json_.end()) {
            fail(LoadProblemCode::missingField, childPath(path_, key));
        }
        return *found;
    }

    std::string path(const std::string& key) const { return childPath(path_, key); }

    std::string text(const std::string& key) {
        const Json& value = field(key);
        if (!value.is_string()) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        return value.get<std::string>();
    }

    bool flag(const std::string& key) {
        const Json& value = field(key);
        if (!value.is_boolean()) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        return value.get<bool>();
    }

    int integer(const std::string& key) {
        const Json& value = field(key);
        if (!value.is_number_integer()) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        const auto number = value.get<std::int64_t>();
        if (number < INT32_MIN || number > INT32_MAX) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        return static_cast<int>(number);
    }

    double number(const std::string& key) {
        const Json& value = field(key);
        if (!value.is_number()) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        return value.get<double>();
    }

    const Json& array(const std::string& key) {
        const Json& value = field(key);
        if (!value.is_array()) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        return value;
    }

    const Json& object(const std::string& key) {
        const Json& value = field(key);
        if (!value.is_object()) {
            fail(LoadProblemCode::wrongType, path(key));
        }
        return value;
    }

    template <class E, std::size_t N> E word(const std::string& key, const WordTable<E, N>& table) {
        const std::string text = this->text(key);
        for (const auto& [value, name] : table) {
            if (text == name) {
                return value;
            }
        }
        fail(LoadProblemCode::unknownValue, path(key));
    }

    /// Call when every field is read: reports the ones nobody asked for.
    void finish() {
        for (const auto& item : json_.items()) {
            if (!seen_.contains(item.key())) {
                problems_.push_back({LoadProblemCode::unknownField, path(item.key())});
            }
        }
    }

private:
    const Json& json_;
    std::string path_;
    Problems& problems_;
    std::set<std::string> seen_;
};

Color colorFrom(const std::string& text, const std::string& path) {
    const auto digit = [&](char c) -> int {
        if (c >= '0' && c <= '9') {
            return c - '0';
        }
        if (c >= 'a' && c <= 'f') {
            return c - 'a' + 10;
        }
        if (c >= 'A' && c <= 'F') {
            return c - 'A' + 10;
        }
        fail(LoadProblemCode::wrongType, path);
    };
    if (text.size() != 7 || text[0] != '#') {
        fail(LoadProblemCode::wrongType, path);
    }
    const auto channel = [&](std::size_t at) {
        return static_cast<std::uint8_t>(digit(text[at]) * 16 + digit(text[at + 1]));
    };
    return {channel(1), channel(3), channel(5)};
}

geo::Point3d pointFrom(const Json& json, const std::string& path) {
    if (!json.is_array() || json.size() != 3 ||
        !std::all_of(json.begin(), json.end(), [](const Json& n) { return n.is_number(); })) {
        fail(LoadProblemCode::wrongType, path);
    }
    return {json[0].get<double>(), json[1].get<double>(), json[2].get<double>()};
}

std::vector<std::string> stringsFrom(ObjectReader& reader, const std::string& key) {
    std::vector<std::string> list;
    const Json& array = reader.array(key);
    for (std::size_t i = 0; i < array.size(); ++i) {
        if (!array[i].is_string()) {
            fail(LoadProblemCode::wrongType, childPath(reader.path(key), std::to_string(i)));
        }
        list.push_back(array[i].get<std::string>());
    }
    return list;
}

template <class IdT> std::vector<IdT> idsFrom(ObjectReader& reader, const std::string& key) {
    std::vector<IdT> ids;
    for (std::string& text : stringsFrom(reader, key)) {
        ids.push_back(IdT{std::move(text)});
    }
    return ids;
}

ElementLook lookFrom(const Json& json, const std::string& path, Problems& problems) {
    ObjectReader reader(json, path, problems);
    ElementLook look;
    look.kind = reader.word("kind", elementKindWords);
    look.shape = reader.word("shape", elementShapeWords);
    look.centroid = pointFrom(reader.field("centroid"), reader.path("centroid"));
    look.direction = pointFrom(reader.field("direction"), reader.path("direction"));
    look.hasDirection = reader.flag("hasDirection");
    look.directed = reader.flag("directed");
    look.size = reader.number("size");
    look.radius = reader.number("radius");
    look.neighbours = stringsFrom(reader, "neighbours");
    const Json& looks = reader.array("neighbourLooks");
    for (std::size_t i = 0; i < looks.size(); ++i) {
        const std::string at = childPath(reader.path("neighbourLooks"), std::to_string(i));
        ObjectReader n(looks[i], at, problems);
        NeighbourLook neighbour;
        neighbour.name = n.text("name");
        neighbour.shape = n.word("shape", elementShapeWords);
        neighbour.centroid = pointFrom(n.field("centroid"), n.path("centroid"));
        neighbour.direction = pointFrom(n.field("direction"), n.path("direction"));
        neighbour.hasDirection = n.flag("hasDirection");
        neighbour.directed = n.flag("directed");
        n.finish();
        look.neighbourLooks.push_back(std::move(neighbour));
    }
    reader.finish();
    return look;
}

Value valueFrom(const Json& json, const std::string& path, Problems& problems) {
    ObjectReader reader(json, path, problems);
    Value value;
    value.expression = reader.text("expression");
    value.kind = reader.word("kind", quantityWords);
    reader.finish();
    return value;
}

Feature featureFrom(const Json& json, const std::string& path, Problems& problems) {
    ObjectReader reader(json, path, problems);
    Feature feature;
    feature.id = FeatureId{reader.text("id")};
    feature.number = reader.integer("number");
    feature.type = reader.text("type");
    feature.typeVersion = reader.integer("typeVersion");
    feature.label = reader.text("label");
    for (const auto& [name, value] : reader.object("params").items()) {
        feature.params[name] = valueFrom(value, childPath(reader.path("params"), name), problems);
    }
    for (const auto& [name, value] : reader.object("texts").items()) {
        if (!value.is_string()) {
            fail(LoadProblemCode::wrongType, childPath(reader.path("texts"), name));
        }
        feature.texts[name] = value.get<std::string>();
    }
    for (const auto& [name, list] : reader.object("refs").items()) {
        const std::string listPath = childPath(reader.path("refs"), name);
        if (!list.is_array()) {
            fail(LoadProblemCode::wrongType, listPath);
        }
        std::vector<PersistentRef>& refs = feature.refs[name];
        for (std::size_t i = 0; i < list.size(); ++i) {
            const std::string at = childPath(listPath, std::to_string(i));
            ObjectReader ref(list[i], at, problems);
            PersistentRef parsed;
            parsed.kind = ref.word("kind", elementKindWords);
            parsed.name = ref.text("name");
            parsed.look = lookFrom(ref.field("look"), ref.path("look"), problems);
            ref.finish();
            refs.push_back(std::move(parsed));
        }
    }
    feature.inputs = idsFrom<BodyId>(reader, "inputs");
    feature.outputs = idsFrom<BodyId>(reader, "outputs");
    feature.suppressed = reader.flag("suppressed");
    reader.finish();
    return feature;
}

Body bodyFrom(const Json& json, const std::string& path, Problems& problems) {
    ObjectReader reader(json, path, problems);
    Body body;
    body.id = BodyId{reader.text("id")};
    body.name = reader.text("name");
    body.color = colorFrom(reader.text("color"), reader.path("color"));
    body.visible = reader.flag("visible");
    body.hole = reader.flag("hole");
    body.kind = reader.word("kind", bodyKindWords);
    body.createdBy = FeatureId{reader.text("createdBy")};
    body.cutExceptions = idsFrom<BodyId>(reader, "cutExceptions");
    reader.finish();
    return body;
}

Parameter parameterFrom(const Json& json, const std::string& path, Problems& problems) {
    ObjectReader reader(json, path, problems);
    Parameter parameter;
    parameter.name = reader.text("name");
    parameter.expression = reader.text("expression");
    parameter.kind = reader.word("kind", quantityWords);
    parameter.comment = reader.text("comment");
    reader.finish();
    return parameter;
}

DocumentSettings settingsFrom(const Json& json, const std::string& path, Problems& problems) {
    ObjectReader reader(json, path, problems);
    DocumentSettings settings;
    settings.defaultUnitMillimetres = reader.number("defaultUnitMillimetres");
    const Json& palette = reader.array("palette");
    for (std::size_t i = 0; i < palette.size(); ++i) {
        const std::string at = childPath(reader.path("palette"), std::to_string(i));
        ObjectReader entry(palette[i], at, problems);
        PaletteEntry parsed;
        parsed.key = entry.text("key");
        parsed.color = colorFrom(entry.text("color"), entry.path("color"));
        parsed.custom = entry.flag("custom");
        entry.finish();
        settings.palette.push_back(std::move(parsed));
    }
    reader.finish();
    return settings;
}

/// The checks that make a document safe to work with: ids exist and are not used twice.
void checkConsistency(const DocumentParts& parts) {
    std::set<std::string> featureIds;
    for (std::size_t i = 0; i < parts.features.size(); ++i) {
        const std::string& id = parts.features[i].id.value;
        if (id.empty() || !featureIds.insert(id).second) {
            fail(LoadProblemCode::inconsistent, "/features/" + std::to_string(i) + "/id");
        }
    }
    std::set<std::string> bodyIds;
    for (std::size_t i = 0; i < parts.bodies.size(); ++i) {
        const std::string& id = parts.bodies[i].id.value;
        if (id.empty() || !bodyIds.insert(id).second) {
            fail(LoadProblemCode::inconsistent, "/bodies/" + std::to_string(i) + "/id");
        }
    }
    if (parts.rollback && *parts.rollback > parts.features.size()) {
        fail(LoadProblemCode::inconsistent, "/rollback");
    }
    if (parts.lastFeatureNumber < 0) {
        fail(LoadProblemCode::inconsistent, "/lastFeatureNumber");
    }
}

DocumentParts partsFrom(const Json& root, Problems& problems) {
    ObjectReader reader(root, "", problems);
    reader.integer("formatVersion"); // read (and checked) before; marks it as known
    DocumentParts parts;
    parts.title = reader.text("title");
    parts.settings = settingsFrom(reader.field("settings"), reader.path("settings"), problems);
    const Json& parameters = reader.array("parameters");
    for (std::size_t i = 0; i < parameters.size(); ++i) {
        parts.parameters.push_back(parameterFrom(
            parameters[i], childPath(reader.path("parameters"), std::to_string(i)), problems));
    }
    const Json& features = reader.array("features");
    for (std::size_t i = 0; i < features.size(); ++i) {
        parts.features.push_back(featureFrom(
            features[i], childPath(reader.path("features"), std::to_string(i)), problems));
    }
    const Json& bodies = reader.array("bodies");
    for (std::size_t i = 0; i < bodies.size(); ++i) {
        parts.bodies.push_back(
            bodyFrom(bodies[i], childPath(reader.path("bodies"), std::to_string(i)), problems));
    }
    parts.lastFeatureNumber = reader.integer("lastFeatureNumber");
    const Json& rollback = reader.field("rollback");
    if (rollback.is_number_integer() && rollback.get<std::int64_t>() >= 0) {
        parts.rollback = static_cast<std::size_t>(rollback.get<std::int64_t>());
    } else if (!rollback.is_null()) {
        fail(LoadProblemCode::wrongType, reader.path("rollback"));
    }
    reader.finish();
    checkConsistency(parts);
    return parts;
}

LoadResult read(std::string_view text, const MigrationTable& migrations, int targetVersion) {
    LoadResult result;
    try {
        Json root = Json::parse(text.begin(), text.end(), nullptr, /*allow_exceptions=*/false);
        if (root.is_discarded()) {
            fail(LoadProblemCode::notJson, "");
        }
        if (!root.is_object()) {
            fail(LoadProblemCode::wrongType, "");
        }
        const auto versionField = root.find("formatVersion");
        if (versionField == root.end()) {
            fail(LoadProblemCode::missingField, "/formatVersion");
        }
        if (!versionField->is_number_integer()) {
            fail(LoadProblemCode::wrongType, "/formatVersion");
        }
        const std::int64_t version = versionField->get<std::int64_t>();
        if (version > targetVersion) {
            // A file from a newer program: read what we know and say so.
            result.problems.push_back({LoadProblemCode::newerFormat, "/formatVersion"});
        } else {
            for (std::int64_t v = version; v < targetVersion; ++v) {
                const auto step = migrations.find(static_cast<int>(v));
                if (step == migrations.end()) {
                    fail(LoadProblemCode::unsupportedFormat, "/formatVersion");
                }
                step->second(root);
                root["formatVersion"] = v + 1;
            }
        }
        result.document = Document::fromParts(partsFrom(root, result.problems));
    } catch (const Fatal& fatal) {
        result.document.reset();
        result.problems.push_back(fatal.problem);
    } catch (const Json::exception&) {
        // Whatever a migration or a reader did not foresee: a damaged file, not a crash.
        result.document.reset();
        result.problems.push_back({LoadProblemCode::wrongType, ""});
    }
    return result;
}

} // namespace

bool LoadResult::hasFatalProblem() const {
    return std::any_of(problems.begin(), problems.end(), [](const LoadProblem& problem) {
        return problem.code != LoadProblemCode::unknownField &&
               problem.code != LoadProblemCode::newerFormat;
    });
}

std::string toJson(const Document& document) {
    Json parameters = Json::array();
    for (const Parameter& parameter : document.parameters()) {
        parameters.push_back({{"name", parameter.name},
                              {"expression", parameter.expression},
                              {"kind", wordOf(quantityWords, parameter.kind)},
                              {"comment", parameter.comment}});
    }
    Json features = Json::array();
    for (const Feature& feature : document.features()) {
        features.push_back(featureJson(feature));
    }
    Json bodies = Json::array();
    for (const Body& body : document.bodies()) {
        bodies.push_back(bodyJson(body));
    }
    Json palette = Json::array();
    for (const PaletteEntry& entry : document.settings().palette) {
        palette.push_back(
            {{"key", entry.key}, {"color", hexOf(entry.color)}, {"custom", entry.custom}});
    }
    Json root = {{"formatVersion", currentFormatVersion},
                 {"title", document.title},
                 {"settings",
                  {{"defaultUnitMillimetres", document.settings().defaultUnitMillimetres},
                   {"palette", std::move(palette)}}},
                 {"parameters", std::move(parameters)},
                 {"features", std::move(features)},
                 {"bodies", std::move(bodies)},
                 {"lastFeatureNumber", document.lastFeatureNumber()},
                 {"rollback", document.rollback() ? Json(*document.rollback()) : Json(nullptr)}};
    return root.dump(2);
}

LoadResult fromJson(std::string_view text) {
    static const MigrationTable builtIn; // empty while the format is at version 1
    return read(text, builtIn, currentFormatVersion);
}

LoadResult fromJsonWithMigrations(std::string_view text, const MigrationTable& migrations,
                                  int targetVersion) {
    return read(text, migrations, targetVersion);
}

} // namespace fission::core
