#include "Noises.h"

#include <stdexcept>
#include <unordered_map>
#include <utility>

namespace mc::levelgen::Noises {

namespace {
    NoiseParameters params(int32_t firstOctave, std::initializer_list<double> amplitudes) {
        return NoiseParameters{ firstOctave, std::vector<double>(amplitudes) };
    }

    void registerNoise(std::unordered_map<std::string, NoiseParameters>& map, const char* key, int32_t firstOctave, std::initializer_list<double> amplitudes) {
        map.emplace(key, params(firstOctave, amplitudes));
    }

    void registerBiomeNoises(
        std::unordered_map<std::string, NoiseParameters>& map,
        int32_t octaveOffset,
        const char* temperature,
        const char* vegetation,
        const char* continentalness,
        const char* erosion
    ) {
        registerNoise(map, temperature, -10 + octaveOffset, { 1.5, 0.0, 1.0, 0.0, 0.0, 0.0 });
        registerNoise(map, vegetation, -8 + octaveOffset, { 1.0, 1.0, 0.0, 0.0, 0.0, 0.0 });
        registerNoise(map, continentalness, -9 + octaveOffset, { 1.0, 1.0, 2.0, 2.0, 2.0, 1.0, 1.0, 1.0, 1.0 });
        registerNoise(map, erosion, -9 + octaveOffset, { 1.0, 1.0, 0.0, 1.0, 1.0 });
    }

