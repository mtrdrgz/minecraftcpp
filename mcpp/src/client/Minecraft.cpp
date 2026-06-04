#include "Minecraft.h"
#include "../core/Log.h"
#include "../network/protocol/handshake/C2SHandshakePacket.h"
#include "../network/protocol/login/LoginPackets.h"
#include "../network/protocol/game/PlayPackets.h"
#include "../world/entity/Entities.h"
#include "../world/entity/player/Player.h"
#include "../world/level/levelgen/NoiseBasedChunkGenerator.h"
#include "../world/level/levelgen/feature/BiomeDecorator.h"
#include "../world/level/levelgen/feature/BiomeFeatures.h"
#include "../world/level/levelgen/structure/StructureGen.h"
#include "../world/level/block/BlockTags.h"
#include "../assets/AssetManager.h"
#include "../gui/screens/TitleScreen.h"
#include <stb_image.h>
#include <cmath>
#include <filesystem>
#include <exception>

namespace mc {

namespace {
    render::ITexture* loadAssetTex(render::IRenderDevice* dev, render::ICommandList* cmd, std::string_view path) {
        auto b = AssetManager::instance().readRaw(path);
        if (b.empty()) {
            MC_LOG_WARN("Asset not found: {}", path);
            return nullptr;
        }
        int w, h, ch;
        stbi_set_flip_vertically_on_load(false);
        uint8_t* p = stbi_load_from_memory(b.data(), (int)b.size(), &w, &h, &ch, 4);
        if (!p) return nullptr;
        render::TextureDesc d;
        d.width = w; d.height = h; d.format = render::TextureFormat::RGBA8;
        d.filter = render::FilterMode::Nearest;
        render::ITexture* t = dev->createTexture(d);
        cmd->uploadTexture(t, p);
        stbi_image_free(p);
        return t;
    }
}

Minecraft::Minecraft(Window* window, render::IRenderDevice* device)
    : m_window(window), m_device(device) 
{
    m_guiGraphics = std::make_unique<render::GuiGraphics>(device);
    m_gui = std::make_unique<gui::Gui>(this);
    m_soundManager = std::make_unique<audio::SoundManager>();
    m_soundManager->init();
    // Do NOT force inGame to true

    unsigned int numThreads = std::thread::hardware_concurrency();
    if (numThreads > 1) numThreads--; // Leave 1 thread for rendering
    else numThreads = 1;
    m_threadPool = std::make_unique<ThreadPool>(numThreads);
    MC_LOG_INFO("ThreadPool initialized with {} threads", numThreads);
    
    // Explicitly set the TitleScreen
    auto ts = std::make_unique<gui::screens::TitleScreen>();
    setScreen(std::move(ts));
}

Minecraft::~Minecraft() {
    disconnect();
    // Cancel or wait on any active generation tasks
    m_generationTasks.clear();
    m_threadPool.reset();
}

void Minecraft::setScreen(std::unique_ptr<gui::Screen> screen) {
    MC_LOG_INFO("setScreen called with {}", screen ? screen->title() : "nullptr");
    m_currentScreen = std::move(screen);
    if (m_currentScreen) {
        m_currentScreen->init(this, m_window->width(), m_window->height());
    }
}

// ── Chunk storage ─────────────────────────────────────────────────────────────
LevelChunk* Minecraft::getChunk(ChunkPos pos) {
    auto it = m_chunks.find(chunkKey(pos));
    return it == m_chunks.end() ? nullptr : it->second.get();
}
LevelChunk* Minecraft::getOrCreateChunk(ChunkPos pos) {
    auto key = chunkKey(pos);
    auto it  = m_chunks.find(key);
    if (it != m_chunks.end()) return it->second.get();
    auto chunk = std::make_unique<LevelChunk>(pos);
    LevelChunk* ptr = chunk.get();
    m_chunks[key] = std::move(chunk);
    return ptr;
}
void Minecraft::unloadChunk(ChunkPos pos) {
    m_chunks.erase(chunkKey(pos));
}

// ── Login sequence ─────────────────────────────────────────────────────────────
void Minecraft::connectToServer(std::string_view host, uint16_t port,
                                 std::string_view username)
{
    m_connection = std::make_unique<net::Connection>();
    if (!m_connection->connect(host, port)) {
        MC_LOG_ERROR("Failed to connect: {}", m_connection->lastError);
        m_connection.reset();
        return;
    }

    setScreen(nullptr);
    m_inGame = false;

    net::PacketBuffer handshake;
    net::C2SHandshakePacket hsPkt;
    hsPkt.serverAddress = std::string(host);
    hsPkt.serverPort    = port;
    hsPkt.nextState     = 2; // login
    net::encodePacket(hsPkt, handshake);
    m_connection->sendPacket(handshake);
    m_connection->state = net::ConnectionState::Login;

    net::PacketBuffer loginStart;
    net::C2SLoginStartPacket startPkt;
    startPkt.name = std::string(username);
    net::encodePacket(startPkt, loginStart);
    m_connection->sendPacket(loginStart);

    MC_LOG_INFO("Login sequence started for '{}'", username);
}

namespace {
    // Locate the local "26.1.2/data" tree (the data-driven worldgen JSON). The
    // exe may be launched from the repo root, mcpp/, or mcpp/build/, so probe a
    // few parents of both the working dir and the executable dir.
    std::string discoverDataRoot() {
        namespace fs = std::filesystem;
        auto probe = [](fs::path start) -> std::string {
            std::error_code ec;
            for (int i = 0; i < 6 && !start.empty(); ++i) {
                fs::path cand = start / "26.1.2" / "data";
                if (fs::exists(cand / "minecraft" / "worldgen" / "biome", ec))
                    return cand.generic_string();
                if (!start.has_parent_path()) break;
                start = start.parent_path();
            }
            return "";
        };
        std::error_code ec;
        if (auto r = probe(fs::current_path(ec)); !r.empty()) return r;
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            if (auto r = probe(fs::path(buf).parent_path()); !r.empty()) return r;
        }
        return "";
    }
}

