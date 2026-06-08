#include "Minecraft.h"

#include "../core/Log.h"
#include "../world/entity/Entities.h"
#include "../world/level/block/BlockTags.h"
#include "../world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "../world/level/levelgen/feature/BiomeFeatures.h"

namespace mc {

void Minecraft::startLocalGameFast(uint64_t seed) {
    MC_LOG_INFO("Starting local singleplayer world asynchronously, seed={}", seed);

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

    // Keep one main-thread generator for biome/height queries. Terrain chunks are
    // still produced by the existing async chunk queue, using the same generator
    // algorithm and block writes. This removes the blocking 5x5 pregen from the
    // click-to-ingame path without changing final terrain output.
    m_localGenerator = std::make_unique<levelgen::NoiseBasedChunkGenerator>(seed);

    PlayerState& state = m_localPlayer.state();
    state.entityId = 0;
    state.gameMode = 1;
    state.x = 0.5;
    state.z = 0.5;
    state.y = (double)m_localGenerator->getBaseHeight(0, 0) + 4.0;
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
