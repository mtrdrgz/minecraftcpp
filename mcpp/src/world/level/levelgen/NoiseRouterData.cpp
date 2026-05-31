#include "NoiseRouterData.h"
#include "Noise.h"
#include "Noises.h"
#include "NoiseSettings.h"
#include "RandomState.h"
#include "TerrainProvider.h"
#include <utility>

namespace mc::levelgen::NoiseRouterData {

namespace {
    DensityFunctionPtr identityMarker(DensityFunctionPtr input) {
        return input;
    }

    NoiseRouter simpleRouter(DensityFunctionPtr fullNoise) {
        auto zero = DensityFunctions::zero();
        return NoiseRouter{
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            zero,
            std::move(fullNoise),
            zero,
            zero,
            zero
        };
    }

    DensityFunctionPtr overworldBase3d(RandomState& randomState) {
        return DensityFunctions::blendedNoise(BlendedNoise(randomState.terrainRandom(), 0.25, 0.125, 80.0, 160.0, 8.0));
    }

    DensityFunctionPtr netherBase3d(RandomState& randomState) {
        return DensityFunctions::blendedNoise(BlendedNoise(randomState.terrainRandom(), 0.25, 0.375, 80.0, 60.0, 8.0));
    }

    DensityFunctionPtr endBase3d(RandomState& randomState) {
        return DensityFunctions::blendedNoise(BlendedNoise(randomState.terrainRandom(), 0.25, 0.25, 80.0, 160.0, 4.0));
    }

    DensityFunctionPtr remap(DensityFunctionPtr input, double fromMin, double fromMax, double toMin, double toMax) {
        double factor = (toMax - toMin) / (fromMax - fromMin);
        double offset = toMin - fromMin * factor;
        return DensityFunctions::add(
            DensityFunctions::mul(std::move(input), DensityFunctions::constant(factor)),
            DensityFunctions::constant(offset));
    }

    DensityFunctionPtr offsetToDepth(DensityFunctionPtr offset) {
        return DensityFunctions::add(DensityFunctions::yClampedGradient(-64, 320, 1.5, -1.5), std::move(offset));
    }

    DensityFunctionPtr noiseGradientDensity(DensityFunctionPtr factor, DensityFunctionPtr depthWithJaggedness) {
        DensityFunctionPtr gradientUnscaled = DensityFunctions::mul(std::move(depthWithJaggedness), std::move(factor));
        return DensityFunctions::mul(DensityFunctions::constant(4.0), DensityFunctions::map(std::move(gradientUnscaled), DensityFunctions::MapType::QuarterNegative));
    }

    DensityFunctionPtr preliminarySurfaceLevel(DensityFunctionPtr offset, DensityFunctionPtr factor, bool amplified) {
        DensityFunctionPtr cachedFactor = std::move(factor);
        DensityFunctionPtr cachedOffset = std::move(offset);
        DensityFunctionPtr upperBound = remap(
            DensityFunctions::add(
                DensityFunctions::mul(DensityFunctions::constant(0.2734375), DensityFunctions::map(cachedFactor, DensityFunctions::MapType::Invert)),
                DensityFunctions::mul(DensityFunctions::constant(-1.0), cachedOffset)),
            1.5,
            -1.5,
            -64.0,
            320.0);
        upperBound = DensityFunctions::clamp(std::move(upperBound), -40.0, 320.0);
        DensityFunctionPtr density = DensityFunctions::add(
            slideOverworld(
                amplified,
                DensityFunctions::clamp(
                    DensityFunctions::add(
                        noiseGradientDensity(cachedFactor, offsetToDepth(cachedOffset)),
                        DensityFunctions::constant(-0.703125)),
                    -64.0,
                    64.0)),
            DensityFunctions::constant(-0.390625));
        return DensityFunctions::findTopSurface(std::move(density), std::move(upperBound), -64, NoiseSettings::overworld().getCellHeight());
    }

