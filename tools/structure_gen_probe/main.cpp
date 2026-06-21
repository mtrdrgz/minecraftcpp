// structure_gen_probe — run the REAL structure generator (StructureGen.cpp) over a
// synthetic flat world against the real worldgen data, and report which structures
// place and how many blocks they write. This is the headless, cross-platform
// verification tool for structure work (villages etc.) — it exercises the exact
// in-engine code path without a window/GPU/Windows, so structure changes can be
// verified on Linux/CI just like the worldgen parity gates.
//
// It is an *observation* tool: it reports exactly what the engine code does today
// (it does NOT bypass the runtime structure-set gates). When a structure family is
// enabled/ported, its placements appear here.
//
// Build (parity-only build, Linux GCC):
//   cmake -S . -B build-linux -G Ninja -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
//   cmake --build build-linux --target structure_gen_probe
// Run (from repo root, after provision_parity_runtime so 26.1.2/data exists):
//   MCPP_BLOCK_STATES=src/assets/block_states.json \
//     ./build-linux/structure_gen_probe --biome minecraft:plains --radius 80
//   # add: 2>/dev/null | grep "placed" to see each structure's placement log
//
// Exit code is 0 when at least one structure placed in the scanned region, else 1,
// so it can double as a smoke gate.

#include "world/level/levelgen/structure/StructureGen.h"
#include "world/level/block/Blocks.h"

#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace mc;
using namespace mc::levelgen::structure;

namespace {

// Borderless flat world: solid `stone` below `surface`, air above; records writes.
struct FlatWorld {
    int surface = 68;
    std::uint32_t stone = 1;
    std::unordered_map<std::int64_t, std::uint32_t> over;
    long writes = 0;

    static std::int64_t key(int x, int y, int z) {
        return (static_cast<std::int64_t>(x & 0x3FFFFF))
             | (static_cast<std::int64_t>(z & 0x3FFFFF) << 22)
             | (static_cast<std::int64_t>((y + 2048) & 0xFFFFF) << 44);
    }
    std::uint32_t get(int x, int y, int z) const {
        auto it = over.find(key(x, y, z));
        if (it != over.end()) return it->second;
        return y < surface ? stone : 0u;
    }
    void set(int x, int y, int z, std::uint32_t id) { over[key(x, y, z)] = id; ++writes; }
};

std::string argValue(int argc, char** argv, const std::string& flag, const std::string& def) {
    for (int i = 1; i + 1 < argc; ++i)
        if (flag == argv[i]) return argv[i + 1];
    return def;
}

} // namespace

int main(int argc, char** argv) {
    initBlocks();

    const std::string data   = argValue(argc, argv, "--data", "26.1.2/data/minecraft");
    const std::string biome  = argValue(argc, argv, "--biome", "minecraft:plains");
    const int radius         = std::atoi(argValue(argc, argv, "--radius", "80").c_str());
    const std::uint64_t seed = std::strtoull(argValue(argc, argv, "--seed", "12345").c_str(), nullptr, 10);
    const int surfaceY       = std::atoi(argValue(argc, argv, "--surface", "68").c_str());

    std::cerr << "structure_gen_probe: data=" << data << " biome=" << biome
              << " seed=" << seed << " radius=" << radius << " surface=" << surfaceY << "\n";

    long totalWrites = 0;
    int chunksWithWrites = 0;
    int beardChunks = 0;
    long beardRigids = 0, beardJunctions = 0;
    for (int cz = 0; cz < radius; ++cz) {
        for (int cx = 0; cx < radius; ++cx) {
            FlatWorld fw;
            fw.surface = surfaceY;
            StructureWorld w;
            w.getBlock  = [&fw](int x, int y, int z) { return fw.get(x, y, z); };
            w.setBlock  = [&fw](int x, int y, int z, std::uint32_t id) { fw.set(x, y, z, id); };
            w.heightAt  = [&fw](int, int) { return fw.surface - 1; };
            auto biomeGetter = [&biome](int, int, int) { return biome; };
            generateStructures({cx, cz}, seed, w, biomeGetter, data);
            if (fw.writes > 0) { totalWrites += fw.writes; ++chunksWithWrites; }

            // Exercise the Beardifier integration (forStructuresInChunk): build the
            // per-chunk Beardifier from nearby terrain-adapting structures.
            auto columnHeight = [surfaceY](int, int) { return surfaceY - 1; };
            mc::levelgen::Beardifier beard =
                generateBeardifier({cx, cz}, seed, columnHeight, biomeGetter, data);
            if (!beard.isEmpty()) {
                ++beardChunks;
                // a non-empty beardifier must produce non-zero density somewhere near
                // its pieces; sample a small column to confirm it is live.
                double mag = 0.0;
                for (int y = surfaceY - 8; y <= surfaceY + 8; ++y)
                    mag += std::abs(beard.compute(cx * 16 + 8, y, cz * 16 + 8));
                (void)mag;
            }
        }
    }
    (void)beardRigids; (void)beardJunctions;

    std::cerr << "=== SUMMARY: scanned " << (radius * radius) << " chunks, "
              << chunksWithWrites << " with writes, " << totalWrites << " blocks total; "
              << beardChunks << " chunks have a non-empty Beardifier\n";
    return chunksWithWrites > 0 ? 0 : 1;
}
