#pragma once

#include "Noise.h"

#include <memory>
#include <string>

namespace mc::levelgen::Noises {

    constexpr const char* TEMPERATURE = "temperature";
    constexpr const char* VEGETATION = "vegetation";
    constexpr const char* CONTINENTALNESS = "continentalness";
    constexpr const char* EROSION = "erosion";
    constexpr const char* TEMPERATURE_LARGE = "temperature_large";
    constexpr const char* VEGETATION_LARGE = "vegetation_large";
    constexpr const char* CONTINENTALNESS_LARGE = "continentalness_large";
    constexpr const char* EROSION_LARGE = "erosion_large";
    constexpr const char* RIDGE = "ridge";
    constexpr const char* SHIFT = "offset";
    constexpr const char* TEMPERATURE_NETHER = "nether/temperature";
    constexpr const char* VEGETATION_NETHER = "nether/vegetation";
    constexpr const char* AQUIFER_BARRIER = "aquifer_barrier";
    constexpr const char* AQUIFER_FLUID_LEVEL_FLOODEDNESS = "aquifer_fluid_level_floodedness";
    constexpr const char* AQUIFER_LAVA = "aquifer_lava";
    constexpr const char* AQUIFER_FLUID_LEVEL_SPREAD = "aquifer_fluid_level_spread";
    constexpr const char* PILLAR = "pillar";
    constexpr const char* PILLAR_RARENESS = "pillar_rareness";
    constexpr const char* PILLAR_THICKNESS = "pillar_thickness";
    constexpr const char* SPAGHETTI_2D = "spaghetti_2d";
    constexpr const char* SPAGHETTI_2D_ELEVATION = "spaghetti_2d_elevation";
    constexpr const char* SPAGHETTI_2D_MODULATOR = "spaghetti_2d_modulator";
    constexpr const char* SPAGHETTI_2D_THICKNESS = "spaghetti_2d_thickness";
    constexpr const char* SPAGHETTI_3D_1 = "spaghetti_3d_1";
    constexpr const char* SPAGHETTI_3D_2 = "spaghetti_3d_2";
    constexpr const char* SPAGHETTI_3D_RARITY = "spaghetti_3d_rarity";
    constexpr const char* SPAGHETTI_3D_THICKNESS = "spaghetti_3d_thickness";
    constexpr const char* SPAGHETTI_ROUGHNESS = "spaghetti_roughness";
    constexpr const char* SPAGHETTI_ROUGHNESS_MODULATOR = "spaghetti_roughness_modulator";
    constexpr const char* CAVE_ENTRANCE = "cave_entrance";
    constexpr const char* CAVE_LAYER = "cave_layer";
    constexpr const char* CAVE_CHEESE = "cave_cheese";
    constexpr const char* ORE_VEININESS = "ore_veininess";
    constexpr const char* ORE_VEIN_A = "ore_vein_a";
    constexpr const char* ORE_VEIN_B = "ore_vein_b";
    constexpr const char* ORE_GAP = "ore_gap";
    constexpr const char* NOODLE = "noodle";
    constexpr const char* NOODLE_THICKNESS = "noodle_thickness";
    constexpr const char* NOODLE_RIDGE_A = "noodle_ridge_a";
    constexpr const char* NOODLE_RIDGE_B = "noodle_ridge_b";
    constexpr const char* JAGGED = "jagged";
    constexpr const char* SURFACE = "surface";
    constexpr const char* SURFACE_SECONDARY = "surface_secondary";
    constexpr const char* CLAY_BANDS_OFFSET = "clay_bands_offset";
    constexpr const char* BADLANDS_PILLAR = "badlands_pillar";
    constexpr const char* BADLANDS_PILLAR_ROOF = "badlands_pillar_roof";
    constexpr const char* BADLANDS_SURFACE = "badlands_surface";
    constexpr const char* ICEBERG_PILLAR = "iceberg_pillar";
    constexpr const char* ICEBERG_PILLAR_ROOF = "iceberg_pillar_roof";
    constexpr const char* ICEBERG_SURFACE = "iceberg_surface";
    constexpr const char* SWAMP = "surface_swamp";
    constexpr const char* CALCITE = "calcite";
    constexpr const char* GRAVEL = "gravel";
    constexpr const char* POWDER_SNOW = "powder_snow";
    constexpr const char* PACKED_ICE = "packed_ice";
    constexpr const char* ICE = "ice";
    constexpr const char* SOUL_SAND_LAYER = "soul_sand_layer";
    constexpr const char* GRAVEL_LAYER = "gravel_layer";
    constexpr const char* PATCH = "patch";
    constexpr const char* NETHERRACK = "netherrack";
    constexpr const char* NETHER_WART = "nether_wart";
    constexpr const char* NETHER_STATE_SELECTOR = "nether_state_selector";

    std::string identifier(const std::string& name);
    const NoiseParameters& parameters(const std::string& name);
    bool hasParameters(const std::string& name);
    std::shared_ptr<NormalNoise> instantiate(const PositionalRandomFactory& context, const std::string& name);

} // namespace mc::levelgen::Noises
