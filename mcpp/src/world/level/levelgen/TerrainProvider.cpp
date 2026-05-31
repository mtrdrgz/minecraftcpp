#include "TerrainProvider.h"
#include "NoiseRouterData.h"

#include <algorithm>
#include <cmath>

namespace mc::levelgen::TerrainProvider {

namespace {
    using Transform = CubicSpline::ValueTransform;

    float noTransform(float value) {
        return value;
    }

    float amplifiedOffset(float offset) {
        return offset < 0.0f ? offset : offset * 2.0f;
    }

    float amplifiedFactor(float factor) {
        return 1.25f - 6.25f / (factor + 5.0f);
    }

    float amplifiedJaggedness(float jaggedness) {
        return jaggedness * 2.0f;
    }

    float lerp(float alpha, float p0, float p1) {
        return p0 + alpha * (p1 - p0);
    }

    float calculateSlope(float y1, float y2, float x1, float x2) {
        return (y2 - y1) / (x2 - x1);
    }

    CubicSplinePtr ridgeSpline(
        BoundedFloatFunctionPtr ridges,
        float valley,
        float low,
        float mid,
        float high,
        float peaks,
        float minValleySteepness,
        Transform offsetTransformer
    ) {
        float d1 = std::max(0.5f * (low - valley), minValleySteepness);
        float d2 = 5.0f * (mid - low);
        auto builder = CubicSpline::builder(std::move(ridges), std::move(offsetTransformer));
        builder.addPoint(-1.0f, valley, d1);
        builder.addPoint(-0.4f, low, std::min(d1, d2));
        builder.addPoint(0.0f, mid, d2);
        builder.addPoint(0.4f, high, 2.0f * (high - mid));
        builder.addPoint(1.0f, peaks, 0.7f * (peaks - high));
        return builder.build();
    }

    float mountainContinentalness(float ridge, float modulation, float allowRiversBelow) {
        float ridgeSlope = 1.0f - (1.0f - modulation) * 0.5f;
        float ridgeIntersect = 0.5f * (1.0f - modulation);
        float adjustedRidgeHeight = (ridge + 1.17f) * 0.46082947f;
        float continentalness = adjustedRidgeHeight * ridgeSlope - ridgeIntersect;
        return ridge < allowRiversBelow ? std::max(continentalness, -0.2222f) : std::max(continentalness, 0.0f);
    }

    float calculateMountainRidgeZeroContinentalnessPoint(float modulation) {
        float ridgeSlope = 1.0f - (1.0f - modulation) * 0.5f;
        float ridgeIntersect = 0.5f * (1.0f - modulation);
        return ridgeIntersect / (0.46082947f * ridgeSlope) - 1.17f;
    }

    CubicSplinePtr buildMountainRidgeSplineWithPoints(BoundedFloatFunctionPtr ridges, float modulation, bool saddle, Transform offsetTransformer) {
        auto build = CubicSpline::builder(std::move(ridges), std::move(offsetTransformer));
        float minPointContinentalness = mountainContinentalness(-1.0f, modulation, -0.7f);
        float maxPointContinentalness = mountainContinentalness(1.0f, modulation, -0.7f);
        float ridgeZeroPoint = calculateMountainRidgeZeroContinentalnessPoint(modulation);
        if (-0.65f < ridgeZeroPoint && ridgeZeroPoint < 1.0f) {
            float afterRiverThresholdContinentalness = mountainContinentalness(-0.65f, modulation, -0.7f);
            float beforeRiverThresholdContinentalness = mountainContinentalness(-0.75f, modulation, -0.7f);
            float minPointDerivative = calculateSlope(minPointContinentalness, beforeRiverThresholdContinentalness, -1.0f, -0.75f);
            build.addPoint(-1.0f, minPointContinentalness, minPointDerivative);
            build.addPoint(-0.75f, beforeRiverThresholdContinentalness);
            build.addPoint(-0.65f, afterRiverThresholdContinentalness);
            float ridgeZeroPointContinentalness = mountainContinentalness(ridgeZeroPoint, modulation, -0.7f);
            float maxPointDerivative = calculateSlope(ridgeZeroPointContinentalness, maxPointContinentalness, ridgeZeroPoint, 1.0f);
            build.addPoint(ridgeZeroPoint - 0.01f, ridgeZeroPointContinentalness);
            build.addPoint(ridgeZeroPoint, ridgeZeroPointContinentalness, maxPointDerivative);
            build.addPoint(1.0f, maxPointContinentalness, maxPointDerivative);
        } else {
            float simpleDerivative = calculateSlope(minPointContinentalness, maxPointContinentalness, -1.0f, 1.0f);
            if (saddle) {
                build.addPoint(-1.0f, std::max(0.2f, minPointContinentalness));
                build.addPoint(0.0f, lerp(0.5f, minPointContinentalness, maxPointContinentalness), simpleDerivative);
            } else {
                build.addPoint(-1.0f, minPointContinentalness, simpleDerivative);
            }
            build.addPoint(1.0f, maxPointContinentalness, simpleDerivative);
        }

        return build.build();
    }

