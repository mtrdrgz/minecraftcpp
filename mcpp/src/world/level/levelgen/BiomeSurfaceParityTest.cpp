// Per-biome OVERWORLD surface-rule parity test, ISOLATED from base terrain.
//
// The expected TSV (tools/BiomeSurfaceParity.java) emits the biome-independent
// base terrain column (BASE rows) and, for each forced biome, the vanilla
// post-buildSurface column (SURF rows). This test loads the BASE column into a
// chunk (so terrain is byte-identical to vanilla — independent of any C++
// fillFromNoise/aquifer FP divergence) and runs the ported SurfaceSystem with the
// same forced biome. Any mismatch is therefore a surface-rule port bug, certified
// one biome at a time.

#include "NoiseBasedChunkGenerator.h"
#include "../block/Blocks.h"
#include "../block/BlockState.h"
#include "../chunk/LevelChunk.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {
int floorDiv(int x, int y) {
    int q = x / y, r = x % y;
    if (r != 0 && ((r < 0) != (y < 0))) --q;
    return q;
}
std::int64_t packChunk(int cx, int cz) {
    return (static_cast<std::int64_t>(static_cast<std::uint32_t>(cx)) << 32)
        | static_cast<std::uint32_t>(cz);
}
std::int64_t packCol(int x, int z) { return packChunk(x, z); }

std::string blockNameAt(const mc::LevelChunk& chunk, int x, int y, int z) {
    const std::uint32_t id = chunk.getBlock(x, y, z);
    const mc::BlockState* s = mc::getBlockState(id);
    if (!s || !s->block || s->block->name.empty()) return "minecraft:air";
    return "minecraft:" + s->block->name;
}
std::string stripNs(const std::string& id) {
    auto c = id.find(':');
    return c == std::string::npos ? id : id.substr(c + 1);
}
void installDefaultBlockStatesEnv() {
    if (std::getenv("MCPP_BLOCK_STATES")) return;
    for (const char* path : { "mcpp/src/assets/block_states.json", "src/assets/block_states.json" }) {
        std::ifstream probe(path, std::ios::binary);
        if (!probe) continue;
#if defined(_WIN32)
        _putenv_s("MCPP_BLOCK_STATES", path);
#else
        setenv("MCPP_BLOCK_STATES", path, 0);
#endif
        return;
    }
}

struct Col { std::map<int, std::string> base; };          // y -> base block name
struct SurfRow { int y; std::string block; };
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) { std::cerr << "usage: biome_surface_parity --cases <tsv>\n"; return 2; }

    installDefaultBlockStatesEnv();
    mc::initBlocks();

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    // base[(seed,col)] = column ; surf[(biome,seed,col)] = rows
    std::map<std::pair<long long, std::int64_t>, Col> base;
    std::map<std::tuple<std::string, long long, std::int64_t>, std::vector<SurfRow>> surf;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::istringstream ss(line);
        std::string tag;
        std::getline(ss, tag, '\t');
        if (tag == "BASE") {
            std::string seed, x, z, y, blk;
            std::getline(ss, seed, '\t'); std::getline(ss, x, '\t'); std::getline(ss, z, '\t');
            std::getline(ss, y, '\t'); std::getline(ss, blk, '\t');
            base[{std::stoll(seed), packCol(std::stoi(x), std::stoi(z))}].base[std::stoi(y)] = blk;
        } else if (tag == "SURF") {
            std::string biome, seed, x, z, y, blk;
            std::getline(ss, biome, '\t'); std::getline(ss, seed, '\t'); std::getline(ss, x, '\t');
            std::getline(ss, z, '\t'); std::getline(ss, y, '\t'); std::getline(ss, blk, '\t');
            surf[{biome, std::stoll(seed), packCol(std::stoi(x), std::stoi(z))}]
                .push_back({std::stoi(y), blk});
        }
    }
    if (base.empty() || surf.empty()) { std::cerr << "empty parity data\n"; return 2; }

    const std::uint32_t airId = mc::getDefaultBlockStateId("air", 0);

    std::unordered_map<long long, std::unique_ptr<mc::levelgen::NoiseBasedChunkGenerator>> generators;

    int total = 0, mismatches = 0, shown = 0;
    std::set<std::string> biomesSeen, biomesBad;

    // Iterate per (biome, seed): build chunks once, fill all of that seed's test
    // columns from BASE, run buildSurface(forced biome), then compare.
    std::set<std::tuple<std::string, long long>> biomeSeeds;
    for (const auto& kv : surf) biomeSeeds.insert({std::get<0>(kv.first), std::get<1>(kv.first)});

    for (const auto& [biome, seed] : biomeSeeds) {
        biomesSeen.insert(biome);
        if (!generators.count(seed))
            generators[seed] = std::make_unique<mc::levelgen::NoiseBasedChunkGenerator>(
                static_cast<std::uint64_t>(seed));
        auto& gen = *generators[seed];

        // Build/fill chunks for every column of this seed from BASE.
        std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>> chunks;
        for (const auto& [k, col] : base) {
            if (k.first != seed) continue;
            const std::int64_t colKey = k.second;
            const int x = static_cast<int>(colKey >> 32);
            const int z = static_cast<int>(static_cast<std::int32_t>(colKey & 0xffffffff));
            const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
            const std::int64_t ck = packChunk(cx, cz);
            if (!chunks.count(ck))
                chunks[ck] = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ cx, cz });
            auto& chunk = *chunks[ck];
            for (const auto& [y, name] : col.base) {
                const std::uint32_t id = mc::getDefaultBlockStateId(stripNs(name), airId);
                if (id != airId) chunk.setBlock(x, y, z, id);
            }
        }
        const std::string forced = biome;
        for (auto& [ck, chunk] : chunks)
            gen.buildSurface(*chunk, [forced](int, int, int) { return forced; });

        // Compare each column.
        for (const auto& [k, col] : base) {
            if (k.first != seed) continue;
            const std::int64_t colKey = k.second;
            auto sit = surf.find({biome, seed, colKey});
            if (sit == surf.end()) continue;
            const int x = static_cast<int>(colKey >> 32);
            const int z = static_cast<int>(static_cast<std::int32_t>(colKey & 0xffffffff));
            const int cx = floorDiv(x, 16), cz = floorDiv(z, 16);
            auto& chunk = *chunks[packChunk(cx, cz)];
            for (const SurfRow& r : sit->second) {
                ++total;
                const std::string got = blockNameAt(chunk, x, r.y, z);
                if (got != r.block) {
                    ++mismatches;
                    biomesBad.insert(biome);
                    if (shown++ < 100000)
                        std::cerr << "MISMATCH biome=" << biome << " seed=" << seed << " pos=("
                                  << x << ',' << r.y << ',' << z << ") got=" << got
                                  << " expected=" << r.block << '\n';
                }
            }
        }
    }

    std::cout << "BiomeSurface biomes=" << biomesSeen.size() << " cases=" << total
              << " mismatches=" << mismatches << " biomes_with_mismatch=" << biomesBad.size() << "\n";
    if (!biomesBad.empty()) {
        std::cerr << "biomes with mismatches:";
        for (const auto& b : biomesBad) std::cerr << ' ' << b;
        std::cerr << '\n';
    }
    return mismatches == 0 ? 0 : 1;
}
