#include "Minecraft.h"
#include "../core/Log.h"
#include "../../profiling/include/Profiler.h"
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
#include "../gui/screens/PauseScreen.h"
#include "../gui/screens/options/OptionsScreen.h"
#include "../render/gui/PanoramaRenderer.h"
#include "../assets/resource_ids.h"
#include <stb_image.h>
#include <windows.h>
#include <cmath>
#include <ctime>
#include <iomanip>
#include <filesystem>
#include <fstream>
#include <exception>
#include <optional>
#include <sstream>
#include <random>
#include <chrono>
#include <vector>
#include <string>

namespace mc {

namespace {
    // Decode a 4-channel texture from already-read PNG bytes.
    render::ITexture* decodeTex(render::IRenderDevice* dev, render::ICommandList* cmd, const uint8_t* bytes, int len) {
        if (!bytes || len <= 0) return nullptr;
        int w, h, ch;
        stbi_set_flip_vertically_on_load(false);
        uint8_t* p = stbi_load_from_memory(bytes, len, &w, &h, &ch, 4);
        if (!p) return nullptr;
        render::TextureDesc d;
        d.width = (uint32_t)w; d.height = (uint32_t)h; d.format = render::TextureFormat::RGBA8;
        d.filter = render::FilterMode::Nearest;
        render::ITexture* t = dev->createTexture(d);
        cmd->uploadTexture(t, p);
        stbi_image_free(p);
        return t;
    }

    render::ITexture* loadAssetTex(render::IRenderDevice* dev, render::ICommandList* cmd, std::string_view path) {
        auto b = AssetManager::instance().readRaw(path);
        if (b.empty()) {
            MC_LOG_WARN("Asset not found: {}", path);
            return nullptr;
        }
        return decodeTex(dev, cmd, b.data(), (int)b.size());
    }