    CubicSplinePtr buildErosionOffsetSpline(
        BoundedFloatFunctionPtr erosion,
        BoundedFloatFunctionPtr ridges,
        float lowValley,
        float hill,
        float tallHill,
        float mountainFactor,
        float plain,
        float swamp,
        bool includeExtremeHills,
        bool saddle,
        Transform offsetTransformer
    ) {
        auto veryLowErosionMountains = buildMountainRidgeSplineWithPoints(ridges, lerp(mountainFactor, 0.6f, 1.5f), saddle, offsetTransformer);
        auto lowErosionMountains = buildMountainRidgeSplineWithPoints(ridges, lerp(mountainFactor, 0.6f, 1.0f), saddle, offsetTransformer);
        auto mountains = buildMountainRidgeSplineWithPoints(ridges, mountainFactor, saddle, offsetTransformer);
        auto widePlateau = ridgeSpline(ridges, lowValley - 0.15f, 0.5f * mountainFactor, 0.5f * mountainFactor, 0.5f * mountainFactor, 0.6f * mountainFactor, 0.5f, offsetTransformer);
        auto narrowPlateau = ridgeSpline(ridges, lowValley, plain * mountainFactor, hill * mountainFactor, 0.5f * mountainFactor, 0.6f * mountainFactor, 0.5f, offsetTransformer);
        auto plains = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5f, offsetTransformer);
        auto plainsFarInland = ridgeSpline(ridges, lowValley, plain, plain, hill, tallHill, 0.5f, offsetTransformer);
        auto extremeBuilder = CubicSpline::builder(ridges, offsetTransformer);
        extremeBuilder.addPoint(-1.0f, lowValley);
        extremeBuilder.addPoint(-0.4f, plains);
        extremeBuilder.addPoint(0.0f, tallHill + 0.07f);
        auto extremeHills = extremeBuilder.build();
        auto swamps = ridgeSpline(ridges, -0.02f, swamp, swamp, hill, tallHill, 0.0f, offsetTransformer);

        auto builder = CubicSpline::builder(std::move(erosion), std::move(offsetTransformer));
        builder.addPoint(-0.85f, veryLowErosionMountains);
        builder.addPoint(-0.7f, lowErosionMountains);
        builder.addPoint(-0.4f, mountains);
        builder.addPoint(-0.35f, widePlateau);
        builder.addPoint(-0.1f, narrowPlateau);
        builder.addPoint(0.2f, plains);
        if (includeExtremeHills) {
            builder.addPoint(0.4f, plainsFarInland);
            builder.addPoint(0.45f, extremeHills);
            builder.addPoint(0.55f, extremeHills);
            builder.addPoint(0.58f, plainsFarInland);
        }
        builder.addPoint(0.7f, swamps);
        return builder.build();
    }

    CubicSplinePtr buildWeirdnessJaggednessSpline(BoundedFloatFunctionPtr weirdness, float jaggednessFactor, Transform jaggednessTransformer) {
        float maxJaggednessAtNegativeWeirdness = 0.63f * jaggednessFactor;
        float maxJaggednessAtPositiveWeirdness = 0.3f * jaggednessFactor;
        auto builder = CubicSpline::builder(std::move(weirdness), std::move(jaggednessTransformer));
        builder.addPoint(-0.01f, maxJaggednessAtNegativeWeirdness);
        builder.addPoint(0.01f, maxJaggednessAtPositiveWeirdness);
        return builder.build();
    }

    CubicSplinePtr buildRidgeJaggednessSpline(
        BoundedFloatFunctionPtr weirdness,
        BoundedFloatFunctionPtr ridges,
        float jaggednessFactorAtPeakRidge,
        float jaggednessFactorAtHighRidge,
        Transform jaggednessTransformer
    ) {
        float highSliceStart = static_cast<float>(NoiseRouterData::peaksAndValleys(0.4f));
        float highSliceEnd = static_cast<float>(NoiseRouterData::peaksAndValleys(0.56666666f));
        float highSliceMiddle = (highSliceStart + highSliceEnd) / 2.0f;
        auto ridgeSplineBuilder = CubicSpline::builder(std::move(ridges), jaggednessTransformer);
        ridgeSplineBuilder.addPoint(highSliceStart, 0.0f);
        ridgeSplineBuilder.addPoint(
            highSliceMiddle,
            jaggednessFactorAtHighRidge > 0.0f ? buildWeirdnessJaggednessSpline(weirdness, jaggednessFactorAtHighRidge, jaggednessTransformer) : CubicSpline::constant(0.0f));
        ridgeSplineBuilder.addPoint(
            1.0f,
            jaggednessFactorAtPeakRidge > 0.0f ? buildWeirdnessJaggednessSpline(std::move(weirdness), jaggednessFactorAtPeakRidge, std::move(jaggednessTransformer)) : CubicSpline::constant(0.0f));
        return ridgeSplineBuilder.build();
    }

