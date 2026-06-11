#pragma once

// 1:1 port of the block-model JSON deserializers (CuboidModel.GSON, CuboidModel.java:31-95):
//   CuboidModel.Deserializer        — elements/parent/textures/ambientocclusion/gui_light/display
//   CuboidModelElement.Deserializer — from/to/rotation/faces/shade/light_emission
//   CuboidFace.Deserializer         — cullface/tintindex/texture/uv/rotation
// Parses a real .json into the certified bake structures (ModelElementBake.h) + TextureSlots.Data
// (TextureSlotsResolve.h), composing the certified CuboidRotation transforms. Certified end-to-end
// by cuboid_model_json_parity (parse -> certified bake -> compare quads + parsed metadata).
//
// FLOAT-PARSE 1:1 NOTE: vanilla reads floats via GsonHelper.convertToFloat -> JsonElement.getAsFloat
// -> Float.parseFloat(token) = SINGLE-rounding string->float. nlohmann parses numbers to double, and
// (float)double DOUBLE-rounds (1-ULP divergence on non-float-exact decimals). Every value a vanilla
// model uses (16-grid coords, 0/22.5/45 angles, 1/16th uvs) is float-exact, so (float)double matches
// Float.parseFloat for all real inputs; for full generality use strtof on the raw token string.

#include "CuboidRotation.h"
#include "ModelElementBake.h"
#include "TextureSlotsResolve.h"

#include "../../world/level/levelgen/Mth.h"

#include <nlohmann/json.hpp>

#include <array>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace mc::render::model {

