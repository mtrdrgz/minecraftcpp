// Byte-exact parity gate for the C++ 1:1 StructureTemplatePool + element type
// hierarchy + worldgen/template_pool JSON loader (StructureTemplatePool.h) vs the
// REAL 26.1.2 classes (StructureTemplatePoolParity.java).
//
// We load the SAME four pillager_outpost pools from the SAME template_pool JSON
// (26.1.2/data/minecraft/worldgen/template_pool/pillager_outpost/*.json), recover
// each referenced template's SIZE by parsing the base64 .nbt the Java tool shipped
// (the certified StructureTemplateLoader), then compare bit-for-bit:
//   - POOL:     fallback id, size(), getMaxSize()
//   - TEMPLATE: expanded `templates` order (location, projection, groundLevelDelta)
//   - SHUFFLE:  getShuffledTemplates(seed) order — element (location, projection) per
//               index, with WorldgenRandom(LegacyRandomSource(seed))
//   - BOX:      element.getBoundingBox(pos, rotation) min/max + ySpan
//   - BOX_EMPTY: EmptyPoolElement.getBoundingBox MUST throw ("filter me!")
//
// Prints: StructureTemplatePool checks=N mismatches=M  (exit nonzero iff M>0).
//
//   structure_template_pool_parity --cases mcpp/build/structure_template_pool.tsv

#include "StructureTemplatePool.h"
#include "../templatesystem/StructureTemplateLoader.h"
#include "../../../../../nbt/NbtIo.h"
#include "../../../../../nbt/Tag.h"
#include "../../RandomSource.h"

#include <nlohmann/json.hpp>

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace pools = mc::levelgen::structure::pools;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::Vec3i;
using mc::levelgen::structure::BlockPos;
using mc::levelgen::structure::BoundingBox;
using pools::StructureTemplatePool;
using pools::StructurePoolElement;
using pools::Projection;