void Minecraft::ensureWorldgenData() {
    if (m_worldgenTried) return;
    m_worldgenTried = true;

    std::string dataRoot = discoverDataRoot();
    if (dataRoot.empty()) {
        MC_LOG_WARN("Worldgen data (26.1.2/data) not found near cwd or exe; "
                    "terrain will generate without trees/vegetation");
        return;
    }

    try {
        m_biomeFeatures = std::make_unique<levelgen::feature::BiomeFeatures>(
            levelgen::feature::BiomeFeatures::loadFromDirectory(dataRoot + "/minecraft/worldgen/biome"));
        m_blockTags = std::make_unique<block::BlockTags>(
            block::BlockTags::loadFromDirectory(dataRoot + "/minecraft/tags/block"));
        m_worldgenDir = dataRoot + "/minecraft/worldgen";
        m_worldgenReady = true;
        MC_LOG_INFO("Worldgen decoration data loaded ({} biomes) from {}",
                    m_biomeFeatures->biomeCount(), dataRoot);
    } catch (const std::exception& e) {
        MC_LOG_WARN("Failed to load worldgen decoration data: {}", e.what());
        m_worldgenReady = false;
    }
}

void Minecraft::decorateChunk(LevelChunk& chunk) {
    if (!m_worldgenReady || !m_localGenerator) return;
    auto biomeGetter = [this](int x, int y, int z) {
        return m_localGenerator->getBiome(x, y, z);
    };
    // Let features write across this chunk's borders into already-loaded neighbours
    // (fixes trees clipped at chunk edges) — main-thread only, so getChunk is safe.
    auto chunkAt = [this](int cx, int cz) { return getChunk({ cx, cz }); };
    try {
        levelgen::feature::applyBiomeDecoration(
            chunk, (std::int64_t)m_worldSeed, biomeGetter,
            *m_biomeFeatures, *m_blockTags, m_worldgenDir, chunkAt);
    } catch (const std::exception& e) {
        MC_LOG_WARN("decorateChunk failed at ({},{}): {}", chunk.pos().x, chunk.pos().z, e.what());
    }
}

namespace {
    // Floor-divide world block coords to chunk coords (handles negatives).
    ChunkPos worldToChunk(int wx, int wz) {
        return { wx >= 0 ? wx / 16 : (wx - 15) / 16,
                 wz >= 0 ? wz / 16 : (wz - 15) / 16 };
    }
}

