// Parity test for overworld terrain columns after Java buildSurface+applyCarvers,
// before structures/features/decoration.
//
// Ground truth: tools/CarvedTerrainColumnParity.java, which runs the real Java
// configured world carvers for the 17x17 source-chunk area.

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
struct Case {
    long long seed = 0;
    int blockX = 0;
    int blockZ = 0;
    int y = 0;
    std::string block;
};

int floorDiv(int x, int y) {
    int q = x / y;
    int r = x % y;
    if (r != 0 && ((r < 0) != (y < 0))) {
        --q;
    }
    return q;
}

std::int64_t packChunk(int chunkX, int chunkZ) {
    return (static_cast<std::int64_t>(static_cast<std::uint32_t>(chunkX)) << 32)
        | static_cast<std::uint32_t>(chunkZ);
}

std::string blockNameAt(const mc::LevelChunk& chunk, int blockX, int y, int blockZ) {
    const std::uint32_t id = chunk.getBlock(blockX, y, blockZ);
    const mc::BlockState* state = mc::getBlockState(id);
    if (!state || !state->block || state->block->name.empty()) {
        return "minecraft:air";
    }
    return "minecraft:" + state->block->name;
}

void installDefaultBlockStatesEnv() {
    if (std::getenv("MCPP_BLOCK_STATES")) {
        return;
    }
    for (const char* path : { "src/assets/block_states.json", "src/assets/block_states.json" }) {
        std::ifstream probe(path, std::ios::binary);
        if (!probe) {
            continue;
        }
#if defined(_WIN32)
        _putenv_s("MCPP_BLOCK_STATES", path);
#else
        setenv("MCPP_BLOCK_STATES", path, 0);
#endif
        return;
    }
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) {
            casesPath = argv[++i];
        }
    }
    if (casesPath.empty()) {
        std::cerr << "usage: carved_terrain_column_parity --cases <tsv>\n";
        return 2;
    }

    installDefaultBlockStatesEnv();
    mc::initBlocks();

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    std::vector<Case> cases;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) {
            continue;
        }
        std::istringstream ss(line);
        std::string seed, x, z, y;
        Case c;
        std::getline(ss, seed, '\t');
        std::getline(ss, x, '\t');
        std::getline(ss, z, '\t');
        std::getline(ss, y, '\t');
        std::getline(ss, c.block, '\t');
        if (c.block.empty()) {
            continue;
        }
        c.seed = std::strtoll(seed.c_str(), nullptr, 10);
        c.blockX = std::atoi(x.c_str());
        c.blockZ = std::atoi(z.c_str());
        c.y = std::atoi(y.c_str());
        cases.push_back(std::move(c));
    }

    long long curSeed = 0;
    bool haveGenerator = false;
    std::unique_ptr<mc::levelgen::NoiseBasedChunkGenerator> generator;
    std::unordered_map<std::int64_t, std::unique_ptr<mc::LevelChunk>> chunks;

    int total = 0;
    int mismatches = 0;
    int shown = 0;

    for (const Case& c : cases) {
        if (!haveGenerator || c.seed != curSeed) {
            generator = std::make_unique<mc::levelgen::NoiseBasedChunkGenerator>(static_cast<std::uint64_t>(c.seed));
            chunks.clear();
            curSeed = c.seed;
            haveGenerator = true;
        }

        const int chunkX = floorDiv(c.blockX, 16);
        const int chunkZ = floorDiv(c.blockZ, 16);
        const std::int64_t key = packChunk(chunkX, chunkZ);
        auto it = chunks.find(key);
        if (it == chunks.end()) {
            auto chunk = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ chunkX, chunkZ });
            generator->fillFromNoise(*chunk);
            generator->buildSurface(*chunk);
            generator->applyCarvers(*chunk);
            const char* dumpFile = std::getenv("MCPP_DUMP_FILE");
            if (dumpFile && c.seed == 1 && chunkX == 0 && chunkZ == 0) {
                std::ofstream df(dumpFile);
                for (int lx = 0; lx < 16; ++lx) {
                    for (int lz = 0; lz < 16; ++lz) {
                        for (int yy = -64; yy < -64 + 384; ++yy) {
                            const std::string b = blockNameAt(*chunk, lx, yy, lz);
                            if (b == "minecraft:air" || b == "minecraft:water" || b == "minecraft:lava") {
                                df << lx << '\t' << yy << '\t' << lz << '\t' << b << '\n';
                            }
                        }
                    }
                }
            }
            it = chunks.emplace(key, std::move(chunk)).first;
        }

        const std::string got = blockNameAt(*it->second, c.blockX, c.y, c.blockZ);
        ++total;
        if (got != c.block) {
            ++mismatches;
            if (shown < 120) {
                std::cerr << "MISMATCH seed=" << c.seed << " pos=(" << c.blockX << ',' << c.y << ','
                          << c.blockZ << ") got=" << got << " expected=" << c.block << '\n';
                ++shown;
            }
        }
    }

    std::cout << "CarvedTerrainColumn cases=" << total << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
