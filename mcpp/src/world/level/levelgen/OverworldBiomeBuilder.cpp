#include "OverworldBiomeBuilder.h"

#include <array>

namespace mc::levelgen {
namespace {

using Climate::Parameter;
using Entry = Climate::ParameterList<std::string>::Entry;

struct OverworldBiomeBuilder {
    Parameter fullRange = Parameter::span(-1.0f, 1.0f);
    std::array<Parameter, 5> temperatures = {
        Parameter::span(-1.0f, -0.45f), Parameter::span(-0.45f, -0.15f),
        Parameter::span(-0.15f, 0.2f), Parameter::span(0.2f, 0.55f),
        Parameter::span(0.55f, 1.0f)
    };
    std::array<Parameter, 5> humidities = {
        Parameter::span(-1.0f, -0.35f), Parameter::span(-0.35f, -0.1f),
        Parameter::span(-0.1f, 0.1f), Parameter::span(0.1f, 0.3f),
        Parameter::span(0.3f, 1.0f)
    };
    std::array<Parameter, 7> erosions = {
        Parameter::span(-1.0f, -0.78f), Parameter::span(-0.78f, -0.375f),
        Parameter::span(-0.375f, -0.2225f), Parameter::span(-0.2225f, 0.05f),
        Parameter::span(0.05f, 0.45f), Parameter::span(0.45f, 0.55f),
        Parameter::span(0.55f, 1.0f)
    };
    Parameter frozenRange = temperatures[0];
    Parameter unfrozenRange = Parameter::span(temperatures[1], temperatures[4]);
    Parameter mushroomFieldsContinentalness = Parameter::span(-1.2f, -1.05f);
    Parameter deepOceanContinentalness = Parameter::span(-1.05f, -0.455f);
    Parameter oceanContinentalness = Parameter::span(-0.455f, -0.19f);
    Parameter coastContinentalness = Parameter::span(-0.19f, -0.11f);
    Parameter inlandContinentalness = Parameter::span(-0.11f, 0.55f);
    Parameter nearInlandContinentalness = Parameter::span(-0.11f, 0.03f);
    Parameter midInlandContinentalness = Parameter::span(0.03f, 0.3f);
    Parameter farInlandContinentalness = Parameter::span(0.3f, 1.0f);

    const char* oceans[2][5] = {
        { "minecraft:deep_frozen_ocean", "minecraft:deep_cold_ocean", "minecraft:deep_ocean", "minecraft:deep_lukewarm_ocean", "minecraft:warm_ocean" },
        { "minecraft:frozen_ocean", "minecraft:cold_ocean", "minecraft:ocean", "minecraft:lukewarm_ocean", "minecraft:warm_ocean" }
    };
    const char* middleBiomes[5][5] = {
        { "minecraft:snowy_plains", "minecraft:snowy_plains", "minecraft:snowy_plains", "minecraft:snowy_taiga", "minecraft:taiga" },
        { "minecraft:plains", "minecraft:plains", "minecraft:forest", "minecraft:taiga", "minecraft:old_growth_spruce_taiga" },
        { "minecraft:flower_forest", "minecraft:plains", "minecraft:forest", "minecraft:birch_forest", "minecraft:dark_forest" },
        { "minecraft:savanna", "minecraft:savanna", "minecraft:forest", "minecraft:jungle", "minecraft:jungle" },
        { "minecraft:desert", "minecraft:desert", "minecraft:desert", "minecraft:desert", "minecraft:desert" }
    };
    const char* middleBiomesVariant[5][5] = {
        { "minecraft:ice_spikes", nullptr, "minecraft:snowy_taiga", nullptr, nullptr },
        { nullptr, nullptr, nullptr, nullptr, "minecraft:old_growth_pine_taiga" },
        { "minecraft:sunflower_plains", nullptr, nullptr, "minecraft:old_growth_birch_forest", nullptr },
        { nullptr, nullptr, "minecraft:plains", "minecraft:sparse_jungle", "minecraft:bamboo_jungle" },
        { nullptr, nullptr, nullptr, nullptr, nullptr }
    };
    const char* plateauBiomes[5][5] = {
        { "minecraft:snowy_plains", "minecraft:snowy_plains", "minecraft:snowy_plains", "minecraft:snowy_taiga", "minecraft:snowy_taiga" },
        { "minecraft:meadow", "minecraft:meadow", "minecraft:forest", "minecraft:taiga", "minecraft:old_growth_spruce_taiga" },
        { "minecraft:meadow", "minecraft:meadow", "minecraft:meadow", "minecraft:meadow", "minecraft:pale_garden" },
        { "minecraft:savanna_plateau", "minecraft:savanna_plateau", "minecraft:forest", "minecraft:forest", "minecraft:jungle" },
        { "minecraft:badlands", "minecraft:badlands", "minecraft:badlands", "minecraft:wooded_badlands", "minecraft:wooded_badlands" }
    };
    const char* plateauBiomesVariant[5][5] = {
        { "minecraft:ice_spikes", nullptr, nullptr, nullptr, nullptr },
        { "minecraft:cherry_grove", nullptr, "minecraft:meadow", "minecraft:meadow", "minecraft:old_growth_pine_taiga" },
        { "minecraft:cherry_grove", "minecraft:cherry_grove", "minecraft:forest", "minecraft:birch_forest", nullptr },
        { nullptr, nullptr, nullptr, nullptr, nullptr },
        { "minecraft:eroded_badlands", "minecraft:eroded_badlands", nullptr, nullptr, nullptr }
    };
    const char* shatteredBiomes[5][5] = {
        { "minecraft:windswept_gravelly_hills", "minecraft:windswept_gravelly_hills", "minecraft:windswept_hills", "minecraft:windswept_forest", "minecraft:windswept_forest" },
        { "minecraft:windswept_gravelly_hills", "minecraft:windswept_gravelly_hills", "minecraft:windswept_hills", "minecraft:windswept_forest", "minecraft:windswept_forest" },
        { "minecraft:windswept_hills", "minecraft:windswept_hills", "minecraft:windswept_hills", "minecraft:windswept_forest", "minecraft:windswept_forest" },
        { nullptr, nullptr, nullptr, nullptr, nullptr },
        { nullptr, nullptr, nullptr, nullptr, nullptr }
    };