    DensityFunctionPtr yLimitedInterpolatable(
        DensityFunctionPtr y,
        DensityFunctionPtr whenInRange,
        int minYInclusive,
        int maxYInclusive,
        double whenOutOfRange
    ) {
        return DensityFunctions::interpolated(
            DensityFunctions::rangeChoice(
                std::move(y),
                minYInclusive,
                maxYInclusive + 1.0,
                std::move(whenInRange),
                DensityFunctions::constant(whenOutOfRange)));
    }

    DensityFunctionPtr spaghettiRoughnessFunction(RandomState& randomState) {
        auto spaghettiRoughnessNoise = DensityFunctions::noise(randomState.getOrCreateNoise(Noises::SPAGHETTI_ROUGHNESS));
        auto spaghettiRoughnessModulator = DensityFunctions::mappedNoise(
            randomState.getOrCreateNoise(Noises::SPAGHETTI_ROUGHNESS_MODULATOR), 0.0, -0.1);
        return DensityFunctions::cacheOnce(DensityFunctions::mul(
            std::move(spaghettiRoughnessModulator),
            DensityFunctions::add(
                DensityFunctions::map(std::move(spaghettiRoughnessNoise), DensityFunctions::MapType::Abs),
                DensityFunctions::constant(-0.4))));
    }

    DensityFunctionPtr entrances(RandomState& randomState, DensityFunctionPtr spaghettiRoughness) {
        auto spaghetti3DRarityModulator = DensityFunctions::cacheOnce(DensityFunctions::noise(
            randomState.getOrCreateNoise(Noises::SPAGHETTI_3D_RARITY), 2.0, 1.0));
        auto spaghetti3DThicknessModulator = DensityFunctions::mappedNoise(
            randomState.getOrCreateNoise(Noises::SPAGHETTI_3D_THICKNESS), -0.065, -0.088);
        auto spaghetti3DCave1 = DensityFunctions::weirdScaledSampler(
            spaghetti3DRarityModulator,
            randomState.getOrCreateNoise(Noises::SPAGHETTI_3D_1),
            DensityFunctions::RarityValueMapper::Type1);
        auto spaghetti3DCave2 = DensityFunctions::weirdScaledSampler(
            spaghetti3DRarityModulator,
            randomState.getOrCreateNoise(Noises::SPAGHETTI_3D_2),
            DensityFunctions::RarityValueMapper::Type1);
        auto spaghetti3DFunction = DensityFunctions::clamp(
            DensityFunctions::add(
                DensityFunctions::max(std::move(spaghetti3DCave1), std::move(spaghetti3DCave2)),
                std::move(spaghetti3DThicknessModulator)),
            -1.0,
            1.0);
        auto bigEntranceNoiseSource = DensityFunctions::noise(
            randomState.getOrCreateNoise(Noises::CAVE_ENTRANCE), 0.75, 0.5);
        auto bigEntrancesFunction = DensityFunctions::add(
            DensityFunctions::add(std::move(bigEntranceNoiseSource), DensityFunctions::constant(0.37)),
            DensityFunctions::yClampedGradient(-10, 30, 0.3, 0.0));
        return DensityFunctions::cacheOnce(DensityFunctions::min(
            std::move(bigEntrancesFunction),
            DensityFunctions::add(std::move(spaghettiRoughness), std::move(spaghetti3DFunction))));
    }

    DensityFunctionPtr pillars(RandomState& randomState) {
        auto pillarNoiseSource = DensityFunctions::noise(randomState.getOrCreateNoise(Noises::PILLAR), 25.0, 0.3);
        auto pillarRarenessModulator = DensityFunctions::mappedNoise(
            randomState.getOrCreateNoise(Noises::PILLAR_RARENESS), 0.0, -2.0);
        auto pillarThicknessModulator = DensityFunctions::mappedNoise(
            randomState.getOrCreateNoise(Noises::PILLAR_THICKNESS), 0.0, 1.1);
        auto pillarsWithRareness = DensityFunctions::add(
            DensityFunctions::mul(std::move(pillarNoiseSource), DensityFunctions::constant(2.0)),
            std::move(pillarRarenessModulator));
        return DensityFunctions::cacheOnce(DensityFunctions::mul(
            std::move(pillarsWithRareness),
            DensityFunctions::map(std::move(pillarThicknessModulator), DensityFunctions::MapType::Cube)));
    }

