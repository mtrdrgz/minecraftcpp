// Full-chunk parity test: proves the C++ NoiseBasedChunkGenerator is byte-for-byte
// identical to the real Java generator over EVERY block of a chunk (not just sampled
// columns), through fillFromNoise + buildSurface + applyCarvers.
//
// Ground truth: tools/FullChunkParity.java, which runs the real Java generator and
// dumps every block of each requested chunk in canonical order
// (lx -> lz -> y ascending), one block per TSV line.
//
//   tools/run_groundtruth.ps1 -Tool FullChunkParity -Out mcpp/build/full_chunk_cases.tsv
//   ninja -C mcpp/build full_chunk_parity
//   mcpp/build/full_chunk_parity --cases mcpp/build/full_chunk_cases.tsv
//
// The C++ side does not enumerate the chunk itself: it generates+caches each chunk
// once, then looks up exactly the (blockX, y, blockZ) cell named on each TSV line, so
// the canonical ordering is enforced solely by the Java dumper. Every TSV line is one
// compared case.

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
        std::cerr << "usage: full_chunk_parity --cases <tsv>\n";
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

    long long total = 0;
    long long mismatches = 0;
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
            it = chunks.emplace(key, std::move(chunk)).first;
        }

        const std::string got = blockNameAt(*it->second, c.blockX, c.y, c.blockZ);
        ++total;
        if (got != c.block) {
            ++mismatches;
            if (shown < 200) {
                std::cerr << "MISMATCH seed=" << c.seed << " pos=(" << c.blockX << ',' << c.y << ','
                          << c.blockZ << ") got=" << got << " expected=" << c.block << '\n';
                ++shown;
            }
        }
    }

    std::cout << "FullChunk cases=" << total << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