    std::vector<Entry> biomes;

    void addSurfaceBiome(Parameter temperature, Parameter humidity, Parameter continentalness, Parameter erosion, Parameter weirdness, float offset, const char* biome) {
        biomes.push_back({ Climate::parameters(temperature, humidity, continentalness, erosion, Parameter::point(0.0f), weirdness, offset), biome });
        biomes.push_back({ Climate::parameters(temperature, humidity, continentalness, erosion, Parameter::point(1.0f), weirdness, offset), biome });
    }
    void addUndergroundBiome(Parameter temperature, Parameter humidity, Parameter continentalness, Parameter erosion, Parameter weirdness, float offset, const char* biome) {
        biomes.push_back({ Climate::parameters(temperature, humidity, continentalness, erosion, Parameter::span(0.2f, 0.9f), weirdness, offset), biome });
    }
    void addBottomBiome(Parameter temperature, Parameter humidity, Parameter continentalness, Parameter erosion, Parameter weirdness, float offset, const char* biome) {
        biomes.push_back({ Climate::parameters(temperature, humidity, continentalness, erosion, Parameter::point(1.1f), weirdness, offset), biome });
    }

    void addBiomes() {
        addOffCoastBiomes();
        addInlandBiomes();
        addUndergroundBiomes();
    }
    void addOffCoastBiomes() {
        addSurfaceBiome(fullRange, fullRange, mushroomFieldsContinentalness, fullRange, fullRange, 0.0f, "minecraft:mushroom_fields");
        for (int t = 0; t < 5; ++t) {
            addSurfaceBiome(temperatures[t], fullRange, deepOceanContinentalness, fullRange, fullRange, 0.0f, oceans[0][t]);
            addSurfaceBiome(temperatures[t], fullRange, oceanContinentalness, fullRange, fullRange, 0.0f, oceans[1][t]);
        }
    }
    void addInlandBiomes() {
        addMidSlice(Parameter::span(-1.0f, -0.93333334f));
        addHighSlice(Parameter::span(-0.93333334f, -0.7666667f));
        addPeaks(Parameter::span(-0.7666667f, -0.56666666f));
        addHighSlice(Parameter::span(-0.56666666f, -0.4f));
        addMidSlice(Parameter::span(-0.4f, -0.26666668f));
        addLowSlice(Parameter::span(-0.26666668f, -0.05f));
        addValleys(Parameter::span(-0.05f, 0.05f));
        addLowSlice(Parameter::span(0.05f, 0.26666668f));
        addMidSlice(Parameter::span(0.26666668f, 0.4f));
        addHighSlice(Parameter::span(0.4f, 0.56666666f));
        addPeaks(Parameter::span(0.56666666f, 0.7666667f));
        addHighSlice(Parameter::span(0.7666667f, 0.93333334f));
        addMidSlice(Parameter::span(0.93333334f, 1.0f));
    }
    void addPeaks(Parameter weirdness) {
        for (int t = 0; t < 5; ++t) for (int h = 0; h < 5; ++h) {
            const char* middle = pickMiddleBiome(t, h, weirdness);
            const char* middleHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            const char* middleHotSlopeCold = pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
            const char* plateau = pickPlateauBiome(t, h, weirdness);
            const char* shattered = pickShatteredBiome(t, h, weirdness);
            const char* shatteredSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, shattered);
            const char* peak = pickPeakBiome(t, h, weirdness);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, farInlandContinentalness), erosions[0], weirdness, 0.0f, peak);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, nearInlandContinentalness), erosions[1], weirdness, 0.0f, middleHotSlopeCold);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[1], weirdness, 0.0f, peak);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, nearInlandContinentalness), Parameter::span(erosions[2], erosions[3]), weirdness, 0.0f, middle);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[2], weirdness, 0.0f, plateau);
            addSurfaceBiome(temperatures[t], humidities[h], midInlandContinentalness, erosions[3], weirdness, 0.0f, middleHot);
            addSurfaceBiome(temperatures[t], humidities[h], farInlandContinentalness, erosions[3], weirdness, 0.0f, plateau);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, farInlandContinentalness), erosions[4], weirdness, 0.0f, middle);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, nearInlandContinentalness), erosions[5], weirdness, 0.0f, shatteredSavanna);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[5], weirdness, 0.0f, shattered);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, middle);
        }
    }
    void addHighSlice(Parameter weirdness) {
        for (int t = 0; t < 5; ++t) for (int h = 0; h < 5; ++h) {
            const char* middle = pickMiddleBiome(t, h, weirdness);
            const char* middleHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
            const char* middleHotSlopeCold = pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
            const char* plateau = pickPlateauBiome(t, h, weirdness);
            const char* shattered = pickShatteredBiome(t, h, weirdness);
            const char* middleSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middle);
            const char* slope = pickSlopeBiome(t, h, weirdness);
            const char* peak = pickPeakBiome(t, h, weirdness);
            addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, middle);
            addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, erosions[0], weirdness, 0.0f, slope);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[0], weirdness, 0.0f, peak);
            addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, erosions[1], weirdness, 0.0f, middleHotSlopeCold);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[1], weirdness, 0.0f, slope);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, nearInlandContinentalness), Parameter::span(erosions[2], erosions[3]), weirdness, 0.0f, middle);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[2], weirdness, 0.0f, plateau);
            addSurfaceBiome(temperatures[t], humidities[h], midInlandContinentalness, erosions[3], weirdness, 0.0f, middleHot);
            addSurfaceBiome(temperatures[t], humidities[h], farInlandContinentalness, erosions[3], weirdness, 0.0f, plateau);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, farInlandContinentalness), erosions[4], weirdness, 0.0f, middle);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, nearInlandContinentalness), erosions[5], weirdness, 0.0f, middleSavanna);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[5], weirdness, 0.0f, shattered);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, middle);
        }
    }
    void addMidSlice(Parameter weirdness);
    void addLowSlice(Parameter weirdness);
    void addValleys(Parameter weirdness);
    void addUndergroundBiomes() {
        addUndergroundBiome(fullRange, fullRange, Parameter::span(0.8f, 1.0f), fullRange, fullRange, 0.0f, "minecraft:dripstone_caves");
        addUndergroundBiome(fullRange, Parameter::span(0.7f, 1.0f), fullRange, fullRange, fullRange, 0.0f, "minecraft:lush_caves");
        addBottomBiome(fullRange, fullRange, fullRange, Parameter::span(erosions[0], erosions[1]), fullRange, 0.0f, "minecraft:deep_dark");
    }
    void addCommonSwamps(Parameter weirdness) {
        addSurfaceBiome(fullRange, fullRange, coastContinentalness, Parameter::span(erosions[0], erosions[2]), weirdness, 0.0f, "minecraft:stony_shore");
        addSurfaceBiome(Parameter::span(temperatures[1], temperatures[2]), fullRange, Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, "minecraft:swamp");
        addSurfaceBiome(Parameter::span(temperatures[3], temperatures[4]), fullRange, Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, "minecraft:mangrove_swamp");
    }
    const char* pickMiddleBiome(int t, int h, Parameter weirdness) const {
        if (weirdness.max < 0) return middleBiomes[t][h];
        const char* variant = middleBiomesVariant[t][h];
        return variant ? variant : middleBiomes[t][h];
    }
    const char* pickMiddleBiomeOrBadlandsIfHot(int t, int h, Parameter weirdness) const { return t == 4 ? pickBadlandsBiome(h, weirdness) : pickMiddleBiome(t, h, weirdness); }
    const char* pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(int t, int h, Parameter weirdness) const { return t == 0 ? pickSlopeBiome(t, h, weirdness) : pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness); }
    const char* maybePickWindsweptSavannaBiome(int t, int h, Parameter weirdness, const char* underlying) const { return t > 1 && h < 4 && weirdness.max >= 0 ? "minecraft:windswept_savanna" : underlying; }
    const char* pickShatteredCoastBiome(int t, int h, Parameter weirdness) const {
        const char* beachOrMiddle = weirdness.max >= 0 ? pickMiddleBiome(t, h, weirdness) : pickBeachBiome(t, h);
        return maybePickWindsweptSavannaBiome(t, h, weirdness, beachOrMiddle);
    }
    const char* pickBeachBiome(int t, int) const { return t == 0 ? "minecraft:snowy_beach" : (t == 4 ? "minecraft:desert" : "minecraft:beach"); }
    const char* pickBadlandsBiome(int h, Parameter weirdness) const {
        if (h < 2) return weirdness.max < 0 ? "minecraft:badlands" : "minecraft:eroded_badlands";
        return h < 3 ? "minecraft:badlands" : "minecraft:wooded_badlands";
    }
    const char* pickPlateauBiome(int t, int h, Parameter weirdness) const {
        if (weirdness.max >= 0) if (const char* variant = plateauBiomesVariant[t][h]) return variant;
        return plateauBiomes[t][h];
    }
    const char* pickPeakBiome(int t, int h, Parameter weirdness) const {
        if (t <= 2) return weirdness.max < 0 ? "minecraft:jagged_peaks" : "minecraft:frozen_peaks";
        return t == 3 ? "minecraft:stony_peaks" : pickBadlandsBiome(h, weirdness);
    }
    const char* pickSlopeBiome(int t, int h, Parameter weirdness) const { return t >= 3 ? pickPlateauBiome(t, h, weirdness) : (h <= 1 ? "minecraft:snowy_slopes" : "minecraft:grove"); }
    const char* pickShatteredBiome(int t, int h, Parameter weirdness) const { const char* biome = shatteredBiomes[t][h]; return biome ? biome : pickMiddleBiome(t, h, weirdness); }
};

