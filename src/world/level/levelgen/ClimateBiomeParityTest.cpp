// Parity test for the overworld Climate.Sampler + MultiNoiseBiomeSource preset.
//
// Ground truth: tools/ClimateBiomeParity.java (real decompiled RandomState,
// Climate.Sampler and MultiNoiseBiomeSourceParameterList.Preset.OVERWORLD).
//   mcpp/tools/run_groundtruth.ps1 -Tool ClimateBiomeParity -Out build/climate_biome_cases.tsv
//   climate_biome_parity --cases build/climate_biome_cases.tsv

#include "BiomeSource.h"
#include "NoiseGeneratorSettings.h"
#include "NoiseRouterData.h"
#include "RandomState.h"

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

namespace mc {
std::uint32_t getDefaultBlockStateId(std::string_view, std::uint32_t fallback) { return fallback; }
}

namespace {
struct Case {
    long long seed = 0;
    int quartX = 0;
    int quartY = 0;
    int quartZ = 0;
    mc::levelgen::Climate::TargetPoint target;
    std::string biome;
};

mc::levelgen::Climate::TargetPoint sampleTarget(const mc::levelgen::NoiseRouter& router, int quartX, int quartY, int quartZ) {
    const mc::levelgen::DensityFunctionContext context{ quartX << 2, quartY << 2, quartZ << 2 };
    return mc::levelgen::Climate::target(
        static_cast<float>(router.temperature->compute(context)),
        static_cast<float>(router.vegetation->compute(context)),
        static_cast<float>(router.continents->compute(context)),
        static_cast<float>(router.erosion->compute(context)),
        static_cast<float>(router.depth->compute(context)),
        static_cast<float>(router.ridges->compute(context)));
}

bool equalTarget(const mc::levelgen::Climate::TargetPoint& a, const mc::levelgen::Climate::TargetPoint& b) {
    return a.temperature == b.temperature
        && a.humidity == b.humidity
        && a.continentalness == b.continentalness
        && a.erosion == b.erosion
        && a.depth == b.depth
        && a.weirdness == b.weirdness;
}

void printTarget(const mc::levelgen::Climate::TargetPoint& t) {
    std::cerr << t.temperature << ',' << t.humidity << ',' << t.continentalness << ','
              << t.erosion << ',' << t.depth << ',' << t.weirdness;
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
        std::cerr << "usage: climate_biome_parity --cases <tsv>\n";
        return 2;
    }

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
        std::string seed, qx, qy, qz, temperature, humidity, continentalness, erosion, depth, weirdness;
        Case c;
        std::getline(ss, seed, '\t');
        std::getline(ss, qx, '\t');
        std::getline(ss, qy, '\t');
        std::getline(ss, qz, '\t');
        std::getline(ss, temperature, '\t');
        std::getline(ss, humidity, '\t');
        std::getline(ss, continentalness, '\t');
        std::getline(ss, erosion, '\t');
        std::getline(ss, depth, '\t');
        std::getline(ss, weirdness, '\t');
        std::getline(ss, c.biome, '\t');
        if (c.biome.empty()) {
            continue;
        }
        c.seed = std::strtoll(seed.c_str(), nullptr, 10);
        c.quartX = std::atoi(qx.c_str());
        c.quartY = std::atoi(qy.c_str());
        c.quartZ = std::atoi(qz.c_str());
        c.target.temperature = std::strtoll(temperature.c_str(), nullptr, 10);
        c.target.humidity = std::strtoll(humidity.c_str(), nullptr, 10);
        c.target.continentalness = std::strtoll(continentalness.c_str(), nullptr, 10);
        c.target.erosion = std::strtoll(erosion.c_str(), nullptr, 10);
        c.target.depth = std::strtoll(depth.c_str(), nullptr, 10);
        c.target.weirdness = std::strtoll(weirdness.c_str(), nullptr, 10);
        cases.push_back(std::move(c));
    }

    long long curSeed = 0;
    bool have = false;
    std::unique_ptr<mc::levelgen::RandomState> rs;
    mc::levelgen::NoiseRouter router;
    std::unique_ptr<mc::levelgen::BiomeSource> source;
    int total = 0;
    int mismatches = 0;
    int shown = 0;

    for (const Case& c : cases) {
        if (!have || c.seed != curSeed) {
            rs = std::make_unique<mc::levelgen::RandomState>(
                mc::levelgen::NoiseGeneratorSettings::overworld(), static_cast<std::uint64_t>(c.seed));
            router = mc::levelgen::NoiseRouterData::overworld(*rs, false, false);
            source = std::make_unique<mc::levelgen::BiomeSource>(router);
            curSeed = c.seed;
            have = true;
        }

        const mc::levelgen::Climate::TargetPoint gotTarget = sampleTarget(router, c.quartX, c.quartY, c.quartZ);
        const std::string gotBiome = source->getNoiseBiome(c.quartX, c.quartY, c.quartZ);
        ++total;
        if (!equalTarget(gotTarget, c.target) || gotBiome != c.biome) {
            ++mismatches;
            if (shown < 20) {
                std::cerr << "MISMATCH seed=" << c.seed << " quart=(" << c.quartX << ',' << c.quartY << ','
                          << c.quartZ << ") gotTarget=";
                printTarget(gotTarget);
                std::cerr << " expectedTarget=";
                printTarget(c.target);
                std::cerr << " gotBiome=" << gotBiome << " expectedBiome=" << c.biome << '\n';
                ++shown;
            }
        }
    }

    std::cout << "ClimateBiome cases=" << total << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
