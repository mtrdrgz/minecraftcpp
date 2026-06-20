// CarverDiffParityTest — outputs positions changed by applyCarvers only.
// Reads a TSV of (seed, blockX, blockZ, y, block) to know which chunks to generate.
// For each unique chunk, does: fillFromNoise + buildSurface + snapshot + applyCarvers + diff.
// Outputs: seed  blockX  blockZ  y  beforeBlock  afterBlock  (only changed positions)

#include "NoiseBasedChunkGenerator.h"
#include "../block/Blocks.h"
#include "../block/BlockState.h"
#include "../chunk/LevelChunk.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace {

int floorDiv(int x, int y) {
    int q = x / y;
    int r = x % y;
    if (r != 0 && ((r < 0) != (y < 0))) --q;
    return q;
}

std::int64_t packChunk(int chunkX, int chunkZ) {
    return (static_cast<std::int64_t>(static_cast<std::uint32_t>(chunkX)) << 32)
        | static_cast<std::uint32_t>(chunkZ);
}

std::string blockNameAt(const mc::LevelChunk& chunk, int blockX, int y, int blockZ) {
    const std::uint32_t id = chunk.getBlock(blockX, y, blockZ);
    const mc::BlockState* state = mc::getBlockState(id);
    if (!state || !state->block || state->block->name.empty()) return "minecraft:air";
    return "minecraft:" + state->block->name;
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

constexpr int MIN_Y = -64;
constexpr int HEIGHT = 384;

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: carver_diff_parity --cases <tsv>\n";
        return 2;
    }

    installDefaultBlockStatesEnv();
    mc::initBlocks();

    // Read cases to know which chunks to generate
    struct Case { long long seed; int blockX, blockZ, y; };
    std::vector<Case> cases;
    {
        std::ifstream in(casesPath);
        if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }
        std::string line;
        while (std::getline(in, line)) {
            if (line.empty()) continue;
            std::istringstream ss(line);
            std::string s, x, z, y, blk;
            std::getline(ss, s, '\t'); std::getline(ss, x, '\t');
            std::getline(ss, z, '\t'); std::getline(ss, y, '\t');
            std::getline(ss, blk, '\t');
            if (blk.empty()) continue;
            Case c;
            c.seed = std::strtoll(s.c_str(), nullptr, 10);
            c.blockX = std::atoi(x.c_str());
            c.blockZ = std::atoi(z.c_str());
            c.y = std::atoi(y.c_str());
            cases.push_back(std::move(c));
        }
    }

    long long curSeed = 0;
    bool haveGen = false;
    std::unique_ptr<mc::levelgen::NoiseBasedChunkGenerator> gen;
    std::unordered_map<std::int64_t, bool> doneChunks;

    for (const auto& c : cases) {
        if (!haveGen || c.seed != curSeed) {
            gen = std::make_unique<mc::levelgen::NoiseBasedChunkGenerator>(static_cast<std::uint64_t>(c.seed));
            doneChunks.clear();
            curSeed = c.seed;
            haveGen = true;
        }

        int chunkX = floorDiv(c.blockX, 16);
        int chunkZ = floorDiv(c.blockZ, 16);
        auto key = packChunk(chunkX, chunkZ);
        if (doneChunks.count(key)) continue;
        doneChunks[key] = true;

        // Generate chunk: fillFromNoise + buildSurface
        auto chunk = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ chunkX, chunkZ });
        gen->fillFromNoise(*chunk);
        gen->buildSurface(*chunk);

        // Snapshot pre-carver
        std::unordered_map<long long, std::string> pre;
        for (int lx = 0; lx < 16; lx++)
            for (int lz = 0; lz < 16; lz++)
                for (int y = MIN_Y; y < MIN_Y + HEIGHT; y++)
                    pre[(long long)lx | ((long long)lz << 4) | ((long long)(y - MIN_Y) << 8)] =
                        blockNameAt(*chunk, chunkX*16+lx, y, chunkZ*16+lz);

        // Apply carvers
        gen->applyCarvers(*chunk);

        // Output changed positions
        for (int lx = 0; lx < 16; lx++)
            for (int lz = 0; lz < 16; lz++)
                for (int y = MIN_Y; y < MIN_Y + HEIGHT; y++) {
                    long long k = (long long)lx | ((long long)lz << 4) | ((long long)(y - MIN_Y) << 8);
                    std::string after = blockNameAt(*chunk, chunkX*16+lx, y, chunkZ*16+lz);
                    if (pre[k] != after)
                        std::cout << c.seed << "\t" << (chunkX*16+lx) << "\t" << (chunkZ*16+lz)
                                  << "\t" << y << "\t" << pre[k] << "\t" << after << "\n";
                }
    }
    return 0;
}