    CubicSplinePtr buildErosionJaggednessSpline(
        BoundedFloatFunctionPtr erosion,
        BoundedFloatFunctionPtr weirdness,
        BoundedFloatFunctionPtr ridges,
        float jaggednessFactorAtPeakRidgeAndErosionIndex0,
        float jaggednessFactorAtPeakRidgeAndErosionIndex1,
        float jaggednessFactorAtHighRidgeAndErosionIndex0,
        float jaggednessFactorAtHighRidgeAndErosionIndex1,
        Transform jaggednessTransformer
    ) {
        auto ridgeJaggednessSplineAtErosion0 = buildRidgeJaggednessSpline(weirdness, ridges, jaggednessFactorAtPeakRidgeAndErosionIndex0, jaggednessFactorAtHighRidgeAndErosionIndex0, jaggednessTransformer);
        auto ridgeJaggednessSplineAtErosion1 = buildRidgeJaggednessSpline(weirdness, ridges, jaggednessFactorAtPeakRidgeAndErosionIndex1, jaggednessFactorAtHighRidgeAndErosionIndex1, jaggednessTransformer);
        auto builder = CubicSpline::builder(std::move(erosion), std::move(jaggednessTransformer));
        builder.addPoint(-1.0f, ridgeJaggednessSplineAtErosion0);
        builder.addPoint(-0.78f, ridgeJaggednessSplineAtErosion1);
        builder.addPoint(-0.5775f, ridgeJaggednessSplineAtErosion1);
        builder.addPoint(-0.375f, 0.0f);
        return builder.build();
    }

    CubicSplinePtr getErosionFactor(
        BoundedFloatFunctionPtr erosion,
        BoundedFloatFunctionPtr weirdness,
        BoundedFloatFunctionPtr ridges,
        float baseValue,
        bool shatteredTerrain,
        Transform factorTransformer
    ) {
        auto baseBuilder = CubicSpline::builder(weirdness, factorTransformer);
        baseBuilder.addPoint(-0.2f, 6.3f);
        baseBuilder.addPoint(0.2f, baseValue);
        auto baseSpline = baseBuilder.build();

        auto midBuilder = CubicSpline::builder(weirdness, factorTransformer);
        midBuilder.addPoint(-0.05f, 6.3f);
        midBuilder.addPoint(0.05f, 2.67f);

        auto invBuilder = CubicSpline::builder(weirdness, factorTransformer);
        invBuilder.addPoint(-0.05f, 2.67f);
        invBuilder.addPoint(0.05f, 6.3f);

        auto erosionPoints = CubicSpline::builder(std::move(erosion), factorTransformer);
        erosionPoints.addPoint(-0.6f, baseSpline);
        erosionPoints.addPoint(-0.5f, midBuilder.build());
        erosionPoints.addPoint(-0.35f, baseSpline);
        erosionPoints.addPoint(-0.25f, baseSpline);
        erosionPoints.addPoint(-0.1f, invBuilder.build());
        erosionPoints.addPoint(0.03f, baseSpline);

        if (shatteredTerrain) {
            auto weirdnessShatteredBuilder = CubicSpline::builder(weirdness, factorTransformer);
            weirdnessShatteredBuilder.addPoint(0.0f, baseValue);
            weirdnessShatteredBuilder.addPoint(0.1f, 0.625f);
            auto ridgesShatteredBuilder = CubicSpline::builder(std::move(ridges), factorTransformer);
            ridgesShatteredBuilder.addPoint(-0.9f, baseValue);
            ridgesShatteredBuilder.addPoint(-0.69f, weirdnessShatteredBuilder.build());
            auto ridgesShattered = ridgesShatteredBuilder.build();
            erosionPoints.addPoint(0.35f, baseValue);
            erosionPoints.addPoint(0.45f, ridgesShattered);
            erosionPoints.addPoint(0.55f, ridgesShattered);
            erosionPoints.addPoint(0.62f, baseValue);
        } else {
            auto extremeHillsBuilder = CubicSpline::builder(ridges, factorTransformer);
            extremeHillsBuilder.addPoint(-0.7f, baseSpline);
            extremeHillsBuilder.addPoint(-0.15f, 1.37f);
            auto extremeHillsTerrainFromMidSliceAndUp = extremeHillsBuilder.build();

            auto extraNoiseBuilder = CubicSpline::builder(std::move(ridges), factorTransformer);
            extraNoiseBuilder.addPoint(0.45f, baseSpline);
            extraNoiseBuilder.addPoint(0.7f, 1.56f);
            auto extra3dNoiseOnPeaksOnly = extraNoiseBuilder.build();

            erosionPoints.addPoint(0.05f, extra3dNoiseOnPeaksOnly);
            erosionPoints.addPoint(0.4f, extra3dNoiseOnPeaksOnly);
            erosionPoints.addPoint(0.45f, extremeHillsTerrainFromMidSliceAndUp);
            erosionPoints.addPoint(0.55f, extremeHillsTerrainFromMidSliceAndUp);
            erosionPoints.addPoint(0.58f, baseValue);
        }

        return erosionPoints.build();
    }
}