    DensityFunctionPtr spaghetti2D(RandomState& randomState, DensityFunctionPtr spaghetti2DThicknessModulator) {
        auto spaghetti2DRarityModulator = DensityFunctions::noise(
            randomState.getOrCreateNoise(Noises::SPAGHETTI_2D_MODULATOR), 2.0, 1.0);
        auto spaghetti2DCave = DensityFunctions::weirdScaledSampler(
            std::move(spaghetti2DRarityModulator),
            randomState.getOrCreateNoise(Noises::SPAGHETTI_2D),
            DensityFunctions::RarityValueMapper::Type2);
        auto spaghetti2DElevationModulator = DensityFunctions::mappedNoise(
            randomState.getOrCreateNoise(Noises::SPAGHETTI_2D_ELEVATION), 1.0, 0.0, -8.0, 8.0);
        auto slopedSpaghetti = DensityFunctions::map(
            DensityFunctions::add(
                std::move(spaghetti2DElevationModulator),
                DensityFunctions::yClampedGradient(-64, 320, 8.0, -40.0)),
            DensityFunctions::MapType::Abs);
        auto layerRidged = DensityFunctions::map(
            DensityFunctions::add(slopedSpaghetti, spaghetti2DThicknessModulator),
            DensityFunctions::MapType::Cube);
        auto caveNoise = DensityFunctions::add(
            std::move(spaghetti2DCave),
            DensityFunctions::mul(DensityFunctions::constant(0.083), std::move(spaghetti2DThicknessModulator)));
        return DensityFunctions::clamp(
            DensityFunctions::max(std::move(caveNoise), std::move(layerRidged)),
            -1.0,
            1.0);
    }

    DensityFunctionPtr noodle(RandomState& randomState) {
        auto y = DensityFunctions::y();
        auto noodleToggle = yLimitedInterpolatable(
            y,
            DensityFunctions::noise(randomState.getOrCreateNoise(Noises::NOODLE), 1.0, 1.0),
            -60,
            320,
            -1.0);
        auto noodleThickness = yLimitedInterpolatable(
            y,
            DensityFunctions::mappedNoise(randomState.getOrCreateNoise(Noises::NOODLE_THICKNESS), 1.0, 1.0, -0.05, -0.1),
            -60,
            320,
            0.0);
        constexpr double noodleRidgeFrequency = 2.6666666666666665;
        auto noodleRidgeA = yLimitedInterpolatable(
            y,
            DensityFunctions::noise(randomState.getOrCreateNoise(Noises::NOODLE_RIDGE_A), noodleRidgeFrequency, noodleRidgeFrequency),
            -60,
            320,
            0.0);
        auto noodleRidgeB = yLimitedInterpolatable(
            std::move(y),
            DensityFunctions::noise(randomState.getOrCreateNoise(Noises::NOODLE_RIDGE_B), noodleRidgeFrequency, noodleRidgeFrequency),
            -60,
            320,
            0.0);
        auto noodleRidged = DensityFunctions::mul(
            DensityFunctions::constant(1.5),
            DensityFunctions::max(
                DensityFunctions::map(std::move(noodleRidgeA), DensityFunctions::MapType::Abs),
                DensityFunctions::map(std::move(noodleRidgeB), DensityFunctions::MapType::Abs)));
        return DensityFunctions::rangeChoice(
            std::move(noodleToggle),
            -1000000.0,
            0.0,
            DensityFunctions::constant(64.0),
            DensityFunctions::add(std::move(noodleThickness), std::move(noodleRidged)));
    }