void Minecraft::runStructures(ChunkPos active) {
    if (!m_localGenerator) return;

    // Cross-chunk writer over the loaded chunks: a structure whose origin is in
    // `active` may write into adjacent loaded chunks (writes elsewhere are dropped).
    levelgen::structure::StructureWorld world;
    world.getBlock = [this](int x, int y, int z) -> uint32_t {
        LevelChunk* c = getChunk(worldToChunk(x, z));
        return c ? c->getBlock(x, y, z) : 0u;
    };
    world.setBlock = [this](int x, int y, int z, uint32_t id) {
        LevelChunk* c = getChunk(worldToChunk(x, z));
        if (c) { c->setBlock(x, y, z, id); c->meshDirty = true; }
    };
    world.heightAt = [this](int x, int z) -> int {
        LevelChunk* c = getChunk(worldToChunk(x, z));
        if (!c) return 0;
        return c->heightmap(((x % 16) + 16) % 16, ((z % 16) + 16) % 16);
    };
    auto biomeGetter = [this](int x, int y, int z) {
        return m_localGenerator->getBiome(x, y, z);
    };
    try {
        levelgen::structure::generateStructures(active, m_worldSeed, world, biomeGetter);
    } catch (const std::exception& e) {
        MC_LOG_WARN("runStructures failed at ({},{}): {}", active.x, active.z, e.what());
    }
}

void Minecraft::startLocalGame(uint64_t seed) {
    MC_LOG_INFO("Starting local singleplayer prototype world, seed={}", seed);

    if (m_connection) {
        m_connection->disconnect();
        m_connection.reset();
    }

    clearEntities();
    m_playerInfo.clear();
    m_chunks.clear();
    m_localPlayer.reset();

    m_worldSeed = seed;
    m_localGenerator = std::make_unique<levelgen::NoiseBasedChunkGenerator>(seed);
    ensureWorldgenData();
    constexpr int RADIUS = 1;

    for (int cz = -RADIUS; cz <= RADIUS; ++cz) {
        for (int cx = -RADIUS; cx <= RADIUS; ++cx) {
            LevelChunk* chunk = getOrCreateChunk({cx, cz});
            m_localGenerator->fillFromNoise(*chunk);
            m_localGenerator->buildSurface(*chunk);
        }
    }

    // Decoration (trees + vegetation) runs after all base terrain is in, on the
    // main thread (the feature caches in BiomeDecorator aren't thread-safe).
    for (int cz = -RADIUS; cz <= RADIUS; ++cz) {
        for (int cx = -RADIUS; cx <= RADIUS; ++cx) {
            decorateChunk(*getOrCreateChunk({cx, cz}));
        }
    }

    // Structures run after decoration so a building's footprint overwrites any
    // trees/grass placed on it. The cross-chunk writer spans the loaded spawn area.
    for (int cz = -RADIUS; cz <= RADIUS; ++cz) {
        for (int cx = -RADIUS; cx <= RADIUS; ++cx) {
            runStructures({cx, cz});
        }
    }


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

    MC_LOG_INFO("Local world ready: {} chunks generated", m_chunks.size());
}

void Minecraft::disconnect() {
    m_inGame = false;
    clearEntities();
    m_playerInfo.clear();
    m_localPlayer.reset();
    if (m_connection) m_connection->disconnect();
    m_connection.reset();
    MC_LOG_INFO("Disconnected");
}

void Minecraft::applyPlayerInfoToEntity(const PlayerInfo& info) {
    for (auto& [entityId, entity] : g_entities) {
        (void)entityId;
        if (!entity || entity->uuid != info.profileId || entity->type != EntityType::PLAYER) {
            continue;
        }
        static_cast<Player*>(entity.get())->profileName = info.name;
    }
}

void Minecraft::handlePackets() {
    if (!m_connection || !m_connection->isConnected()) return;

    net::PacketBuffer buf;
    int32_t id;
    while (m_connection->receivePacket(buf, id)) {
        if (m_connection->state == net::ConnectionState::Login) {
            if (id == 0x02) { // Login Success
                MC_LOG_INFO("Login successful!");
                m_connection->state = net::ConnectionState::Config;
                net::PacketBuffer ack;
                net::C2SConfigAcknowledgedPacket ackPkt;
                net::encodePacket(ackPkt, ack);
                m_connection->sendPacket(ack);
            }
        } else if (m_connection->state == net::ConnectionState::Config) {
            if (id == 0x03) { // Finish Configuration
                MC_LOG_INFO("Configuration finished, entering Play state");
                m_connection->state = net::ConnectionState::Play;
                net::PacketBuffer ack;
                net::C2SFinishConfigurationPacket ackPkt;
                net::encodePacket(ackPkt, ack);
                m_connection->sendPacket(ack);
                m_inGame = true;
            }
        } else if (m_connection->state == net::ConnectionState::Play) {
            handlePlayPacket(id, buf);
        }
    }
}

