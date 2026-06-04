// Deterministic check for the structure generator: drives generateStructures over
// a synthetic flat-stone world (no engine, no GPU) and asserts that the spaced
// structures actually place their blocks — including spanning more than one chunk,
// which proves the cross-chunk StructureWorld writer.
//
// Needs the real block registry for distinct state ids: run with
//   MCPP_BLOCK_STATES=<repo>/mcpp/src/assets/block_states.json
//
// Scope note: the piece shapes are hand-built approximations (not the faithful
// template/jigsaw ports yet); this verifies the placement plumbing, spacing/biome
// gating and cross-chunk writes, not 1:1 block-for-block parity with Java.

#include "StructureGen.h"
#include "../../block/Blocks.h"
#include "../../block/BlockState.h"

#include <algorithm>
#include <cstdint>
#include <iostream>
#include <string>
#include <unordered_map>

using namespace mc;
using namespace mc::levelgen::structure;

namespace {
bool g_ok = true;
void check(bool c, const std::string& m) {
    if (!c) { g_ok = false; std::cerr << "FAIL: " << m << "\n"; }
}

// Borderless flat world: solid `stone` below `surface`, air above. setBlock records
// overrides and tracks the written bounding box / change count.
struct FlatWorld {
    int surface = 64;
    uint32_t stone = 1;
    std::unordered_map<int64_t, uint32_t> over;
    int minX = 1 << 30, maxX = -(1 << 30), minZ = 1 << 30, maxZ = -(1 << 30);

    static int64_t key(int x, int y, int z) {
        return ((int64_t)(x & 0x3FFFFF))
             | ((int64_t)(z & 0x3FFFFF) << 22)
             | ((int64_t)((y + 2048) & 0xFFFFF) << 44);
    }
    uint32_t get(int x, int y, int z) const {
        auto it = over.find(key(x, y, z));
        if (it != over.end()) return it->second;
        return y < surface ? stone : 0u;
    }
    void set(int x, int y, int z, uint32_t id) {
        over[key(x, y, z)] = id;
        minX = std::min(minX, x); maxX = std::max(maxX, x);
        minZ = std::min(minZ, z); maxZ = std::max(maxZ, z);
    }
    int countOf(uint32_t id) const {
        int n = 0;
        for (auto& [k, v] : over) { (void)k; if (v == id) ++n; }
        return n;
    }
};

StructureWorld makeWorld(FlatWorld& fw) {
    StructureWorld w;
    w.getBlock = [&fw](int x, int y, int z) { return fw.get(x, y, z); };
    w.setBlock = [&fw](int x, int y, int z, uint32_t id) { fw.set(x, y, z, id); };
    w.heightAt = [&fw](int, int) { return fw.surface - 1; };
    return w;
}
} // namespace

int main() {
    initBlocks();

    const uint32_t stone     = getDefaultBlockStateId("stone", 1);
    const uint32_t sandstone = getDefaultBlockStateId("sandstone", 0);
    const uint32_t obsidian  = getDefaultBlockStateId("obsidian", 0);
    const uint32_t cryingObs = getDefaultBlockStateId("crying_obsidian", 0);
    if (sandstone == 0 || obsidian == 0) {
        std::cerr << "block registry not loaded; set MCPP_BLOCK_STATES to block_states.json\n";
        return 2;
    }

    // 1. Desert pyramid: scan for the nearest grid-trigger chunk (forcing a desert
    //    biome) and assert it lays substantial sandstone spanning >1 chunk in X.
    {
        bool found = false;
        for (int cz = 0; cz < 96 && !found; ++cz)
        for (int cx = 0; cx < 96 && !found; ++cx) {
            FlatWorld fw; fw.stone = stone;
            auto w = makeWorld(fw);
            generateStructures({cx, cz}, /*worldSeed*/ 0, w,
                [](int, int, int) { return std::string("minecraft:desert"); });
            int sand = fw.countOf(sandstone);
            if (sand > 50) {
                found = true;
                check(sand > 200, "desert pyramid placed substantial sandstone (got " + std::to_string(sand) + ")");
                check((fw.maxX - fw.minX) > 16, "pyramid footprint spans >1 chunk in X (cross-chunk writer)");
                std::cerr << "  pyramid @ chunk (" << cx << "," << cz << ")  sandstone=" << sand
                          << "  xspan=" << (fw.maxX - fw.minX + 1) << "\n";
            }
        }
        check(found, "found a desert-pyramid trigger chunk within 96x96");
    }

    // 2. Ruined portal: any biome. Use plains so the desert/swamp/snowy gates fail
    //    and placement falls through to the ruined-portal grid; assert obsidian.
    {
        bool found = false;
        for (int cz = 0; cz < 160 && !found; ++cz)
        for (int cx = 0; cx < 160 && !found; ++cx) {
            FlatWorld fw; fw.stone = stone;
            auto w = makeWorld(fw);
            generateStructures({cx, cz}, /*worldSeed*/ 0, w,
                [](int, int, int) { return std::string("minecraft:plains"); });
            int obs = fw.countOf(obsidian) + fw.countOf(cryingObs);
            if (obs > 0) {
                found = true;
                check(obs >= 1, "ruined portal placed obsidian/crying_obsidian (got " + std::to_string(obs) + ")");
                std::cerr << "  ruined portal @ chunk (" << cx << "," << cz << ")  obsidian=" << obs << "\n";
            }
        }
        check(found, "found a ruined-portal trigger chunk within 160x160");
    }

    // 3. Determinism: same seed + same chunk => identical writes.
    {
        FlatWorld a; a.stone = stone; auto wa = makeWorld(a);
        FlatWorld b; b.stone = stone; auto wb = makeWorld(b);
        auto desert = [](int, int, int) { return std::string("minecraft:desert"); };
        generateStructures({16, 16}, 12345, wa, desert);
        generateStructures({16, 16}, 12345, wb, desert);
        check(a.over == b.over, "structure generation is deterministic for a fixed seed");
    }

    if (!g_ok) { std::cerr << "Structure gen tests FAILED\n"; return 1; }
    std::cout << "Structure gen tests passed\n";
    return 0;
}
