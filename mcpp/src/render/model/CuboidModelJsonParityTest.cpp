// Parity gate for cuboidjson::parseModel (block-model JSON deserialization) vs the real
// CuboidModel.GSON (CuboidModel/CuboidModelElement/CuboidFace deserializers). Decodes each base64
// JSON, parses it, bakes the geometry via the certified elembake::bakeCuboidGeometry, and checks
// every QuadCollection bucket (which verifies from/to/uv/rotation/cullface parsing) PLUS the parsed
// metadata (ao/guiLight/parent, per-element shade/light_emission, per-face tintindex) and the
// resolved texture-slot lookups.
//
//   cuboid_model_json_parity --cases mcpp/build/cuboid_model_json.tsv

#include "CuboidModelJson.h"
#include "TextureSlotsResolve.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cj = mc::render::model::cuboidjson;
namespace eb = mc::render::model::elembake;
namespace fb = mc::render::model::fb;
namespace j = mc::render::model::joml;
namespace ts = mc::render::model::texslots;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t fbits(float v) { return std::bit_cast<uint32_t>(v); }
uint64_t lb(const std::string& s) { return static_cast<uint64_t>(std::stoull(s, nullptr, 16)); }

std::string base64Decode(const std::string& in) {
    static const std::string T = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    int dec[256];
    for (int i = 0; i < 256; ++i) dec[i] = -1;
    for (int i = 0; i < 64; ++i) dec[(unsigned char)T[i]] = i;
    std::string out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || dec[c] == -1) continue;
        val = (val << 6) + dec[c];
        bits += 6;
        if (bits >= 0) { out.push_back(char((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

// Sprite resolver matching the GT's 3 sprites (256x256 atlas, 16x16, pad 0). Float-exact bounds.
eb::SpriteUV resolveSprite(const std::string& key) {
    if (key == "s1") return fb::SpriteUV{0.0625f, 0.125f, 0.0f, 0.0625f};
    if (key == "s2") return fb::SpriteUV{0.0f, 0.0625f, 0.0625f, 0.125f};
    return fb::SpriteUV{0.0f, 0.0625f, 0.0f, 0.0625f};  // s0 / default
}

struct ExpQuad { int dir; uint32_t pos[12]; uint64_t uv[4]; };
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: cuboid_model_json_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, checks = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << "\n";
    };

    // per-case state
    cj::ModelDoc doc;
    bool parsed = false;
    int em = 0, gl = 0, ha = 0, av = 0, hg = 0, hp = 0;
    std::string parentExp;
    std::vector<std::array<int, 8>> elemMeta;  // shade, le, t0..t5
    struct TexLk { std::string name; int present; std::string sprite; int ft; };
    std::vector<TexLk> texLks;
    std::vector<ExpQuad> expected[7];
    int curBucket = -2;
    eb::QuadBuckets computed;
    bool baked = false;

    auto bucketVec = [&](int key) -> const std::vector<fb::BakedQuadGeom>& {
        return key < 0 ? computed.unculled : computed.culled[key];
    };

    auto runCase = [&]() {
        ++cases;
        // metadata
        ++checks;
        if (doc.hasAo != (ha != 0) || (doc.hasAo && (doc.ao ? 1 : 0) != av)) fail("ao");
        if (doc.hasGuiLight != (hg != 0) || (doc.hasGuiLight && doc.guiLight != gl)) fail("guiLight");
        if (doc.hasParent != (hp != 0) || (doc.hasParent && doc.parent != parentExp)) fail("parent");
        // element metadata
        if ((int)doc.elements.size() != em) fail("elem count");
        for (size_t i = 0; i < elemMeta.size() && i < doc.elements.size(); ++i) {
            ++checks;
            if (doc.shade[i] != elemMeta[i][0]) fail("shade e" + std::to_string(i));
            if (doc.lightEmission[i] != elemMeta[i][1]) fail("light e" + std::to_string(i));
            for (int d = 0; d < 6; ++d)
                if (doc.faceTint[i][d] != elemMeta[i][2 + d]) fail("tint e" + std::to_string(i) + " d" + std::to_string(d));
        }
        // texture lookups
        auto resolved = [&]() {
            ts::Resolver r;
            r.addLast(doc.textureSlots);
            return r.resolve();
        }();
        for (const auto& tl : texLks) {
            ++checks;
            auto got = ts::Resolver::getMaterial(resolved, tl.name);
            if (got.has_value() != (tl.present != 0)) fail("tex present " + tl.name);
            else if (got.has_value() && (got->sprite != tl.sprite || (got->forceTranslucent ? 1 : 0) != tl.ft))
                fail("tex " + tl.name + " got=" + got->sprite);
        }
        // quads
        for (int slot = 0; slot < 7; ++slot) {
            int key = slot - 1;
            const auto& got = bucketVec(key);
            const auto& exp = expected[slot];
            if (got.size() != exp.size()) { fail("bucket " + std::to_string(key) + " size"); continue; }
            for (size_t i = 0; i < exp.size(); ++i) {
                ++checks;
                const auto& g = got[i];
                const auto& e = exp[i];
                bool ok = (g.direction == e.dir);
                for (int k = 0; k < 4 && ok; ++k) {
                    if (fbits(g.pos[k].x) != e.pos[k * 3] || fbits(g.pos[k].y) != e.pos[k * 3 + 1] ||
                        fbits(g.pos[k].z) != e.pos[k * 3 + 2]) ok = false;
                    if (g.packedUV[k] != e.uv[k]) ok = false;
                }
                if (!ok) fail("bucket " + std::to_string(key) + " quad " + std::to_string(i));
            }
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        if (t == "CASE") {
            parsed = false; baked = false; curBucket = -2;
            elemMeta.clear(); texLks.clear();
            for (int s = 0; s < 7; ++s) expected[s].clear();
        } else if (t == "JSON") {
            std::string jsonStr = base64Decode(p[1]);
            try {
                doc = cj::parseModel(jsonStr, resolveSprite);
                parsed = true;
            } catch (const std::exception& ex) {
                fail(std::string("parse threw: ") + ex.what());
                parsed = false;
            }
        } else if (t == "META") {
            ha = std::stoi(p[1]); av = std::stoi(p[2]); hg = std::stoi(p[3]); gl = std::stoi(p[4]);
            hp = std::stoi(p[5]); parentExp = (p[5] == "1") ? p[6] : "";
        } else if (t == "ELEMMETA") {
            em = std::stoi(p[1]);
        } else if (t == "EM") {
            std::array<int, 8> a{};
            a[0] = std::stoi(p[1]); a[1] = std::stoi(p[2]);
            for (int d = 0; d < 6; ++d) a[2 + d] = std::stoi(p[3 + d]);
            elemMeta.push_back(a);
        } else if (t == "TEX") {
            // count line; nothing to store
        } else if (t == "TL") {
            texLks.push_back(TexLk{p[1], std::stoi(p[2]), p[3], std::stoi(p[4])});
        } else if (t == "OUT") {
            if (parsed && !baked) {
                j::Matrix4f ident;
                computed = eb::bakeCuboidGeometry(doc.elements, false, ident, false, ident);
                baked = true;
            }
            curBucket = std::stoi(p[1]) + 1;
        } else if (t == "QUAD") {
            ExpQuad q;
            q.dir = std::stoi(p[1]);
            for (int k = 0; k < 12; ++k) q.pos[k] = static_cast<uint32_t>(std::stoul(p[2 + k], nullptr, 16));
            for (int k = 0; k < 4; ++k) q.uv[k] = lb(p[14 + k]);
            if (curBucket >= 0 && curBucket < 7) expected[curBucket].push_back(q);
        } else if (t == "END") {
            if (parsed) runCase();
            else { ++cases; }
        }
    }

    std::cout << "CuboidModelJson cases=" << cases << " checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