void OverworldBiomeBuilder::addMidSlice(Parameter weirdness) {
    addCommonSwamps(weirdness);
    for (int t = 0; t < 5; ++t) for (int h = 0; h < 5; ++h) {
        const char* middle = pickMiddleBiome(t, h, weirdness);
        const char* middleHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
        const char* middleHotSlopeCold = pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
        const char* shattered = pickShatteredBiome(t, h, weirdness);
        const char* plateau = pickPlateauBiome(t, h, weirdness);
        const char* beach = pickBeachBiome(t, h);
        const char* middleSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middle);
        const char* shatteredCoast = pickShatteredCoastBiome(t, h, weirdness);
        const char* slope = pickSlopeBiome(t, h, weirdness);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[0], weirdness, 0.0f, slope);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(nearInlandContinentalness, midInlandContinentalness), erosions[1], weirdness, 0.0f, middleHotSlopeCold);
        addSurfaceBiome(temperatures[t], humidities[h], farInlandContinentalness, erosions[1], weirdness, 0.0f, t == 0 ? slope : plateau);
        addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, erosions[2], weirdness, 0.0f, middle);
        addSurfaceBiome(temperatures[t], humidities[h], midInlandContinentalness, erosions[2], weirdness, 0.0f, middleHot);
        addSurfaceBiome(temperatures[t], humidities[h], farInlandContinentalness, erosions[2], weirdness, 0.0f, plateau);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, nearInlandContinentalness), erosions[3], weirdness, 0.0f, middle);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[3], weirdness, 0.0f, middleHot);
        if (weirdness.max < 0) {
            addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, erosions[4], weirdness, 0.0f, beach);
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[4], weirdness, 0.0f, middle);
        } else {
            addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(coastContinentalness, farInlandContinentalness), erosions[4], weirdness, 0.0f, middle);
        }
        addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, erosions[5], weirdness, 0.0f, shatteredCoast);
        addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, erosions[5], weirdness, 0.0f, middleSavanna);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[5], weirdness, 0.0f, shattered);
        addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, erosions[6], weirdness, 0.0f, weirdness.max < 0 ? beach : middle);
        if (t == 0) addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, middle);
    }
}