void Minecraft::handlePlayPacket(int32_t id, net::PacketBuffer& buf) {
    switch (id) {
    case net::S2CLoginPlayPacket::ID: {
        auto p = net::S2CLoginPlayPacket::read(buf);
        m_localPlayer.state().entityId = p.entityId;
        MC_LOG_INFO("Joined world as entity id={}", p.entityId);
        break;
    }
    case net::S2CChunkDataPacket::ID: {
        auto p = net::S2CChunkDataPacket::read(buf);
        MC_LOG_INFO("Received chunk data for {},{}", p.chunkX, p.chunkZ);
        if (LevelChunk* chunk = getOrCreateChunk({p.chunkX, p.chunkZ})) {
            p.populateChunk(*chunk);
            chunk->meshDirty = true;
            MC_LOG_INFO("Chunk populated and marked dirty");
            for (int dz = -1; dz <= 1; ++dz) {
                for (int dx = -1; dx <= 1; ++dx) {
                    if (dx == 0 && dz == 0) continue;
                    if (LevelChunk* n = getChunk({p.chunkX + dx, p.chunkZ + dz}))
                        n->meshDirty = true;
                }
            }
        }
        break;
    }
    case net::S2CAddEntityPacket::ID: {
        auto p = net::S2CAddEntityPacket::read(buf);
        Entity* entity = spawnEntity(p.entityId, p.type);
        entity->uuid = p.uuid;
        entity->setPosition(p.x, p.y, p.z);
        entity->velocity = {(float)p.velX, (float)p.velY, (float)p.velZ};
        entity->setRotation(p.yaw, p.pitch);
        entity->headYaw = p.headYaw;
        if (p.type == EntityType::PLAYER) {
            if (auto it = m_playerInfo.find(p.uuid); it != m_playerInfo.end()) {
                static_cast<Player*>(entity)->profileName = it->second.name;
            }
        }
        break;
    }
    case net::S2CAnimatePacket::ID: {
        auto p = net::S2CAnimatePacket::read(buf);
        if (Entity* entity = getEntity(p.entityId)) {
            if (p.action == 0 || p.action == 3) { 
                entity->swingTime = entity->swingTimeMax;
            }
        }
        break;
    }
    case net::S2CSetEntityMotionPacket::ID: {
        auto p = net::S2CSetEntityMotionPacket::read(buf);
        if (Entity* entity = getEntity(p.entityId)) {
            entity->velocity = {(float)p.velX, (float)p.velY, (float)p.velZ};
        }
        break;
    }
    case net::S2CSetEntityDataPacket::ID: {
        auto p = net::S2CSetEntityDataPacket::read(buf);
        if (Entity* entity = getEntity(p.entityId)) {
            for (auto& [id, meta] : p.packedItems) {
                entity->metadata[id] = std::move(meta);
            }
        }
        break;
    }
    case net::S2CSetEquipmentPacket::ID: {
        auto p = net::S2CSetEquipmentPacket::read(buf);
        if (Entity* entity = getEntity(p.entityId)) {
            for (auto& [slot, stack] : p.slots) {
                entity->equipment[slot] = stack;
            }
        }
        break;
    }
    case net::S2CSoundPacket::ID: {
        auto p = net::S2CSoundPacket::read(buf);
        if (m_soundManager && p.soundId >= 0) {
            m_soundManager->play((uint32_t)p.soundId, p.source, p.x, p.y, p.z, p.volume, p.pitch);
        }
        break;
    }
    case net::S2CSoundEntityPacket::ID: {
        auto p = net::S2CSoundEntityPacket::read(buf);
        if (m_soundManager && p.soundId >= 0) {
            if (Entity* e = getEntity(p.entityId)) {
                m_soundManager->play((uint32_t)p.soundId, p.source, e->position.x, e->position.y, e->position.z, p.volume, p.pitch);
            }
        }
        break;
    }
    case net::S2CStopSoundPacket::ID: {
        auto p = net::S2CStopSoundPacket::read(buf);
        if (m_soundManager) {
            audio::SoundSource* src = (p.flags & 1) ? &p.source.value() : nullptr;
            ResourceLocation* name = (p.flags & 2) ? &p.soundName.value() : nullptr;
            m_soundManager->stop(name, src);
        }
        break;
    }
    case net::S2CTeleportEntityPacket::ID: {
        auto p = net::S2CTeleportEntityPacket::read(buf);
        if (Entity* entity = getEntity(p.entityId)) {
            entity->setPosition(p.x, p.y, p.z);
            entity->velocity = {(float)p.velX, (float)p.velY, (float)p.velZ};
            entity->setRotation(p.yaw, p.pitch);
            entity->onGround = p.onGround;
        }
        break;
    }
    case net::S2CPlayerPositionPacket::ID: {
        auto p = net::S2CPlayerPositionPacket::read(buf);
        m_localPlayer.applyServerPosition(p.x, p.y, p.z, p.yaw, p.pitch, p.relatives);
        m_pendingTeleportId = p.teleportId;
        break;
    }
    case net::S2CKeepAlivePacket::ID: {
        auto p = net::S2CKeepAlivePacket::read(buf);
        net::PacketBuffer resp;
        net::C2SKeepAlivePacket respPkt;
        respPkt.id = p.id;
        net::encodePacket(respPkt, resp);
        m_connection->sendPacket(resp);
        break;
    }
    case net::S2CPlayerInfoUpdatePacket::ID: {
        auto p = net::S2CPlayerInfoUpdatePacket::read(buf);
        for (const auto& entry : p.entries) {
            PlayerInfo* info = nullptr;
            auto it = m_playerInfo.find(entry.profileId);
            if (it == m_playerInfo.end()) {
                if (entry.hasProfile) {
                    info = &m_playerInfo[entry.profileId];
                    info->profileId = entry.profileId;
                    info->name = entry.profileName;
                }
            } else {
                info = &it->second;
            }
            if (!info) continue;
            if (p.hasAction(net::S2CPlayerInfoUpdatePacket::UPDATE_GAME_MODE))
                info->gameMode = entry.gameMode;
            if (p.hasAction(net::S2CPlayerInfoUpdatePacket::UPDATE_LATENCY))
                info->latency = entry.latency;
            if (p.hasAction(net::S2CPlayerInfoUpdatePacket::UPDATE_LISTED))
                info->listed = entry.listed;
            applyPlayerInfoToEntity(*info);
        }
        break;
    }
    case net::S2CPlayerInfoRemovePacket::ID: {
        auto p = net::S2CPlayerInfoRemovePacket::read(buf);
        for (const auto& uuid : p.profileIds) m_playerInfo.erase(uuid);
        break;
    }
    case net::S2CRemoveEntitiesPacket::ID: {
        auto p = net::S2CRemoveEntitiesPacket::read(buf);
        for (int32_t eid : p.entityIds) removeEntity(eid);
        break;
    }
    default:
        break;
    }
}