namespace cuboidjson {

using json = nlohmann::json;
namespace eb = mc::render::model::elembake;
namespace cr = mc::render::model::cuboid;
namespace ts = mc::render::model::texslots;
using joml::Matrix4f;
using joml::Vector3f;

// ── GsonHelper-equivalent strict accessors ───────────────────────────────────
inline float convertToFloat(const json& e, const char* name) {
    if (!e.is_number()) throw std::runtime_error(std::string("Expected ") + name + " to be a Float");
    return static_cast<float>(e.get<double>());  // see FLOAT-PARSE note
}
inline int convertToInt(const json& e, const char* name) {
    if (!e.is_number()) throw std::runtime_error(std::string("Expected ") + name + " to be an Int");
    if (e.is_number_integer() || e.is_number_unsigned()) return static_cast<int>(e.get<long long>());
    return static_cast<int>(e.get<double>());  // Number.intValue(): truncate toward zero
}
inline float getAsFloat(const json& o, const char* key, float def) {
    return o.contains(key) ? convertToFloat(o.at(key), key) : def;
}
inline float getAsFloatReq(const json& o, const char* key) {
    if (!o.contains(key)) throw std::runtime_error(std::string("Missing ") + key);
    return convertToFloat(o.at(key), key);
}
inline int getAsInt(const json& o, const char* key, int def) {
    return o.contains(key) ? convertToInt(o.at(key), key) : def;
}
inline std::string getAsString(const json& o, const char* key, const std::string& def) {
    if (!o.contains(key)) return def;
    const json& e = o.at(key);
    if (!e.is_string()) throw std::runtime_error(std::string("Expected ") + key + " to be a String");
    return e.get<std::string>();
}
inline std::string getAsStringReq(const json& o, const char* key) {
    if (!o.contains(key)) throw std::runtime_error(std::string("Missing ") + key);
    const json& e = o.at(key);
    if (!e.is_string()) throw std::runtime_error(std::string("Expected ") + key + " to be a String");
    return e.get<std::string>();
}
inline bool isBooleanValue(const json& o, const char* key) {
    return o.contains(key) && o.at(key).is_boolean();
}
inline bool getAsBool(const json& o, const char* key, bool def) {
    if (!o.contains(key)) return def;
    const json& e = o.at(key);
    if (e.is_boolean()) return e.get<bool>();
    if (e.is_string()) return e.get<std::string>() == "true";  // Boolean.parseBoolean
    return false;
}
inline const json& getAsObject(const json& o, const char* key) {
    if (!o.contains(key) || !o.at(key).is_object()) throw std::runtime_error(std::string("Expected ") + key + " object");
    return o.at(key);
}
inline const json& getAsArray(const json& o, const char* key) {
    if (!o.contains(key) || !o.at(key).is_array()) throw std::runtime_error(std::string("Expected ") + key + " array");
    return o.at(key);
}

// ── enum name maps (Direction.byName / Axis.byName / Quadrant.parseJson) ──────
inline int directionByName(const std::string& n) {
    if (n == "down") return 0;
    if (n == "up") return 1;
    if (n == "north") return 2;
    if (n == "south") return 3;
    if (n == "west") return 4;
    if (n == "east") return 5;
    return -1;  // null
}
inline int axisByName(const std::string& n) {
    if (n == "x") return 0;
    if (n == "y") return 1;
    if (n == "z") return 2;
    return -1;
}
inline std::string toLowerAscii(std::string s) {
    for (char& c : s)
        if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}
inline int quadrantParseJson(int degrees) {
    int m = ((degrees % 360) + 360) % 360;  // Mth.positiveModulo
    switch (m) {
        case 0: return 0;    // R0  shift 0
        case 90: return 1;   // R90 shift 1
        case 180: return 2;  // R180
        case 270: return 3;  // R270
        default: throw std::runtime_error("Invalid rotation " + std::to_string(degrees));
    }
}

// Identifier.parse(s).toString(): default namespace "minecraft" when no ':' present.
inline std::string normalizeIdentifier(const std::string& s);

inline Vector3f getVector3f(const json& o, const char* key) {
    const json& a = getAsArray(o, key);
    if (a.size() != 3) throw std::runtime_error(std::string("Expected 3 ") + key + " values");
    return Vector3f{convertToFloat(a[0], key), convertToFloat(a[1], key), convertToFloat(a[2], key)};
}
inline Vector3f getPosition(const json& o, const char* key) {
    Vector3f v = getVector3f(o, key);
    if (v.x < -16.0F || v.y < -16.0F || v.z < -16.0F || v.x > 32.0F || v.y > 32.0F || v.z > 32.0F)
        throw std::runtime_error(std::string("'") + key + "' exceeds allowed boundaries");
    return v;
}

// Sprite resolver: a face texture key/ref -> its resolved atlas SpriteUV (the MaterialBaker role).
using SpriteResolver = std::function<eb::SpriteUV(const std::string&)>;

// Parsed document: bake-ready elements + the metadata not carried in the geometry.
struct ModelDoc {
    bool hasGeometry = false;
    std::vector<eb::CuboidElement> elements;
    std::vector<int> shade;                 // per element (0/1)
    std::vector<int> lightEmission;         // per element
    std::vector<std::array<int, 6>> faceTint;  // per element, per Direction (-1 if face absent)
    ts::Data textureSlots;
    bool hasAo = false;
    bool ao = false;
    bool hasGuiLight = false;
    int guiLight = 0;  // 0=FRONT, 1=SIDE
    bool hasParent = false;
    std::string parent;
};

// CuboidFace.Deserializer -> fills an ElementFace (geometry) and returns its tintIndex (metadata).
inline int parseFace(const json& o, int facing, const SpriteResolver& resolve, eb::ElementFace& out) {
    out.present = true;
    out.cullForDirection = directionByName(getAsString(o, "cullface", ""));
    int tint = getAsInt(o, "tintindex", -1);
    out.tintIndex = tint;
    std::string texture = getAsStringReq(o, "texture");
    out.sprite = resolve(texture);
    if (o.contains("uv")) {
        const json& a = getAsArray(o, "uv");
        if (a.size() != 4) throw std::runtime_error("Expected 4 uv values, found: " + std::to_string(a.size()));
        out.hasUv = true;
        out.uv = eb::UVs{convertToFloat(a[0], "minU"), convertToFloat(a[1], "minV"),
                         convertToFloat(a[2], "maxU"), convertToFloat(a[3], "maxV")};
    } else {
        out.hasUv = false;
    }
    out.uvRotation = quadrantParseJson(getAsInt(o, "rotation", 0));
    (void)facing;
    return tint;
}

// CuboidModelElement.Deserializer.
inline void parseElement(const json& o, const SpriteResolver& resolve, ModelDoc& doc) {
    eb::CuboidElement el;
    el.from = getPosition(o, "from");
    el.to = getPosition(o, "to");

    // rotation
    if (o.contains("rotation")) {
        const json& r = getAsObject(o, "rotation");
        Vector3f origin = getVector3f(r, "origin");
        origin.mul(Vector3f{0.0625F, 0.0625F, 0.0625F});
        Matrix4f base;
        if (!r.contains("axis") && !r.contains("angle")) {
            if (!r.contains("x") && !r.contains("y") && !r.contains("z"))
                throw std::runtime_error("Missing rotation value");
            float x = getAsFloat(r, "x", 0.0F), y = getAsFloat(r, "y", 0.0F), z = getAsFloat(r, "z", 0.0F);
            base = cr::eulerTransformation(x, y, z);
        } else {
            int axis = axisByName(toLowerAscii(getAsStringReq(r, "axis")));
            if (axis < 0) throw std::runtime_error("Invalid rotation axis");
            float angle = getAsFloatReq(r, "angle");
            base = cr::singleAxisTransformation(axis, angle);
        }
        bool rescale = getAsBool(r, "rescale", false);
        el.hasElement = true;
        el.elementOrigin = origin;
        el.elementTransform = cr::computeTransform(base, rescale);
    }

    // faces (EnumMap; >= 1)
    std::array<int, 6> tints = {-1, -1, -1, -1, -1, -1};
    const json& faces = getAsObject(o, "faces");
    int faceCount = 0;
    for (auto it = faces.begin(); it != faces.end(); ++it) {
        int dir = directionByName(it.key());
        if (dir < 0) throw std::runtime_error("Unknown facing: " + it.key());
        tints[dir] = parseFace(it.value(), dir, resolve, el.faces[dir]);
        ++faceCount;
    }
    if (faceCount == 0) throw std::runtime_error("Expected between 1 and 6 unique faces, got 0");

    // shade / light_emission
    if (o.contains("shade") && !isBooleanValue(o, "shade"))
        throw std::runtime_error("Expected 'shade' to be a Boolean");
    el.shade = getAsBool(o, "shade", true);
    int lightEmission = 0;
    if (o.contains("light_emission")) {
        bool isNumber = o.at("light_emission").is_number();
        if (isNumber) lightEmission = getAsInt(o, "light_emission", 0);
        if (!isNumber || lightEmission < 0 || lightEmission > 15)
            throw std::runtime_error("Expected 'light_emission' to be an Integer between 0 and 15");
    }
    el.lightEmission = lightEmission;

    doc.elements.push_back(el);
    doc.shade.push_back(el.shade ? 1 : 0);
    doc.lightEmission.push_back(lightEmission);
    doc.faceTint.push_back(tints);
}

// TextureSlots.parseTextureMap (+ parseEntry).
inline ts::Data parseTextureMap(const json& texturesObject) {
    ts::Data data;
    for (auto it = texturesObject.begin(); it != texturesObject.end(); ++it) {
        const std::string& slot = it.key();
        const json& value = it.value();
        if (value.is_string() && !value.get<std::string>().empty() && value.get<std::string>()[0] == '#') {
            data.addReference(slot, value.get<std::string>().substr(1));
        } else if (value.is_string()) {
            // Material.CODEC SIMPLE: a plain Identifier string -> Material(id, forceTranslucent=false).
            data.addTexture(slot, ts::Material{normalizeIdentifier(value.get<std::string>()), false});
        } else if (value.is_object()) {
            // Material.CODEC FULL: { sprite, force_translucent?=false }.
            std::string sprite = getAsStringReq(value, "sprite");
            bool ft = getAsBool(value, "force_translucent", false);
            data.addTexture(slot, ts::Material{normalizeIdentifier(sprite), ft});
        } else {
            throw std::runtime_error("Invalid texture value for slot " + slot);
        }
    }
    return data;
}

// Identifier.parse(s).toString(): default namespace "minecraft" when no ':' present.
inline std::string normalizeIdentifier(const std::string& s) {
    if (s.find(':') == std::string::npos) return "minecraft:" + s;
    return s;
}

// ── ItemTransform / ItemTransforms (the model "display" block) ───────────────
// ItemTransform.Deserializer (ItemTransform.java:46-82): rotation default [0,0,0]; translation
// default [0,0,0] -> *0.0625 -> clamp [-5,5]; scale default [1,1,1] -> clamp [-4,4].
struct ItemTransform {
    float rot[3], trans[3], scale[3];
};
inline const ItemTransform IT_NO_TRANSFORM{{0, 0, 0}, {0, 0, 0}, {1, 1, 1}};

inline void getVec3(const json& o, const char* key, const float def[3], float out[3]) {
    if (!o.contains(key)) { out[0] = def[0]; out[1] = def[1]; out[2] = def[2]; return; }
    const json& a = getAsArray(o, key);
    if (a.size() != 3) throw std::runtime_error(std::string("Expected 3 ") + key + " values");
    out[0] = convertToFloat(a[0], key);
    out[1] = convertToFloat(a[1], key);
    out[2] = convertToFloat(a[2], key);
}
inline ItemTransform parseItemTransform(const json& o) {
    static const float dr[3] = {0, 0, 0}, dt[3] = {0, 0, 0}, ds[3] = {1, 1, 1};
    ItemTransform t;
    getVec3(o, "rotation", dr, t.rot);
    getVec3(o, "translation", dt, t.trans);
    for (int i = 0; i < 3; ++i) t.trans[i] = mc::levelgen::mth::clamp(t.trans[i] * 0.0625F, -5.0F, 5.0F);
    getVec3(o, "scale", ds, t.scale);
    for (int i = 0; i < 3; ++i) t.scale[i] = mc::levelgen::mth::clamp(t.scale[i], -4.0F, 4.0F);
    return t;
}

// ItemTransforms.Deserializer (ItemTransforms.java:49-78): each ItemDisplayContext read by its
// serialized name, default NO_TRANSFORM. Keyed by the 9 contexts the deserializer handles.
struct ItemTransforms {
    ItemTransform byName[9];  // see ITEM_TRANSFORM_NAMES order
};
inline const char* const ITEM_TRANSFORM_NAMES[9] = {
    "thirdperson_righthand", "thirdperson_lefthand", "firstperson_righthand", "firstperson_lefthand",
    "head", "gui", "ground", "fixed", "on_shelf"};
inline ItemTransforms parseItemTransforms(const json& display) {
    ItemTransforms t;
    for (int i = 0; i < 9; ++i) {
        const char* name = ITEM_TRANSFORM_NAMES[i];
        t.byName[i] = display.contains(name) ? parseItemTransform(display.at(name)) : IT_NO_TRANSFORM;
    }
    // ItemTransforms.Deserializer:54-62 — an ABSENT *_lefthand inherits the matching *_righthand
    // (the `== NO_TRANSFORM` singleton check = "key absent", NOT value-equality). Names order:
    // 0=thirdperson_righthand 1=thirdperson_lefthand 2=firstperson_righthand 3=firstperson_lefthand.
    if (!display.contains("thirdperson_lefthand")) t.byName[1] = t.byName[0];
    if (!display.contains("firstperson_lefthand")) t.byName[3] = t.byName[2];
    return t;
}

// CuboidModel.Deserializer.
inline ModelDoc parseModel(const std::string& jsonStr, const SpriteResolver& resolve) {
    json root = json::parse(jsonStr);
    ModelDoc doc;
    if (root.contains("elements")) {
        doc.hasGeometry = true;
        const json& arr = getAsArray(root, "elements");
        for (const json& e : arr) parseElement(e, resolve, doc);
    }
    std::string parent = getAsString(root, "parent", "");
    if (!parent.empty()) { doc.hasParent = true; doc.parent = normalizeIdentifier(parent); }
    if (root.contains("textures")) doc.textureSlots = parseTextureMap(getAsObject(root, "textures"));
    if (root.contains("ambientocclusion")) { doc.hasAo = true; doc.ao = getAsBool(root, "ambientocclusion", false); }
    if (root.contains("gui_light")) {
        doc.hasGuiLight = true;
        std::string g = getAsStringReq(root, "gui_light");
        if (g == "front") doc.guiLight = 0;
        else if (g == "side") doc.guiLight = 1;
        else throw std::runtime_error("Invalid gui light: " + g);
    }
    return doc;
}

}  // namespace cuboidjson

}  // namespace mc::render::model
