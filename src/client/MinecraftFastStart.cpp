#include "Minecraft.h"

#include "../core/Log.h"
#include "../world/entity/Entities.h"
#include "../world/level/block/BlockTags.h"
#include "../world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "../world/level/levelgen/feature/BiomeDecorator.h"
#include "../world/level/levelgen/feature/BiomeFeatures.h"

namespace mc {

void Minecraft::startLocalGameFast(uint64_t seed, int spawnX, int spawnZ, std::optional<int> spawnY) {
    MC_LOG_INFO("Starting local singleplayer world asynchronously, seed={}, spawn=({}, {}, {})",
                seed, spawnX, spawnY ? std::to_string(*spawnY) : std::string("surface"), spawnZ);

    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }

    clearEntities();
    m_playerInfo.clear();
    m_chunks.clear();
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

    // Decoration context (certified machinery; loads worldgen data from disk).
    // Created up front so the streaming path can freeze each generated chunk's
    // WG heightmaps at integration time — the chunk map was just cleared, so the
    // old context (if any) must go with it.
    ensureWorldgenData();
    levelgen::feature::resetEngineDecoration();
    levelgen::feature::ensureEngineDecoration(m_dataMinecraftDir, m_worldSeed, &m_chunks);

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

    // Kick the queue once immediately. The first frame can render while workers
    // fill the nearby chunks instead of blocking here on 25 synchronous chunks.
    updateLocalChunks();
}

} // namespace mc