CubicSplinePtr overworldOffset(BoundedFloatFunctionPtr continents, BoundedFloatFunctionPtr erosion, BoundedFloatFunctionPtr ridges, bool amplified) {
    Transform offsetTransformer = amplified ? Transform(amplifiedOffset) : Transform(noTransform);
    auto beachSpline = buildErosionOffsetSpline(erosion, ridges, -0.15f, 0.0f, 0.0f, 0.1f, 0.0f, -0.03f, false, false, offsetTransformer);
    auto lowSpline = buildErosionOffsetSpline(erosion, ridges, -0.1f, 0.03f, 0.1f, 0.1f, 0.01f, -0.03f, false, false, offsetTransformer);
    auto midSpline = buildErosionOffsetSpline(erosion, ridges, -0.1f, 0.03f, 0.1f, 0.7f, 0.01f, -0.03f, true, true, offsetTransformer);
    auto highSpline = buildErosionOffsetSpline(erosion, ridges, -0.05f, 0.03f, 0.1f, 1.0f, 0.01f, 0.01f, true, true, offsetTransformer);
    auto builder = CubicSpline::builder(std::move(continents), offsetTransformer);
    builder.addPoint(-1.1f, 0.044f);
    builder.addPoint(-1.02f, -0.2222f);
    builder.addPoint(-0.51f, -0.2222f);
    builder.addPoint(-0.44f, -0.12f);
    builder.addPoint(-0.18f, -0.12f);
    builder.addPoint(-0.16f, beachSpline);
    builder.addPoint(-0.15f, beachSpline);
    builder.addPoint(-0.1f, lowSpline);
    builder.addPoint(0.25f, midSpline);
    builder.addPoint(1.0f, highSpline);
    return builder.build();
}

CubicSplinePtr overworldFactor(
    BoundedFloatFunctionPtr continents,
    BoundedFloatFunctionPtr erosion,
    BoundedFloatFunctionPtr weirdness,
    BoundedFloatFunctionPtr ridges,
    bool amplified
) {
    Transform factorTransformer = amplified ? Transform(amplifiedFactor) : Transform(noTransform);
    auto builder = CubicSpline::builder(std::move(continents), noTransform);
    builder.addPoint(-0.19f, 3.95f);
    builder.addPoint(-0.15f, getErosionFactor(erosion, weirdness, ridges, 6.25f, true, noTransform));
    builder.addPoint(-0.1f, getErosionFactor(erosion, weirdness, ridges, 5.47f, true, factorTransformer));
    builder.addPoint(0.03f, getErosionFactor(erosion, weirdness, ridges, 5.08f, true, factorTransformer));
    builder.addPoint(0.06f, getErosionFactor(std::move(erosion), std::move(weirdness), std::move(ridges), 4.69f, false, factorTransformer));
    return builder.build();
}

CubicSplinePtr overworldJaggedness(
    BoundedFloatFunctionPtr continents,
    BoundedFloatFunctionPtr erosion,
    BoundedFloatFunctionPtr weirdness,
    BoundedFloatFunctionPtr ridges,
    bool amplified
) {
    Transform jaggednessTransformer = amplified ? Transform(amplifiedJaggedness) : Transform(noTransform);
    auto builder = CubicSpline::builder(std::move(continents), jaggednessTransformer);
    builder.addPoint(-0.11f, 0.0f);
    builder.addPoint(0.03f, buildErosionJaggednessSpline(erosion, weirdness, ridges, 1.0f, 0.5f, 0.0f, 0.0f, jaggednessTransformer));
    builder.addPoint(0.65f, buildErosionJaggednessSpline(std::move(erosion), std::move(weirdness), std::move(ridges), 1.0f, 1.0f, 1.0f, 0.0f, jaggednessTransformer));
    return builder.build();
}

} // namespace mc::levelgen::TerrainProvider