    const std::unordered_map<std::string, NoiseParameters>& registry() {
        static const std::unordered_map<std::string, NoiseParameters> data = [] {
            std::unordered_map<std::string, NoiseParameters> map;
            registerBiomeNoises(map, 0, TEMPERATURE, VEGETATION, CONTINENTALNESS, EROSION);
            registerBiomeNoises(map, -2, TEMPERATURE_LARGE, VEGETATION_LARGE, CONTINENTALNESS_LARGE, EROSION_LARGE);
            registerNoise(map, TEMPERATURE_NETHER, -7, { 1.0, 1.0 });
            registerNoise(map, VEGETATION_NETHER, -7, { 1.0, 1.0 });
            registerNoise(map, RIDGE, -7, { 1.0, 2.0, 1.0, 0.0, 0.0, 0.0 });
            registerNoise(map, SHIFT, -3, { 1.0, 1.0, 1.0, 0.0 });
            registerNoise(map, AQUIFER_BARRIER, -3, { 1.0 });
            registerNoise(map, AQUIFER_FLUID_LEVEL_FLOODEDNESS, -7, { 1.0 });
            registerNoise(map, AQUIFER_LAVA, -1, { 1.0 });
            registerNoise(map, AQUIFER_FLUID_LEVEL_SPREAD, -5, { 1.0 });
            registerNoise(map, PILLAR, -7, { 1.0, 1.0 });
            registerNoise(map, PILLAR_RARENESS, -8, { 1.0 });
            registerNoise(map, PILLAR_THICKNESS, -8, { 1.0 });
            registerNoise(map, SPAGHETTI_2D, -7, { 1.0 });
            registerNoise(map, SPAGHETTI_2D_ELEVATION, -8, { 1.0 });
            registerNoise(map, SPAGHETTI_2D_MODULATOR, -11, { 1.0 });
            registerNoise(map, SPAGHETTI_2D_THICKNESS, -11, { 1.0 });
            registerNoise(map, SPAGHETTI_3D_1, -7, { 1.0 });
            registerNoise(map, SPAGHETTI_3D_2, -7, { 1.0 });
            registerNoise(map, SPAGHETTI_3D_RARITY, -11, { 1.0 });
            registerNoise(map, SPAGHETTI_3D_THICKNESS, -8, { 1.0 });
            registerNoise(map, SPAGHETTI_ROUGHNESS, -5, { 1.0 });
            registerNoise(map, SPAGHETTI_ROUGHNESS_MODULATOR, -8, { 1.0 });
            registerNoise(map, CAVE_ENTRANCE, -7, { 0.4, 0.5, 1.0 });
            registerNoise(map, CAVE_LAYER, -8, { 1.0 });
            registerNoise(map, CAVE_CHEESE, -8, { 0.5, 1.0, 2.0, 1.0, 2.0, 1.0, 0.0, 2.0, 0.0 });
            registerNoise(map, ORE_VEININESS, -8, { 1.0 });
            registerNoise(map, ORE_VEIN_A, -7, { 1.0 });
            registerNoise(map, ORE_VEIN_B, -7, { 1.0 });
            registerNoise(map, ORE_GAP, -5, { 1.0 });
            registerNoise(map, NOODLE, -8, { 1.0 });
            registerNoise(map, NOODLE_THICKNESS, -8, { 1.0 });
            registerNoise(map, NOODLE_RIDGE_A, -7, { 1.0 });
            registerNoise(map, NOODLE_RIDGE_B, -7, { 1.0 });
            registerNoise(map, JAGGED, -16, { 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, SURFACE, -6, { 1.0, 1.0, 1.0 });
            registerNoise(map, SURFACE_SECONDARY, -6, { 1.0, 1.0, 0.0, 1.0 });
            registerNoise(map, CLAY_BANDS_OFFSET, -8, { 1.0 });
            registerNoise(map, BADLANDS_PILLAR, -2, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, BADLANDS_PILLAR_ROOF, -8, { 1.0 });
            registerNoise(map, BADLANDS_SURFACE, -6, { 1.0, 1.0, 1.0 });
            registerNoise(map, ICEBERG_PILLAR, -6, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, ICEBERG_PILLAR_ROOF, -3, { 1.0 });
            registerNoise(map, ICEBERG_SURFACE, -6, { 1.0, 1.0, 1.0 });
            registerNoise(map, SWAMP, -2, { 1.0 });
            registerNoise(map, CALCITE, -9, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, GRAVEL, -8, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, POWDER_SNOW, -6, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, PACKED_ICE, -7, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, ICE, -4, { 1.0, 1.0, 1.0, 1.0 });
            registerNoise(map, SOUL_SAND_LAYER, -8, { 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.013333333333333334 });
            registerNoise(map, GRAVEL_LAYER, -8, { 1.0, 1.0, 1.0, 1.0, 0.0, 0.0, 0.0, 0.0, 0.013333333333333334 });
            registerNoise(map, PATCH, -5, { 1.0, 0.0, 0.0, 0.0, 0.0, 0.013333333333333334 });
            registerNoise(map, NETHERRACK, -3, { 1.0, 0.0, 0.0, 0.35 });
            registerNoise(map, NETHER_WART, -3, { 1.0, 0.0, 0.0, 0.9 });
            registerNoise(map, NETHER_STATE_SELECTOR, -4, { 1.0 });
            return map;
        }();
        return data;
    }
}

std::string identifier(const std::string& name) {
    return name.find(':') == std::string::npos ? "minecraft:" + name : name;
}

const NoiseParameters& parameters(const std::string& name) {
    const auto& map = registry();
    std::string key = name;
    if (key.starts_with("minecraft:")) {
        key = key.substr(10);
    }
    auto it = map.find(key);
    if (it == map.end()) {
        throw std::out_of_range("Unknown Minecraft noise parameters: " + name);
    }
    return it->second;
}

bool hasParameters(const std::string& name) {
    const auto& map = registry();
    std::string key = name;
    if (key.starts_with("minecraft:")) {
        key = key.substr(10);
    }
    return map.find(key) != map.end();
}

std::shared_ptr<NormalNoise> instantiate(const PositionalRandomFactory& context, const std::string& name) {
    const NoiseParameters& noiseParameters = parameters(name);
    std::shared_ptr<RandomSource> random = context.fromHashOf(identifier(name));
    return std::make_shared<NormalNoise>(NormalNoise::create(*random, noiseParameters));
}

} // namespace mc::levelgen::Noises