    DensityFunctionPtr underground(
        RandomState& randomState,
        DensityFunctionPtr slopedCheese,
        DensityFunctionPtr spaghetti2DFunction,
        DensityFunctionPtr spaghettiRoughness,
        DensityFunctionPtr entrancesFunction,
        DensityFunctionPtr pillarsFunction
    ) {
        auto layerNoiseSource = DensityFunctions::noise(randomState.getOrCreateNoise(Noises::CAVE_LAYER), 1.0, 8.0);
        auto layerizedCavernsFunction = DensityFunctions::mul(
            DensityFunctions::constant(4.0),
            DensityFunctions::map(std::move(layerNoiseSource), DensityFunctions::MapType::Square));
        auto cheese = DensityFunctions::noise(randomState.getOrCreateNoise(Noises::CAVE_CHEESE), 1.0, 0.6666666666666666);
        auto solidifiedCheeseWithTopSlide = DensityFunctions::add(
            DensityFunctions::clamp(
                DensityFunctions::add(DensityFunctions::constant(0.27), std::move(cheese)),
                -1.0,
                1.0),
            DensityFunctions::clamp(
                DensityFunctions::add(
                    DensityFunctions::constant(1.5),
                    DensityFunctions::mul(DensityFunctions::constant(-0.64), std::move(slopedCheese))),
                0.0,
                0.5));
        auto baseCaveDensity = DensityFunctions::add(
            std::move(layerizedCavernsFunction),
            std::move(solidifiedCheeseWithTopSlide));
        auto undergroundSubtractions = DensityFunctions::min(
            DensityFunctions::min(std::move(baseCaveDensity), std::move(entrancesFunction)),
            DensityFunctions::add(std::move(spaghetti2DFunction), std::move(spaghettiRoughness)));
        auto pillarsInput = pillarsFunction;
        auto pillarsWhenOutOfRange = pillarsFunction;
        auto pillarsWithCutoff = DensityFunctions::rangeChoice(
            std::move(pillarsInput),
            -1000000.0,
            0.03,
            DensityFunctions::constant(-1000000.0),
            std::move(pillarsWhenOutOfRange));
        return DensityFunctions::max(std::move(undergroundSubtractions), std::move(pillarsWithCutoff));
    }
}

NoiseRouter none() {
    return simpleRouter(DensityFunctions::zero());
}

NoiseRouter overworld(RandomState& randomState, bool largeBiomes, bool amplified) {
    auto zero = DensityFunctions::zero();
    auto shiftNoise = randomState.getOrCreateNoise(Noises::SHIFT);
    auto shiftX = DensityFunctions::shiftA(shiftNoise);
    auto shiftZ = DensityFunctions::shiftB(shiftNoise);

    auto temperature = DensityFunctions::shiftedNoise2d(
        shiftX, shiftZ, 0.25, randomState.getOrCreateNoise(largeBiomes ? Noises::TEMPERATURE_LARGE : Noises::TEMPERATURE));
    auto vegetation = DensityFunctions::shiftedNoise2d(
        shiftX, shiftZ, 0.25, randomState.getOrCreateNoise(largeBiomes ? Noises::VEGETATION_LARGE : Noises::VEGETATION));
    auto continents = DensityFunctions::shiftedNoise2d(
        shiftX, shiftZ, 0.25, randomState.getOrCreateNoise(largeBiomes ? Noises::CONTINENTALNESS_LARGE : Noises::CONTINENTALNESS));
    auto erosion = DensityFunctions::shiftedNoise2d(
        shiftX, shiftZ, 0.25, randomState.getOrCreateNoise(largeBiomes ? Noises::EROSION_LARGE : Noises::EROSION));
    auto ridges = DensityFunctions::shiftedNoise2d(
        shiftX, shiftZ, 0.25, randomState.getOrCreateNoise(Noises::RIDGE));
    auto ridgesFolded = DensityFunctions::peaksAndValleys(ridges);

    auto continentsCoordinate = densityCoordinate(continents);
    auto erosionCoordinate = densityCoordinate(erosion);
    auto weirdnessCoordinate = densityCoordinate(ridges);
    auto ridgesCoordinate = densityCoordinate(ridgesFolded);

    auto offset = DensityFunctions::add(
        DensityFunctions::constant(-0.50375),
        spline(TerrainProvider::overworldOffset(continentsCoordinate, erosionCoordinate, ridgesCoordinate, amplified)));
    auto factor = spline(TerrainProvider::overworldFactor(continentsCoordinate, erosionCoordinate, weirdnessCoordinate, ridgesCoordinate, amplified));
    auto depth = offsetToDepth(offset);
    auto unscaledJaggedness = spline(TerrainProvider::overworldJaggedness(continentsCoordinate, erosionCoordinate, weirdnessCoordinate, ridgesCoordinate, amplified));
    auto jaggedNoise = DensityFunctions::noise(randomState.getOrCreateNoise(Noises::JAGGED), 1500.0, 0.0);
    auto jaggedness = DensityFunctions::mul(std::move(unscaledJaggedness), DensityFunctions::map(std::move(jaggedNoise), DensityFunctions::MapType::HalfNegative));
    auto initialDensity = noiseGradientDensity(factor, DensityFunctions::add(depth, std::move(jaggedness)));
    auto preliminary = preliminarySurfaceLevel(offset, factor, amplified);

    auto base3d = overworldBase3d(randomState);
    auto slopedCheese = DensityFunctions::add(std::move(initialDensity), std::move(base3d));
    auto spaghettiRoughness = spaghettiRoughnessFunction(randomState);
    auto spaghetti2DThicknessModulator = DensityFunctions::cacheOnce(DensityFunctions::mappedNoise(
        randomState.getOrCreateNoise(Noises::SPAGHETTI_2D_THICKNESS), 2.0, 1.0, -0.6, -1.3));
    auto spaghetti2DFunction = spaghetti2D(randomState, std::move(spaghetti2DThicknessModulator));
    auto entrancesFunction = entrances(randomState, spaghettiRoughness);
    auto pillarsFunction = pillars(randomState);
    auto noodleFunction = noodle(randomState);
    auto surfaceWithEntrances = DensityFunctions::min(
        slopedCheese,
        DensityFunctions::mul(DensityFunctions::constant(5.0), entrancesFunction));
    auto caves = DensityFunctions::rangeChoice(
        slopedCheese,
        -1000000.0,
        1.5625,
        std::move(surfaceWithEntrances),
        underground(
            randomState,
            slopedCheese,
            std::move(spaghetti2DFunction),
            std::move(spaghettiRoughness),
            std::move(entrancesFunction),
            std::move(pillarsFunction)));
    auto fullNoise = DensityFunctions::min(
        postProcess(slideOverworld(amplified, std::move(caves))),
        std::move(noodleFunction));
    auto veinA = DensityFunctions::map(DensityFunctions::noise(randomState.getOrCreateNoise(Noises::ORE_VEIN_A), 4.0, 4.0), DensityFunctions::MapType::Abs);
    auto veinB = DensityFunctions::map(DensityFunctions::noise(randomState.getOrCreateNoise(Noises::ORE_VEIN_B), 4.0, 4.0), DensityFunctions::MapType::Abs);

    return NoiseRouter{
        DensityFunctions::noise(randomState.getOrCreateNoise(Noises::AQUIFER_BARRIER), 1.0, 0.5),
        DensityFunctions::noise(randomState.getOrCreateNoise(Noises::AQUIFER_FLUID_LEVEL_FLOODEDNESS), 1.0, 0.67),
        DensityFunctions::noise(randomState.getOrCreateNoise(Noises::AQUIFER_FLUID_LEVEL_SPREAD), 1.0, 0.7142857142857143),
        DensityFunctions::noise(randomState.getOrCreateNoise(Noises::AQUIFER_LAVA)),
        std::move(temperature),
        std::move(vegetation),
        std::move(continents),
        std::move(erosion),
        std::move(depth),
        std::move(ridges),
        std::move(preliminary),
        std::move(fullNoise),
        DensityFunctions::noise(randomState.getOrCreateNoise(Noises::ORE_VEININESS), 1.5, 1.5),
        DensityFunctions::add(DensityFunctions::constant(-0.08), DensityFunctions::max(std::move(veinA), std::move(veinB))),
        DensityFunctions::noise(randomState.getOrCreateNoise(Noises::ORE_GAP))
    };
}

NoiseRouter nether(RandomState& randomState) {
    auto zero = DensityFunctions::zero();
    auto temperature = DensityFunctions::shiftedNoise2d(zero, zero, 0.25, randomState.getOrCreateNoise(Noises::TEMPERATURE_NETHER));
    auto vegetation = DensityFunctions::shiftedNoise2d(zero, zero, 0.25, randomState.getOrCreateNoise(Noises::VEGETATION_NETHER));
    auto fullNoise = postProcess(slideNetherLike(netherBase3d(randomState), 0, 128));

    return NoiseRouter{
        zero,
        zero,
        zero,
        zero,
        std::move(temperature),
        std::move(vegetation),
        zero,
        zero,
        zero,
        zero,
        zero,
        std::move(fullNoise),
        zero,
        zero,
        zero
    };
}

NoiseRouter end(RandomState& randomState) {
    auto zero = DensityFunctions::zero();
    auto islands = DensityFunctions::endIslands(0);
    auto fullNoise = postProcess(slideEndLike(DensityFunctions::add(DensityFunctions::endIslands(0), endBase3d(randomState)), 0, 128));
    return NoiseRouter{
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        zero,
        std::move(islands),
        zero,
        zero,
        zero,
        std::move(fullNoise),
        zero,
        zero,
        zero
    };
}

DensityFunctionPtr slide(
    DensityFunctionPtr caves,
    int minY,
    int height,
    int topStartY,
    int topEndY,
    double topTarget,
    int bottomStartY,
    int bottomEndY,
    double bottomTarget
) {
    DensityFunctionPtr noiseValue = std::move(caves);
    DensityFunctionPtr topFactor = DensityFunctions::yClampedGradient(minY + height - topStartY, minY + height - topEndY, 1.0, 0.0);
    noiseValue = DensityFunctions::lerp(std::move(topFactor), topTarget, std::move(noiseValue));
    DensityFunctionPtr bottomFactor = DensityFunctions::yClampedGradient(minY + bottomStartY, minY + bottomEndY, 0.0, 1.0);
    return DensityFunctions::lerp(std::move(bottomFactor), bottomTarget, std::move(noiseValue));
}

DensityFunctionPtr slideOverworld(bool amplified, DensityFunctionPtr caves) {
    return slide(std::move(caves), -64, 384, amplified ? 16 : 80, amplified ? 0 : 64,
                 -0.078125, 0, 24, amplified ? 0.4 : 0.1171875);
}

DensityFunctionPtr slideNetherLike(DensityFunctionPtr base3dNoiseNether, int minY, int height) {
    return slide(std::move(base3dNoiseNether), minY, height, 24, 0, 0.9375, -8, 24, 2.5);
}

DensityFunctionPtr slideEndLike(DensityFunctionPtr caves, int minY, int height) {
    return slide(std::move(caves), minY, height, 72, -184, -23.4375, 4, 32, -0.234375);
}

DensityFunctionPtr postProcess(DensityFunctionPtr slide) {
    DensityFunctionPtr blended = identityMarker(std::move(slide));
    DensityFunctionPtr interpolated = DensityFunctions::interpolated(std::move(blended));
    return DensityFunctions::map(
        DensityFunctions::mul(std::move(interpolated), DensityFunctions::constant(0.64)),
        DensityFunctions::MapType::Squeeze);
}

double peaksAndValleys(double weirdness) {
    return DensityFunctions::peaksAndValleys(weirdness);
}

} // namespace mc::levelgen::NoiseRouterData