namespace {

std::vector<std::uint8_t> base64Decode(const std::string& in) {
    static int8_t lut[256];
    static bool init = false;
    if (!init) {
        for (int i = 0; i < 256; ++i) lut[i] = -1;
        const char* a = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        for (int i = 0; i < 64; ++i) lut[(unsigned char)a[i]] = (int8_t)i;
        init = true;
    }
    std::vector<std::uint8_t> out;
    int val = 0, bits = -8;
    for (unsigned char c : in) {
        if (c == '=' || c == '\r' || c == '\n' || c == ' ' || c == '\t') continue;
        int8_t d = lut[c];
        if (d < 0) continue;
        val = (val << 6) | d;
        bits += 6;
        if (bits >= 0) { out.push_back((std::uint8_t)((val >> bits) & 0xFF)); bits -= 8; }
    }
    return out;
}

std::vector<std::string> split(const std::string& line, char sep) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == sep) { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

std::string readFile(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return std::string();
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

// Map pool-key -> JSON filename.
std::string poolJsonPath(const std::string& key) {
    // key e.g. "pillager_outpost/base_plates" -> the matching JSON file.
    auto pos = key.find('/');
    std::string leaf = pos == std::string::npos ? key : key.substr(pos + 1);
    return "26.1.2/data/minecraft/worldgen/template_pool/pillager_outpost/" + leaf + ".json";
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/structure_template_pool.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) {
        std::cerr << "cannot open cases file: " << casesPath << "\n";
        std::cout << "StructureTemplatePool checks=0 mismatches=1\n";
        return 1;
    }

    long long checks = 0, mismatches = 0;
    int reported = 0;
    auto fail = [&](const std::string& msg) {
        ++mismatches;
        if (reported < 60) { std::cerr << "MISMATCH: " << msg << "\n"; ++reported; }
    };

    // location -> Vec3i size (from the base64 .nbt the Java tool shipped).
    std::map<std::string, Vec3i> sizeOfMap;

    // Pool key order (POOL_KEYS as emitted by Java).
    const std::vector<std::string> poolKeys = {
        "pillager_outpost/base_plates",
        "pillager_outpost/towers",
        "pillager_outpost/feature_plates",
        "pillager_outpost/features",
    };

    // ── First pass: NBT rows -> sizes. ───────────────────────────────────────
    // Collect all rows for the second pass too.
    std::vector<std::vector<std::string>> rows;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> cols = split(line, '\t');
        if (cols.empty()) continue;
        if (cols[0] == "NBT" && cols.size() >= 6) {
            const std::string& id = cols[1];
            const std::string& b64 = cols[2];
            int sx = std::stoi(cols[3]), sy = std::stoi(cols[4]), sz = std::stoi(cols[5]);
            std::vector<std::uint8_t> bytes = base64Decode(b64);
            ++checks;
            try {
                auto root = mc::nbt::NbtReader::readGzip(bytes);
                if (!root.has_value()) {
                    fail("readGzip returned nullopt for " + id);
                } else {
                    auto t = mc::levelgen::structure::templatesystem::loadStructureTemplate(*root);
                    Vec3i sz3{t.size.x, t.size.y, t.size.z};
                    sizeOfMap[id] = sz3;
                    // Verify our parsed size matches the Java-reported size (sanity gate
                    // on the certified loader against the pool-level oracle).
                    if (sz3.x != sx || sz3.y != sy || sz3.z != sz) {
                        fail("NBT size mismatch for " + id + ": got (" +
                             std::to_string(sz3.x) + "," + std::to_string(sz3.y) + "," +
                             std::to_string(sz3.z) + ") expected (" + cols[3] + "," +
                             cols[4] + "," + cols[5] + ")");
                    }
                }
            } catch (const std::exception& e) {
                fail(std::string("NBT load threw for ") + id + ": " + e.what());
            }
        }
        rows.push_back(std::move(cols));
    }

    // The SizeResolver the elements query — strict (throws on unknown so a missing
    // NBT row is a hard failure, never a silent zero size).
    pools::SizeResolver sizeOf = [&](const std::string& loc) -> Vec3i {
        auto it = sizeOfMap.find(loc);
        if (it == sizeOfMap.end())
            throw std::runtime_error("no size for template location: " + loc);
        return it->second;
    };

    // ── Load the four pools from the SAME JSON the game ships. ────────────────
    std::map<std::string, StructureTemplatePool> loadedPools;
    for (const std::string& key : poolKeys) {
        std::string js = readFile(poolJsonPath(key));
        if (js.empty()) {
            fail("cannot read pool JSON for " + key + " (" + poolJsonPath(key) + ")");
            continue;
        }
        try {
            nlohmann::json j = nlohmann::json::parse(js);
            loadedPools[key] = pools::loadPool(j);
        } catch (const std::exception& e) {
            fail(std::string("loadPool threw for ") + key + ": " + e.what());
        }
    }

    // ── Second pass: compare POOL / TEMPLATE / SHUFFLE / BOX / BOX_EMPTY rows. ─
    // For SHUFFLE we need to recompute getShuffledTemplates(seed) per (pool, seed)
    // once and index into it. Cache them keyed by (poolKey, seed).
    auto elementAt = [&](const StructureTemplatePool& pool, int idx) -> const StructurePoolElement& {
        return pool.templates[static_cast<std::size_t>(idx)];
    };

    // Precompute shuffle orders per (pool, seed) lazily.
    std::map<std::string, std::map<long long, std::vector<int>>> shuffleCache;
    auto shuffleOrder = [&](const std::string& key, long long seed) -> const std::vector<int>& {
        auto& bySeed = shuffleCache[key];
        auto it = bySeed.find(seed);
        if (it != bySeed.end()) return it->second;
        const StructureTemplatePool& pool = loadedPools[key];
        // getShuffledTemplates uses the structure WorldgenRandom over LegacyRandomSource.
        auto random = std::make_shared<mc::levelgen::WorldgenRandom>(
            std::make_shared<mc::levelgen::LegacyRandomSource>(seed));
        std::vector<int> order = pool.getShuffledTemplateIndices(*random);
        return bySeed.emplace(seed, std::move(order)).first->second;
    };

    for (const auto& cols : rows) {
        const std::string& tag = cols[0];

        if (tag == "POOL") {
            // POOL key fallbackId size maxSize
            if (cols.size() < 5) { fail("POOL row too short"); continue; }
            const std::string& key = cols[1];
            auto pit = loadedPools.find(key);
            if (pit == loadedPools.end()) { fail("POOL " + key + " not loaded"); continue; }
            const StructureTemplatePool& pool = pit->second;
            ++checks;
            if (pool.getFallbackName() != cols[2])
                fail("fallback mismatch for " + key + ": got " + pool.getFallbackName() +
                     " expected " + cols[2]);
            ++checks;
            if (pool.size() != std::stoi(cols[3]))
                fail("size mismatch for " + key + ": got " + std::to_string(pool.size()) +
                     " expected " + cols[3]);
            ++checks;
            try {
                int got = pool.getMaxSize(sizeOf);
                if (got != std::stoi(cols[4]))
                    fail("maxSize mismatch for " + key + ": got " + std::to_string(got) +
                         " expected " + cols[4]);
            } catch (const std::exception& e) {
                fail(std::string("getMaxSize threw for ") + key + ": " + e.what());
            }
        }
        else if (tag == "TEMPLATE") {
            // TEMPLATE key index location projection groundLevelDelta
            if (cols.size() < 6) { fail("TEMPLATE row too short"); continue; }
            const std::string& key = cols[1];
            auto pit = loadedPools.find(key);
            if (pit == loadedPools.end()) { fail("TEMPLATE " + key + " not loaded"); continue; }
            const StructureTemplatePool& pool = pit->second;
            int idx = std::stoi(cols[2]);
            ++checks;
            if (idx < 0 || idx >= pool.size()) {
                fail("TEMPLATE index OOB for " + key + ": " + cols[2]);
                continue;
            }
            const StructurePoolElement& e = elementAt(pool, idx);
            std::string loc = e.locationString();
            std::string proj = pools::projectionName(e.getProjection());
            int gld = e.getGroundLevelDelta();
            if (loc != cols[3] || proj != cols[4] || gld != std::stoi(cols[5])) {
                fail("TEMPLATE mismatch for " + key + " idx=" + cols[2] +
                     ": got{loc=" + loc + " proj=" + proj + " gld=" + std::to_string(gld) +
                     "} exp{loc=" + cols[3] + " proj=" + cols[4] + " gld=" + cols[5] + "}");
            }
        }
        else if (tag == "SHUFFLE") {
            // SHUFFLE key seed orderIndex location projection
            if (cols.size() < 6) { fail("SHUFFLE row too short"); continue; }
            const std::string& key = cols[1];
            auto pit = loadedPools.find(key);
            if (pit == loadedPools.end()) { fail("SHUFFLE " + key + " not loaded"); continue; }
            const StructureTemplatePool& pool = pit->second;
            long long seed = std::stoll(cols[2]);
            int orderIndex = std::stoi(cols[3]);
            const std::vector<int>& order = shuffleOrder(key, seed);
            ++checks;
            if (orderIndex < 0 || orderIndex >= (int)order.size()) {
                fail("SHUFFLE orderIndex OOB for " + key + " seed=" + cols[2] + ": " + cols[3]);
                continue;
            }
            const StructurePoolElement& e = elementAt(pool, order[static_cast<std::size_t>(orderIndex)]);
            std::string loc = e.locationString();
            std::string proj = pools::projectionName(e.getProjection());
            if (loc != cols[4] || proj != cols[5]) {
                fail("SHUFFLE mismatch for " + key + " seed=" + cols[2] + " oi=" + cols[3] +
                     ": got{loc=" + loc + " proj=" + proj + "} exp{loc=" + cols[4] +
                     " proj=" + cols[5] + "}");
            }
        }
        else if (tag == "BOX") {
            // BOX key index rotOrd posX posY posZ minX minY minZ maxX maxY maxZ ySpan
            if (cols.size() < 14) { fail("BOX row too short"); continue; }
            const std::string& key = cols[1];
            auto pit = loadedPools.find(key);
            if (pit == loadedPools.end()) { fail("BOX " + key + " not loaded"); continue; }
            const StructureTemplatePool& pool = pit->second;
            int idx = std::stoi(cols[2]);
            int rotOrd = std::stoi(cols[3]);
            BlockPos pos{std::stoi(cols[4]), std::stoi(cols[5]), std::stoi(cols[6])};
            Rotation rot = static_cast<Rotation>(rotOrd);
            ++checks;
            if (idx < 0 || idx >= pool.size()) { fail("BOX index OOB for " + key); continue; }
            const StructurePoolElement& e = elementAt(pool, idx);
            try {
                BoundingBox bb = e.getBoundingBox(sizeOf, pos, rot);
                bool ok =
                    bb.minX == std::stoi(cols[7])  && bb.minY == std::stoi(cols[8])  &&
                    bb.minZ == std::stoi(cols[9])  && bb.maxX == std::stoi(cols[10]) &&
                    bb.maxY == std::stoi(cols[11]) && bb.maxZ == std::stoi(cols[12]) &&
                    bb.getYSpan() == std::stoi(cols[13]);
                if (!ok) {
                    std::ostringstream os;
                    os << "BOX mismatch " << key << " idx=" << idx << " rot=" << rotOrd
                       << " pos=(" << pos.x << "," << pos.y << "," << pos.z << ")"
                       << " got{min=(" << bb.minX << "," << bb.minY << "," << bb.minZ
                       << ") max=(" << bb.maxX << "," << bb.maxY << "," << bb.maxZ
                       << ") ySpan=" << bb.getYSpan() << "} exp{min=(" << cols[7] << ","
                       << cols[8] << "," << cols[9] << ") max=(" << cols[10] << ","
                       << cols[11] << "," << cols[12] << ") ySpan=" << cols[13] << "}";
                    fail(os.str());
                }
            } catch (const std::exception& ex) {
                fail(std::string("BOX threw for ") + key + " idx=" + std::to_string(idx) +
                     ": " + ex.what());
            }
        }
        else if (tag == "BOX_EMPTY") {
            // BOX_EMPTY key index rotOrd posX posY posZ — getBoundingBox MUST throw.
            if (cols.size() < 7) { fail("BOX_EMPTY row too short"); continue; }
            const std::string& key = cols[1];
            auto pit = loadedPools.find(key);
            if (pit == loadedPools.end()) { fail("BOX_EMPTY " + key + " not loaded"); continue; }
            const StructureTemplatePool& pool = pit->second;
            int idx = std::stoi(cols[2]);
            int rotOrd = std::stoi(cols[3]);
            BlockPos pos{std::stoi(cols[4]), std::stoi(cols[5]), std::stoi(cols[6])};
            Rotation rot = static_cast<Rotation>(rotOrd);
            ++checks;
            if (idx < 0 || idx >= pool.size()) { fail("BOX_EMPTY index OOB for " + key); continue; }
            const StructurePoolElement& e = elementAt(pool, idx);
            bool threw = false;
            try { (void)e.getBoundingBox(sizeOf, pos, rot); }
            catch (const std::exception&) { threw = true; }
            if (!threw)
                fail("BOX_EMPTY did NOT throw for " + key + " idx=" + std::to_string(idx) +
                     " (EmptyPoolElement.getBoundingBox must throw 'filter me!')");
        }
    }

    std::cout << "StructureTemplatePool checks=" << checks
              << " mismatches=" << mismatches << "\n";
    return mismatches > 0 ? 1 : 0;
}
