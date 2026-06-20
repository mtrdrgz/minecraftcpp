#include "../world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "../world/level/block/Blocks.h"
#include "../world/level/chunk/LevelChunk.h"
#include "../render/level/ChunkMesh.h"

#include <chrono>
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {
using Clock = std::chrono::high_resolution_clock;

double elapsedMs(Clock::time_point a, Clock::time_point b) {
    return std::chrono::duration<double, std::milli>(b - a).count();
}

void installDefaultBlockStatesEnv() {
    if (std::getenv("MCPP_BLOCK_STATES")) {
        return;
    }
    for (const char* path : { "src/assets/block_states.json", "../src/assets/block_states.json" }) {
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
    int radius = 3;
    std::uint64_t seed = 1;
    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        if (arg == "--radius" && i + 1 < argc) {
            radius = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--seed" && i + 1 < argc) {
            seed = static_cast<std::uint64_t>(std::strtoull(argv[++i], nullptr, 10));
        }
    }

    installDefaultBlockStatesEnv();
    mc::initBlocks();

    mc::levelgen::NoiseBasedChunkGenerator generator(seed);
    std::vector<std::unique_ptr<mc::LevelChunk>> chunks;
    chunks.reserve(static_cast<std::size_t>((radius * 2 + 1) * (radius * 2 + 1)));

    double fillMs = 0.0;
    double surfaceMs = 0.0;
    double carverMs = 0.0;
    double meshMs = 0.0;
    std::size_t meshVertices = 0;
    std::size_t meshIndices = 0;

    for (int z = -radius; z <= radius; ++z) {
        for (int x = -radius; x <= radius; ++x) {
            auto chunk = std::make_unique<mc::LevelChunk>(mc::ChunkPos{ x, z });
            std::vector<mc::BlockPos> marks;

            auto t0 = Clock::now();
            generator.fillFromNoise(*chunk, &marks);
            auto t1 = Clock::now();
            generator.buildSurface(*chunk);
            auto t2 = Clock::now();
            generator.applyCarvers(*chunk, &marks);
            auto t3 = Clock::now();

            fillMs += elapsedMs(t0, t1);
            surfaceMs += elapsedMs(t1, t2);
            carverMs += elapsedMs(t2, t3);
            chunks.push_back(std::move(chunk));
        }
    }

    const mc::LevelChunk* noNeighbors[4] = { nullptr, nullptr, nullptr, nullptr };
    for (const auto& chunk : chunks) {
        auto t0 = Clock::now();
        auto meshes = mc::render::ChunkMesher::buildChunk(*chunk, noNeighbors, nullptr);
        auto t1 = Clock::now();
        meshMs += elapsedMs(t0, t1);
        for (const auto& section : meshes) {
            meshVertices += section.vertices.size();
            meshIndices += section.indices.size();
        }
    }

    const double count = static_cast<double>(chunks.size());
    std::cout << "TerrainEnginePerf seed=" << seed << " radius=" << radius
              << " chunks=" << chunks.size() << "\n";
    std::cout << "fillFromNoise total_ms=" << fillMs << " avg_ms=" << (fillMs / count) << "\n";
    std::cout << "buildSurface total_ms=" << surfaceMs << " avg_ms=" << (surfaceMs / count) << "\n";
    std::cout << "applyCarvers total_ms=" << carverMs << " avg_ms=" << (carverMs / count) << "\n";
    std::cout << "chunkMesh total_ms=" << meshMs << " avg_ms=" << (meshMs / count)
              << " vertices=" << meshVertices << " indices=" << meshIndices << "\n";
    return 0;
}