    // Load a PNG embedded as an RCDATA resource (font / GUI textures from client.jar).
    render::ITexture* loadResourceTex(render::IRenderDevice* dev, render::ICommandList* cmd, int resourceId) {
        HMODULE hmod = GetModuleHandleW(nullptr);
        HRSRC hres = FindResourceW(hmod, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (!hres) return nullptr;
        HGLOBAL hg = LoadResource(hmod, hres);
        const uint8_t* data = static_cast<const uint8_t*>(LockResource(hg));
        DWORD size = SizeofResource(hmod, hres);
        return decodeTex(dev, cmd, data, (int)size);
    }

    // Read a text resource (e.g. splashes.txt) embedded as RCDATA.
    std::string loadResourceText(int resourceId) {
        HMODULE hmod = GetModuleHandleW(nullptr);
        HRSRC hres = FindResourceW(hmod, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (!hres) return {};
        HGLOBAL hg = LoadResource(hmod, hres);
        const char* data = static_cast<const char*>(LockResource(hg));
        DWORD size = SizeofResource(hmod, hres);
        return data ? std::string(data, data + size) : std::string{};
    }

    // SplashManager: pick a random non-empty line from splashes.txt.
    std::string pickSplash() {
        const std::string txt = loadResourceText(IDR_SPLASHES);
        std::vector<std::string> lines;
        std::stringstream ss(txt);
        std::string line;
        while (std::getline(ss, line)) {
            while (!line.empty() && (line.back() == '\r' || line.back() == ' ' || line.back() == '\t')) line.pop_back();
            if (!line.empty()) lines.push_back(line);
        }
        if (lines.empty()) return {};
        std::mt19937 rng((unsigned)std::chrono::steady_clock::now().time_since_epoch().count());
        return lines[rng() % lines.size()];
    }
}

Minecraft::Minecraft(Window* window, render::IRenderDevice* device)
    : m_window(window), m_device(device) 
{
    m_guiGraphics = std::make_unique<render::GuiGraphics>(device);
    m_panorama = std::make_unique<render::PanoramaRenderer>(device);
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
    
    // Flush profiling data
    std::time_t now = std::time(nullptr);
    std::tm* timeinfo = std::localtime(&now);
    std::ostringstream filename;
    filename << "profiling/profiles/profile_"
             << std::put_time(timeinfo, "%Y%m%d_%H%M%S") << ".csv";
    std::filesystem::create_directories("profiling/profiles");
    profiling::Profiler::instance().flushToCSV(filename.str());
}

void Minecraft::setScreen(std::unique_ptr<gui::Screen> screen) {
    MC_LOG_INFO("setScreen called with {}", screen ? screen->title() : "nullptr");
    m_currentScreen = std::move(screen);
    if (m_currentScreen) {
        m_cachedGuiScale = guiScale();
        m_cachedGuiWidth = guiScaledWidth();
        m_cachedGuiHeight = guiScaledHeight();
        m_currentScreen->init(this, m_cachedGuiWidth, m_cachedGuiHeight);
    }
}

int Minecraft::guiScale() const {
    const int maxScale = m_options.guiScale;
    int scale = 1;
    const int framebufferWidth = m_window ? m_window->width() : 0;
    const int framebufferHeight = m_window ? m_window->height() : 0;
    while (scale != maxScale
        && scale < framebufferWidth
        && scale < framebufferHeight
        && framebufferWidth / (scale + 1) >= 320
        && framebufferHeight / (scale + 1) >= 240) {
        ++scale;
    }
    return scale;
}

int Minecraft::guiScaledWidth() const {
    const int scale = std::max(1, guiScale());
    const int width = m_window ? m_window->width() : 0;
    return (width + scale - 1) / scale;
}

int Minecraft::guiScaledHeight() const {
    const int scale = std::max(1, guiScale());
    const int height = m_window ? m_window->height() : 0;
    return (height + scale - 1) / scale;
}

double Minecraft::guiMouseX() const {
    if (!m_window || m_window->width() <= 0) return 0.0;
    return (double)m_window->mouseX() * (double)guiScaledWidth() / (double)m_window->width();
}

double Minecraft::guiMouseY() const {
    if (!m_window || m_window->height() <= 0) return 0.0;
    return (double)m_window->mouseY() * (double)guiScaledHeight() / (double)m_window->height();
}

void Minecraft::resizeGui() {
    const int scale = guiScale();
    const int width = guiScaledWidth();
    const int height = guiScaledHeight();
    if (scale == m_cachedGuiScale && width == m_cachedGuiWidth && height == m_cachedGuiHeight) {
        return;
    }
    m_cachedGuiScale = scale;
    m_cachedGuiWidth = width;
    m_cachedGuiHeight = height;
    if (m_currentScreen) {
        m_currentScreen->init(this, width, height);
    }
}

void Minecraft::renderPanorama(render::ICommandList* cmd, int w, int h, float dtSeconds) {
    if (m_panorama) m_panorama->render(cmd, w, h, dtSeconds);
}

render::ITexture* Minecraft::panoramaOverlay() {
    return m_panorama ? m_panorama->overlay() : nullptr;
}

bool Minecraft::panoramaLoaded() const {
    return m_panorama && m_panorama->loaded();
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

    std::vector<std::pair<std::string, std::string>> readJsonAssetEntries(std::string_view prefix) {
        std::vector<std::pair<std::string, std::string>> entries;
        auto& assets = AssetManager::instance();
        for (const std::string& path : assets.list(prefix)) {
            if (!path.ends_with(".json")) {
                continue;
            }
            std::vector<uint8_t> bytes = assets.readRaw(path);
            if (bytes.empty()) {
                continue;
            }
            entries.emplace_back(path, std::string(bytes.begin(), bytes.end()));
        }
        return entries;
    }

    // Materialize every embedded "data/minecraft/..." pack entry into a disk
    // cache and return its ".../data/minecraft" root ("" when the pack lacks a
    // decoration-critical set — fail closed so the disk path can take over).
    //
    // Why a cache and not a reader callback: the CERTIFIED decoration TU
    // (FullChunkDecorateParityTest.cpp, owned by the decoration-parity work)
    // loads its worldgen JSON / tag JSON / structure template NBTs from a plain
    // dataDir via std::ifstream. Surfacing the embedded bytes as files keeps
    // that TU byte-identical to what the parity gates certify, with zero
    // changes to it. Files already present with the same size are not
    // rewritten, so steady-state startups only stat the cache.
    std::string materializeEmbeddedData() {
        namespace fs = std::filesystem;
        auto& assets = AssetManager::instance();
        const std::vector<std::string> paths = assets.list("data/minecraft/");

        std::size_t biomes = 0, blockTags = 0, fluidTags = 0, structures = 0;
        for (const std::string& p : paths) {
            if (p.starts_with("data/minecraft/worldgen/biome/")) ++biomes;
            else if (p.starts_with("data/minecraft/tags/block/")) ++blockTags;
            else if (p.starts_with("data/minecraft/tags/fluid/")) ++fluidTags;
            else if (p.starts_with("data/minecraft/structure/")) ++structures;
        }
        if (biomes == 0 || blockTags == 0 || fluidTags == 0 || structures == 0) {
            if (!paths.empty()) {
                MC_LOG_WARN("Embedded worldgen data incomplete (biome={} tags/block={} tags/fluid={} "
                            "structure={}); rebuild assets.bin — falling back to disk data",
                            biomes, blockTags, fluidTags, structures);
            }
            return "";
        }

        std::error_code ec;
        const fs::path root = fs::temp_directory_path(ec);
        if (ec) return "";
        const fs::path cache = root / "mcpp_embedded_data";
        std::size_t written = 0;
        for (const std::string& p : paths) {
            const std::vector<uint8_t> bytes = assets.readRaw(p);
            if (bytes.empty()) continue;
            const fs::path dst = cache / fs::path(p);
            if (fs::exists(dst, ec) && fs::file_size(dst, ec) == bytes.size()) continue;
            fs::create_directories(dst.parent_path(), ec);
            std::ofstream f(dst, std::ios::binary | std::ios::trunc);
            f.write(reinterpret_cast<const char*>(bytes.data()),
                    static_cast<std::streamsize>(bytes.size()));
            if (!f) {
                MC_LOG_WARN("Cannot write embedded-data cache file {} — falling back to disk data",
                            dst.generic_string());
                return "";
            }
            ++written;
        }
        MC_LOG_INFO("Embedded worldgen data cache: {} pack entries ({} written, rest up to date) at {}",
                    paths.size(), written, cache.generic_string());
        return (cache / "data" / "minecraft").generic_string();
    }
}

void Minecraft::ensureWorldgenData() {
    if (m_worldgenTried) return;
    m_worldgenTried = true;

    try {
        // EMBEDDED FIRST: the shipped exe must decorate in a directory with no
        // 26.1.2/ data checkout. (The parity gate binaries keep reading the
        // disk data directly and are unaffected by this preference.)
        if (std::string embedded = materializeEmbeddedData(); !embedded.empty()) {
            const auto biomeEntries = readJsonAssetEntries("data/minecraft/worldgen/biome/");
            const auto tagEntries = readJsonAssetEntries("data/minecraft/tags/block/");

            levelgen::feature::setJsonAssetReader([](std::string_view path) -> std::optional<std::string> {
                std::vector<uint8_t> bytes = AssetManager::instance().readRaw(path);
                if (bytes.empty()) {
                    return std::nullopt;
                }
                return std::string(bytes.begin(), bytes.end());
            });
            m_biomeFeatures = std::make_unique<levelgen::feature::BiomeFeatures>(
                levelgen::feature::BiomeFeatures::loadFromJsonEntries(biomeEntries));
            m_blockTags = std::make_unique<block::BlockTags>(
                block::BlockTags::loadFromJsonEntries(tagEntries));
            m_worldgenDir = embedded + "/worldgen";
            m_dataMinecraftDir = embedded;   // decoration context reads the materialized EMBEDDED bytes
            m_worldgenReady = true;
            MC_LOG_INFO("Worldgen decoration data loaded ({} biomes, {} block tags) from EMBEDDED assets",
                        m_biomeFeatures->biomeCount(), m_blockTags->tagCount());
            return;
        }

        // Disk fallback: dev checkouts whose assets.bin predates the embedded
        // data entries (run from the repo root so 26.1.2/data resolves).
        std::string dataRoot = discoverDataRoot();
        if (!dataRoot.empty()) {
            m_biomeFeatures = std::make_unique<levelgen::feature::BiomeFeatures>(
                levelgen::feature::BiomeFeatures::loadFromDirectory(dataRoot + "/minecraft/worldgen/biome"));
            m_blockTags = std::make_unique<block::BlockTags>(
                block::BlockTags::loadFromDirectory(dataRoot + "/minecraft/tags/block"));
            m_worldgenDir = dataRoot + "/minecraft/worldgen";
            m_dataMinecraftDir = dataRoot + "/minecraft";   // decoration context loads from DISK
            m_worldgenReady = true;
            MC_LOG_INFO("Worldgen decoration data loaded ({} biomes) from DISK: {}",
                        m_biomeFeatures->biomeCount(), dataRoot);
            return;
        }

        MC_LOG_WARN("Worldgen data not found in embedded assets or 26.1.2/data; "
                    "terrain will generate without trees/vegetation");
    } catch (const std::exception& e) {
        MC_LOG_WARN("Failed to load worldgen decoration data: {}", e.what());
        m_worldgenReady = false;
    }
}

void Minecraft::decorateChunk(LevelChunk& chunk) {
    if (!m_localGenerator) return;

    PROFILE_SCOPE_CHUNK("decorateChunk", chunk.pos().x, chunk.pos().z);

    // Delegates to the certified decoration machinery (EngineDecoration API via
    // BiomeDecorator). Cross-chunk feature writes go through the context's
    // region view over m_chunks — MAIN-THREAD ONLY (caches/hooks not thread-safe).
    try {
        levelgen::feature::applyBiomeDecoration(chunk);
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

void Minecraft::tryDecorate(ChunkPos cp) {
    LevelChunk* c = getChunk(cp);
    if (!c || c->decorated) return;
    // Require all 8 neighbours loaded so cross-chunk feature writes (tree foliage,
    // ore veins, structures) land in real chunks rather than being clipped.
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx || dz) && !getChunk({ cp.x + dx, cp.z + dz })) return;

    c->decorated = true;
    decorateChunk(*c);
    {
        PROFILE_SCOPE_CHUNK("runStructures", cp.x, cp.z);
        runStructures(cp);
    }
    // Cross-chunk writes can touch the neighbours — re-mesh the 3x3.
    {
        PROFILE_SCOPE("remesh_9chunk_neighborhood");
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (LevelChunk* n = getChunk({ cp.x + dx, cp.z + dz })) n->meshDirty = true;
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
        return m_localGenerator->getNoiseBiome(x >> 2, y >> 2, z >> 2);
    };
    try {
        levelgen::structure::generateStructures(active, m_worldSeed, world, biomeGetter, m_dataMinecraftDir);
    } catch (const std::exception& e) {
        MC_LOG_WARN("runStructures failed at ({},{}): {}", active.x, active.z, e.what());
    }
}

void Minecraft::startLocalGame(uint64_t seed, int spawnX, int spawnZ, std::optional<int> spawnY) {
    MC_LOG_INFO("Starting local singleplayer prototype world, seed={}, spawn=({}, {}, {})",
                seed, spawnX, spawnY ? std::to_string(*spawnY) : std::string("surface"), spawnZ);

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
    // Fresh world: drop any previous decoration context (its per-chunk state is
    // tied to the cleared m_chunks) and create the new one (loads the certified
    // worldgen data from disk; logs the data source + resolved feature count).
    levelgen::feature::resetEngineDecoration();
    levelgen::feature::ensureEngineDecoration(m_dataMinecraftDir, m_worldSeed, &m_chunks);
    // Generate JUST the spawn chunk synchronously so the player has ground
    // under their feet immediately. The async updateLocalChunks() fills the
    // surrounding area gradually via the thread pool — no multi-second freeze.
    // (Previously generated 11×11=121 chunks synchronously = ~6s freeze.)
    const ChunkPos spawnChunk = worldToChunk(spawnX, spawnZ);

    {
        PROFILE_SCOPE("startup_terrain_generation");
        // Only generate the spawn chunk + its 8 immediate neighbours (3×3) so the
        // player can see terrain in all directions. The rest streams in async.
        constexpr int STARTUP_RADIUS = 1;
        for (int dz = -STARTUP_RADIUS; dz <= STARTUP_RADIUS; ++dz) {
            for (int dx = -STARTUP_RADIUS; dx <= STARTUP_RADIUS; ++dx) {
                const int cx = spawnChunk.x + dx;
                const int cz = spawnChunk.z + dz;
                LevelChunk* chunk = getOrCreateChunk({cx, cz});
                std::vector<BlockPos> genMarks;
                m_localGenerator->fillFromNoise(*chunk, &genMarks);
                m_localGenerator->buildSurface(*chunk);
                m_localGenerator->applyCarvers(*chunk, &genMarks);
                levelgen::feature::freezeWorldgenHeights(*chunk, &genMarks);
                chunk->meshDirty = true;
            }
        }
    }

    // Decorate only the spawn chunk synchronously (so trees/structures appear
    // around the player immediately). Neighbours decorate async via updateLocalChunks.
    tryDecorate(spawnChunk);


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
        
        // Font + GUI textures are embedded as Windows resources (extracted from
        // client.jar — they aren't in assets.bin). Loaded ONCE into members and reused
        // to (re)build screens (so the title rebuilds correctly when you close Options).
        render::ITexture* fontTex = loadResourceTex(m_device, cmd, IDR_FONT_ASCII);
        m_font = std::make_unique<render::Font>(m_device, fontTex);
        m_logoTex    = loadResourceTex(m_device, cmd, IDR_GUI_LOGO);
        m_editionTex = loadResourceTex(m_device, cmd, IDR_GUI_EDITION);
        m_dirtTex    = loadResourceTex(m_device, cmd, IDR_GUI_DIRT);
        m_btnTex     = loadResourceTex(m_device, cmd, IDR_GUI_BUTTON);
        m_btnHlTex   = loadResourceTex(m_device, cmd, IDR_GUI_BUTTON_HL);
        m_langTex    = loadResourceTex(m_device, cmd, IDR_GUI_LANG);
        m_accessTex  = loadResourceTex(m_device, cmd, IDR_GUI_ACCESS);
        m_splashText = pickSplash();

        m_gui->setHotbarTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/hotbar.png"));
        m_gui->setSelectionTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/hotbar_selection.png"));
        m_gui->setCrosshairTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/crosshair.png"));
        m_gui->setHeartTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/heart/full.png"));
        m_gui->setFoodTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/food_full.png"));

        if (!m_inGame) {
            openTitleScreen();
        }

        guiInit = true;
        m_device->endFrame();
    }
}

void Minecraft::openTitleScreen() {
    auto ts = std::make_unique<gui::screens::TitleScreen>();
    ts->setLogoTexture(m_logoTex);
    ts->setEditionTexture(m_editionTex);
    ts->setDirtTexture(m_dirtTex);
    ts->setButtonTextures(m_btnTex, m_btnHlTex);
    ts->setIconTextures(m_langTex, m_accessTex);
    ts->setSplash(m_splashText);
    setScreen(std::move(ts));
}

void Minecraft::openOptionsScreen() {
    auto os = std::make_unique<gui::screens::OptionsScreen>();
    os->setButtonTextures(m_btnTex, m_btnHlTex);
    os->setBackAction([this]() { openTitleScreen(); });
    setScreen(std::move(os));
}

void Minecraft::openPauseScreen() {
    if (!m_inGame) return;
    if (m_window) m_window->captureMouse(false);
    auto ps = std::make_unique<gui::screens::PauseScreen>();
    ps->setButtonTextures(m_btnTex, m_btnHlTex);
    setScreen(std::move(ps));
}

void Minecraft::resumeGame() {
    if (!m_inGame) return;
    setScreen(nullptr);
    if (m_window) m_window->captureMouse(true);
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
            GeneratedChunk generated = it->future.get();
            std::unique_ptr<LevelChunk> chunk = std::move(generated.chunk);
            if (chunk) {
                ChunkPos cp = chunk->pos();
                // Check if still within radius
                if (std::abs(cp.x - px) <= RADIUS && std::abs(cp.z - pz) <= RADIUS) {
                    auto key = chunkKey(cp);
                    if (m_chunks.find(key) == m_chunks.end()) {
                        LevelChunk* ptr = chunk.get();
                        // Store terrain now; decoration is DEFERRED until all 8
                        // neighbours exist (the tryDecorate pass below) so trees/
                        // ores/structures write across borders without clipping.
                        m_chunks[key] = std::move(chunk);
                        // Freeze the chunk's *_WG heightmaps + inject its generation
                        // marks into the decoration context — on the MAIN thread, at
                        // integration time (post-carvers, pre-decoration).
                        levelgen::feature::freezeWorldgenHeights(*ptr, &generated.genMarks);
                        ptr->meshDirty = true;
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

    // 2b. Deferred decoration: decorate loaded, undecorated chunks whose 8
    // neighbours are present (so trees/ores/structures get full cross-chunk context,
    // no border clipping). BUDGETED per tick: decoration is heavy and runs
    // on the main thread, so doing every eligible chunk at once causes the movement
    // stutter. Nearest-first so the area around the player fills in before the rim.
    //
    // The budget is DYNAMIC: when many chunks are pending decoration (player moved
    // fast, just spawned, or decoration fell behind), allow more per tick to catch
    // up. When the queue is short, keep it low so individual frames stay responsive.
    // Target: keep total decorate work ~10 ms/frame max.
    {
        std::vector<ChunkPos> ready;
        for (const auto& [key, chunk] : m_chunks)
            if (chunk && !chunk->decorated) ready.push_back(chunk->pos());
        std::sort(ready.begin(), ready.end(), [px, pz](ChunkPos a, ChunkPos b) {
            return (a.x - px) * (a.x - px) + (a.z - pz) * (a.z - pz)
                 < (b.x - px) * (b.x - px) + (b.z - pz) * (b.z - pz);
        });
        // Dynamic budget: 4 baseline + 1 per 4 pending (capped at 24).
        // At 60 FPS this allows up to 24*60 = 1440 decorations/sec when backlogged.
        int maxDecorate = 4 + static_cast<int>(ready.size()) / 4;
        if (maxDecorate > 24) maxDecorate = 24;
        if (maxDecorate < 2)  maxDecorate = 2;
        int done = 0;
        for (ChunkPos cp : ready) {
            LevelChunk* c = getChunk(cp);
            if (!c || c->decorated) continue;
            tryDecorate(cp);
            if (c->decorated && ++done >= maxDecorate) break;
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
    // Dynamic queue budget: queue more when few chunks are loaded (startup / fast
    // travel), fewer once the area is mostly filled. Helps the engine catch up
    // quickly after a teleport or fast sprint without over-queuing at steady state.
    constexpr int MAX_QUEUE_PER_TICK = 12;
    int queueBudget = MAX_QUEUE_PER_TICK;
    const size_t loadedCount = m_chunks.size();
    const size_t targetCount = static_cast<size_t>((2 * RADIUS + 1) * (2 * RADIUS + 1));
    if (loadedCount < targetCount / 2) queueBudget = MAX_QUEUE_PER_TICK * 2;  // 24 when <50% loaded
    for (const auto& cand : candidates) {
        if (!m_threadPool) break;
        
        m_generationTasks.push_back({
            cand.pos,
            m_threadPool->enqueue([pos = cand.pos, seed = m_worldSeed]() -> GeneratedChunk {
                GeneratedChunk out;
                out.chunk = std::make_unique<LevelChunk>(pos);
                struct ThreadGeneratorCache {
                    uint64_t seed = 0;
                    std::unique_ptr<levelgen::NoiseBasedChunkGenerator> generator;
                };
                thread_local ThreadGeneratorCache cache;
                if (!cache.generator || cache.seed != seed) {
                    PROFILE_SCOPE_CHUNK("generator_threadlocal_setup", pos.x, pos.z);
                    cache.seed = seed;
                    cache.generator = std::make_unique<levelgen::NoiseBasedChunkGenerator>(seed);
                }
                levelgen::NoiseBasedChunkGenerator& generator = *cache.generator;
                {
                    PROFILE_SCOPE_CHUNK("fillFromNoise", pos.x, pos.z);
                    generator.fillFromNoise(*out.chunk, &out.genMarks);
                }
                {
                    PROFILE_SCOPE_CHUNK("buildSurface", pos.x, pos.z);
                    generator.buildSurface(*out.chunk);
                }
                {
                    PROFILE_SCOPE_CHUNK("applyCarvers", pos.x, pos.z);
                    generator.applyCarvers(*out.chunk, &out.genMarks);
                }
                return out;
            })
        });
        
        if (++queued >= queueBudget) {
            break;
        }
    }
}

} // namespace mc