void Minecraft::tick() {
    handlePackets();

    if (m_inGame) {
        if (m_connection && m_connection->isConnected()
            && m_connection->state == net::ConnectionState::Play) {
            m_localPlayer.tick(m_connection.get(), m_window);

            for (auto& [id, entity] : g_entities) {
                if (entity) entity->tick();
            }
            
            if (m_soundManager) {
                const auto& s = m_localPlayer.state();
                audio::SoundEngine::instance().setListenerPosition(s.x, s.y, s.z, s.yaw, s.pitch);
            }
        } else if (!m_connection) {
            updateLocalChunks();
            
            if (m_soundManager) {
                const auto& s = m_localPlayer.state();
                audio::SoundEngine::instance().setListenerPosition(s.x, s.y, s.z, s.yaw, s.pitch);
            }
        }
    }
    ++m_tickCounter;
}

void Minecraft::render(float pt) {
    if (!m_guiGraphics) return;

    static bool guiInit = false;
    if (!guiInit) {
        auto* cmd = m_device->beginFrame(m_window->width(), m_window->height());
        
        // fontTex is typically null: the GUI/font textures live in client.jar, not
        // assets.bin (which only holds asset-index objects). Build the Font anyway so
        // font() is never null — Font tolerates a null texture (text just won't draw).
        render::ITexture* fontTex = loadAssetTex(m_device, cmd, "minecraft/textures/font/ascii.png");
        m_font = std::make_unique<render::Font>(m_device, fontTex);

        m_gui->setHotbarTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/hotbar.png"));
        m_gui->setSelectionTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/hotbar_selection.png"));
        m_gui->setCrosshairTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/crosshair.png"));
        m_gui->setHeartTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/heart/full.png"));
        m_gui->setFoodTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/food_full.png"));

        auto ts = std::make_unique<gui::screens::TitleScreen>();
        ts->setLogoTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/title/minecraft.png"));
        ts->setButtonTextures(
            loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/button.png"),
            loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/button_highlighted.png")
        );
        setScreen(std::move(ts));

        guiInit = true;
        m_device->endFrame();
    }
}

