#include "BiomeDecorator.h"

#include "EngineDecoration.h"
#include "../../../../core/Log.h"

#include <utility>

namespace mc::levelgen::feature {

namespace {
JsonAssetReader& jsonAssetReader() {
    static JsonAssetReader reader;
    return reader;
}

EngineDecorationContext* g_ctx = nullptr;
bool g_ctxFailed = false;                 // creation failed: stay silent after the first log
std::uint64_t g_ctxSeed = 0;
std::unordered_map<std::int64_t, std::unique_ptr<LevelChunk>>* g_ctxChunks = nullptr;
long long g_decoratedChunks = 0;
} // namespace

void setJsonAssetReader(JsonAssetReader reader) {
    jsonAssetReader() = std::move(reader);
}

bool ensureEngineDecoration(const std::string& dataMinecraftDir, std::uint64_t worldSeed,
                            std::unordered_map<std::int64_t, std::unique_ptr<LevelChunk>>* chunks) {
    if (g_ctx && g_ctxSeed == worldSeed && g_ctxChunks == chunks) return true;
    resetEngineDecoration();
    if (dataMinecraftDir.empty()) {
        MC_LOG_WARN("Decoration disabled: no worldgen data source (assets.bin has no embedded "
                    "data/minecraft entries and 26.1.2/data was not found on disk — rebuild "
                    "assets.bin, or run from the repo root so 26.1.2/data resolves)");
        g_ctxFailed = true;
        return false;
    }
    MC_LOG_INFO("Creating decoration context: seed={} data={}", worldSeed, dataMinecraftDir);
    g_ctx = engineDecorationCreate(dataMinecraftDir, worldSeed, chunks);
    if (!g_ctx) {
        MC_LOG_WARN("Decoration disabled: context creation failed (see [ERR] line above)");
        g_ctxFailed = true;
        return false;
    }
    g_ctxSeed = worldSeed;
    g_ctxChunks = chunks;
    return true;
}

void resetEngineDecoration() {
    if (g_ctx) engineDecorationDestroy(g_ctx);
    g_ctx = nullptr;
    g_ctxFailed = false;
    g_ctxChunks = nullptr;
    g_decoratedChunks = 0;
}

void freezeWorldgenHeights(LevelChunk& chunk, const std::vector<BlockPos>* genMarks) {
    if (!g_ctx) return;
    engineFreezeWgHeights(g_ctx, &chunk, chunk.pos().x, chunk.pos().z, genMarks);
}

void applyBiomeDecoration(LevelChunk& chunk) {
    if (!g_ctx) return;
    engineDecorateChunk(g_ctx, chunk.pos().x, chunk.pos().z);
    if (++g_decoratedChunks % 100 == 0)
        MC_LOG_INFO("Decorated {} chunks", g_decoratedChunks);
}

void beginFeatureTurn(LevelChunk& chunk) {
    if (!g_ctx) return;
    engineBeginFeatureTurn(g_ctx, chunk.pos().x, chunk.pos().z);
}

bool placeStructurePoolFeature(const std::string& featureId, mc::levelgen::RandomSource& random,
                               BlockPos origin, ChunkPos decoratingChunk) {
    if (!g_ctx) return false;
    return enginePlaceStructurePoolFeature(g_ctx, featureId, random, origin,
                                           decoratingChunk.x, decoratingChunk.z);
}

} // namespace mc::levelgen::feature
