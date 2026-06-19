// Dev scout: map noise biomes per chunk for a seed over a chunk rect, using the
// certified C++ climate pipeline (NoiseBasedChunkGenerator::getNoiseBiome — same
// sampler that passes climate_biome_parity / overworld_biome_parity). NOT a parity
// gate; used to find chunks of target biome classes before running the real server.
//
//   biome_scout <seed> <minCx> <minCz> <maxCx> <maxCz> [blockY]
//
// Prints one TSV row per chunk: cx cz biome (sampled at the chunk-center quart,
// quartY = blockY>>2, default blockY 64 — surface-class scouting only; mountains
// (depth>0 at y=64) can read as cave/underground variants, verify via server dump).
#include "NoiseBasedChunkGenerator.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <map>
#include <string>

int main(int argc, char** argv) {
    if (argc < 6) {
        std::cerr << "usage: biome_scout <seed> <minCx> <minCz> <maxCx> <maxCz> [blockY]\n";
        return 2;
    }
    const long long seed = std::strtoll(argv[1], nullptr, 10);
    const int minCx = std::atoi(argv[2]), minCz = std::atoi(argv[3]);
    const int maxCx = std::atoi(argv[4]), maxCz = std::atoi(argv[5]);
    const int blockY = argc > 6 ? std::atoi(argv[6]) : 64;

    mc::levelgen::NoiseBasedChunkGenerator gen(static_cast<std::uint64_t>(seed));
    std::map<std::string, long long> counts;
    for (int cx = minCx; cx <= maxCx; ++cx) {
        for (int cz = minCz; cz <= maxCz; ++cz) {
            const std::string biome = gen.getNoiseBiome(cx * 4 + 2, blockY >> 2, cz * 4 + 2);
            std::cout << cx << '\t' << cz << '\t' << biome << '\n';
            ++counts[biome];
        }
    }
    for (const auto& [biome, n] : counts) std::cerr << biome << '\t' << n << '\n';
    return 0;
}