void Minecraft::updateLocalChunks() {
    int px = (int)std::floor(m_localPlayer.state().x / 16.0);
    int pz = (int)std::floor(m_localPlayer.state().z / 16.0);
    
    constexpr int RADIUS = 6;
    
    // 1. Unload chunks outside RADIUS
    std::vector<ChunkPos> toUnload;
    for (const auto& [key, chunk] : m_chunks) {
        if (chunk) {
            ChunkPos cp = chunk->pos();
            if (std::abs(cp.x - px) > RADIUS || std::abs(cp.z - pz) > RADIUS) {
                toUnload.push_back(cp);
            }
        }
    }
    for (auto cp : toUnload) {
        unloadChunk(cp);
    }

    // 2. Poll completed generation tasks
    for (auto it = m_generationTasks.begin(); it != m_generationTasks.end(); ) {
        if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
            std::unique_ptr<LevelChunk> chunk = it->future.get();
            if (chunk) {
                ChunkPos cp = chunk->pos();
                // Check if still within radius
                if (std::abs(cp.x - px) <= RADIUS && std::abs(cp.z - pz) <= RADIUS) {
                    auto key = chunkKey(cp);
                    if (m_chunks.find(key) == m_chunks.end()) {
                        LevelChunk* ptr = chunk.get();
                        // Decorate on the main thread (terrain+surface were built
                        // on the worker; the decoration feature caches aren't
                        // thread-safe, so trees/vegetation are added here).
                        decorateChunk(*ptr);
                        m_chunks[key] = std::move(chunk);
                        ptr->meshDirty = true;
                        // Structures after the chunk is registered, so the
                        // cross-chunk writer can resolve this chunk too.
                        runStructures(cp);
                        
                        // Mark neighboring chunks dirty so seams are updated
                        for (int dz = -1; dz <= 1; ++dz) {
                            for (int dx = -1; dx <= 1; ++dx) {
                                if (dx == 0 && dz == 0) continue;
                                if (LevelChunk* n = getChunk({ cp.x + dx, cp.z + dz })) {
                                    n->meshDirty = true;
                                }
                            }
                        }
                    }
                }
            }
            it = m_generationTasks.erase(it);
        } else {
            ++it;
        }
    }
    
    // 3. Load/generate chunks inside RADIUS
    struct ChunkCand {
        ChunkPos pos;
        int distSq;
    };
    std::vector<ChunkCand> candidates;
    for (int cz = -RADIUS; cz <= RADIUS; ++cz) {
        for (int cx = -RADIUS; cx <= RADIUS; ++cx) {
            ChunkPos posKey{ px + cx, pz + cz };
            
            // Check if already loaded
            if (getChunk(posKey)) {
                continue;
            }
            
            // Check if already in queue
            bool alreadyQueued = false;
            for (const auto& task : m_generationTasks) {
                if (task.pos.x == posKey.x && task.pos.z == posKey.z) {
                    alreadyQueued = true;
                    break;
                }
            }
            if (alreadyQueued) {
                continue;
            }
            
            candidates.push_back({ posKey, cx * cx + cz * cz });
        }
    }
    
    if (candidates.empty()) return;
    
    std::sort(candidates.begin(), candidates.end(), [](const ChunkCand& a, const ChunkCand& b) {
        return a.distSq < b.distSq;
    });
    
    int queued = 0;
    constexpr int MAX_QUEUE_PER_TICK = 4;
    for (const auto& cand : candidates) {
        if (!m_threadPool) break;
        
        m_generationTasks.push_back({
            cand.pos,
            m_threadPool->enqueue([pos = cand.pos, seed = m_worldSeed]() -> std::unique_ptr<LevelChunk> {
                auto chunk = std::make_unique<LevelChunk>(pos);
                levelgen::NoiseBasedChunkGenerator generator(seed);
                generator.fillFromNoise(*chunk);
                generator.buildSurface(*chunk);
                return chunk;
            })
        });
        
        if (++queued >= MAX_QUEUE_PER_TICK) {
            break;
        }
    }
}

} // namespace mc