void OverworldBiomeBuilder::addLowSlice(Parameter weirdness) {
    addCommonSwamps(weirdness);
    for (int t = 0; t < 5; ++t) for (int h = 0; h < 5; ++h) {
        const char* middle = pickMiddleBiome(t, h, weirdness);
        const char* middleHot = pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness);
        const char* middleHotSlopeCold = pickMiddleBiomeOrBadlandsIfHotOrSlopeIfCold(t, h, weirdness);
        const char* beach = pickBeachBiome(t, h);
        const char* middleSavanna = maybePickWindsweptSavannaBiome(t, h, weirdness, middle);
        const char* shatteredCoast = pickShatteredCoastBiome(t, h, weirdness);
        addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, middleHot);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, middleHotSlopeCold);
        addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, Parameter::span(erosions[2], erosions[3]), weirdness, 0.0f, middle);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), Parameter::span(erosions[2], erosions[3]), weirdness, 0.0f, middleHot);
        addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, Parameter::span(erosions[3], erosions[4]), weirdness, 0.0f, beach);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[4], weirdness, 0.0f, middle);
        addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, erosions[5], weirdness, 0.0f, shatteredCoast);
        addSurfaceBiome(temperatures[t], humidities[h], nearInlandContinentalness, erosions[5], weirdness, 0.0f, middleSavanna);
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), erosions[5], weirdness, 0.0f, middle);
        addSurfaceBiome(temperatures[t], humidities[h], coastContinentalness, erosions[6], weirdness, 0.0f, beach);
        if (t == 0) addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(nearInlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, middle);
    }
}

