#include "Minecraft.h"

#include "../core/Log.h"
#include "../world/entity/Entities.h"
#include "../world/level/block/BlockTags.h"
#include "../world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "../world/level/levelgen/feature/BiomeDecorator.h"
#include "../world/level/levelgen/feature/BiomeFeatures.h"

namespace mc {

namespace {
    ChunkPos worldToChunk(int wx, int wz) {
        return { wx >= 0 ? wx / 16 : (wx - 15) / 16,
                 wz >= 0 ? wz / 16 : (wz - 15) / 16 };
    }
}

void Minecraft::startLocalGameFast(uint64_t seed, int spawnX, int spawnZ, std::optional<int> spawnY) {
    MC_LOG_INFO("Starting local singleplayer world asynchronously, seed={}, spawn=({}, {}, {})",
                seed, spawnX, spawnY ? std::to_string(*spawnY) : std::string("surface"), spawnZ);

    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }

    clearEntities();
    m_playerInfo.clear();
    // No lock needed — decoration workers haven't started yet for this world
    // (the old queue was cleared and the new ensureEngineDecoration hasn't
    // been called yet at this point).
    m_chunks.clear();
    m_chunkCache.clear();
    m_chunkCacheOrder.clear();
    m_decorationQueue.clear();
    m_decorationDone.clear();
    m_generationTasks.clear();
    m_queuedChunks.clear();
    m_localPlayer.reset();

    m_worldSeed = seed;
    m_worldgenReady = false;
    m_worldgenTried = false;
    m_biomeFeatures.reset();
    m_blockTags.reset();
    m_worldgenDir.clear();
    m_dataMinecraftDir.clear();
    m_haveLastStreamPlayerPos = false;
    m_lastLocalMovement = std::chrono::steady_clock::now();

    // Keep one main-thread generator for biome/height queries. Terrain chunks are
    // still produced by the existing async chunk queue, using the same generator
    // algorithm and block writes. This removes the blocking 5x5 pregen from the
    // click-to-ingame path without changing final terrain output.
    m_localGenerator = std::make_unique<levelgen::NoiseBasedChunkGenerator>(seed);

    // Defer ALL worldgen data init to background threads. The gen threads only
    // need block_states.json (already loaded in initBlocks) and the
    // NoiseBasedChunkGenerator (just created). Chunks can generate while the
    // worldgen data extracts and the decoration context loads in parallel.
    m_worldgenReady = false;
    m_worldgenTried = false;
    m_biomeFeatures.reset();
    m_blockTags.reset();
    m_worldgenDir.clear();
    m_dataMinecraftDir.clear();
    m_haveLastStreamPlayerPos = false;
    m_lastLocalMovement = std::chrono::steady_clock::now();

    // Run ensureWorldgenData + decoration context init in a SINGLE background
    // thread (ensureWorldgenData sets m_dataMinecraftDir which the deco context
    // needs). This takes ~350ms total but runs entirely off the main thread.
    levelgen::feature::resetEngineDecoration();
    std::thread worldgenInitThread([this]() {
        ensureWorldgenData();
        levelgen::feature::ensureEngineDecoration(m_dataMinecraftDir, m_worldSeed, &m_chunks);
    });
    worldgenInitThread.detach();

    PlayerState& state = m_localPlayer.state();
    state.entityId = 0;
    state.gameMode = 1;
    state.x = (double)spawnX + 0.5;
    state.z = (double)spawnZ + 0.5;
    state.y = spawnY ? (double)*spawnY : (double)m_localGenerator->getBaseHeight(spawnX, spawnZ) + 4.0;
    state.yaw = 0.0f;
    state.pitch = 10.0f;
    state.onGround = false;
    state.horizontalCollision = false;

    m_inGame = true;
    setScreen(nullptr);

    // Submit ALL render-distance chunks to the gen pool immediately so they
    // generate in parallel. The previous code only called updateLocalChunks()
    // once, which submitted at most 8-32 chunks per tick. At 20 TPS, that's
    // 160-640 chunks/sec — but the player has to wait several ticks before
    // enough chunks are generated to see the world.
    //
    // By submitting ALL chunks at once, all gen threads start working
    // immediately. On a 4-core machine with 3 gen threads, 169 chunks take
    // 169 × 40ms / 3 = 2.25s. On a 16-core R9 9950x with 15 threads, 169
    // chunks take 169 × 40ms / 15 = 0.45s — well under 1 second.
    //
    // We also call updateLocalChunks() to start integrating any that finish
    // before the first frame.
    constexpr int FAST_START_RADIUS = 2;
    const ChunkPos spawnChunk = worldToChunk(spawnX, spawnZ);
    int submitted = 0;
    for (int dz = -FAST_START_RADIUS; dz <= FAST_START_RADIUS; ++dz) {
        for (int dx = -FAST_START_RADIUS; dx <= FAST_START_RADIUS; ++dx) {
            ChunkPos cp{spawnChunk.x + dx, spawnChunk.z + dz};
            auto key = chunkKey(cp);
            if (m_chunks.find(key) != m_chunks.end()) continue;
            if (m_queuedChunks.find(key) != m_queuedChunks.end()) continue;
            if (!m_threadPool) break;

            m_queuedChunks.insert(key);
            auto beard = std::make_shared<levelgen::Beardifier>(buildChunkBeardifier(cp));
            m_generationTasks.push_back({
                cp,
                m_threadPool->enqueue([pos = cp, seed = m_worldSeed, beard]() -> GeneratedChunk {
                    GeneratedChunk out;
                    out.chunk = std::make_unique<LevelChunk>(pos);
                    struct ThreadGenCache {
                        uint64_t seed = 0;
                        std::unique_ptr<levelgen::NoiseBasedChunkGenerator> gen;
                    };
                    thread_local ThreadGenCache cache;
                    if (!cache.gen || cache.seed != seed) {
                        cache.seed = seed;
                        cache.gen = std::make_unique<levelgen::NoiseBasedChunkGenerator>(seed);
                    }
                    cache.gen->fillFromNoise(*out.chunk, &out.genMarks,
                                            beard->isEmpty() ? nullptr : beard.get());
                    cache.gen->buildSurface(*out.chunk);
                    cache.gen->applyCarvers(*out.chunk, &out.genMarks);
                    return out;
                })
            });
            ++submitted;
        }
    }
    MC_LOG_INFO("Fast start: submitted {} chunks to gen pool", submitted);

    // Also kick the decoration queue for any chunks that are already loaded
    // (none yet, but updateLocalChunks will integrate gen results and submit
    // decoration requests).
    updateLocalChunks();
}

} // namespace mc