void OverworldBiomeBuilder::addValleys(Parameter weirdness) {
    addSurfaceBiome(frozenRange, fullRange, coastContinentalness, Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, weirdness.max < 0 ? "minecraft:stony_shore" : "minecraft:frozen_river");
    addSurfaceBiome(unfrozenRange, fullRange, coastContinentalness, Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, weirdness.max < 0 ? "minecraft:stony_shore" : "minecraft:river");
    addSurfaceBiome(frozenRange, fullRange, nearInlandContinentalness, Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, "minecraft:frozen_river");
    addSurfaceBiome(unfrozenRange, fullRange, nearInlandContinentalness, Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, "minecraft:river");
    addSurfaceBiome(frozenRange, fullRange, Parameter::span(coastContinentalness, farInlandContinentalness), Parameter::span(erosions[2], erosions[5]), weirdness, 0.0f, "minecraft:frozen_river");
    addSurfaceBiome(unfrozenRange, fullRange, Parameter::span(coastContinentalness, farInlandContinentalness), Parameter::span(erosions[2], erosions[5]), weirdness, 0.0f, "minecraft:river");
    addSurfaceBiome(frozenRange, fullRange, coastContinentalness, erosions[6], weirdness, 0.0f, "minecraft:frozen_river");
    addSurfaceBiome(unfrozenRange, fullRange, coastContinentalness, erosions[6], weirdness, 0.0f, "minecraft:river");
    addSurfaceBiome(Parameter::span(temperatures[1], temperatures[2]), fullRange, Parameter::span(inlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, "minecraft:swamp");
    addSurfaceBiome(Parameter::span(temperatures[3], temperatures[4]), fullRange, Parameter::span(inlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, "minecraft:mangrove_swamp");
    addSurfaceBiome(frozenRange, fullRange, Parameter::span(inlandContinentalness, farInlandContinentalness), erosions[6], weirdness, 0.0f, "minecraft:frozen_river");
    for (int t = 0; t < 5; ++t) for (int h = 0; h < 5; ++h) {
        addSurfaceBiome(temperatures[t], humidities[h], Parameter::span(midInlandContinentalness, farInlandContinentalness), Parameter::span(erosions[0], erosions[1]), weirdness, 0.0f, pickMiddleBiomeOrBadlandsIfHot(t, h, weirdness));
    }
}

} // namespace

std::vector<Entry> buildOverworldBiomePreset() {
    OverworldBiomeBuilder builder;
    builder.addBiomes();
    return std::move(builder.biomes);
}

std::vector<Entry> buildNetherBiomePreset() {
    return {
        { Climate::parameters(0.0f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),   "minecraft:nether_wastes" },
        { Climate::parameters(0.0f, -0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),   "minecraft:soul_sand_valley" },
        { Climate::parameters(0.4f,  0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f),   "minecraft:crimson_forest" },
        { Climate::parameters(0.0f,  0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.375f), "minecraft:warped_forest" },
        { Climate::parameters(-0.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.175f), "minecraft:basalt_deltas" }
    };
}

} // namespace mc::levelgen
