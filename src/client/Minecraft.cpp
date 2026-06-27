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
#ifdef _WIN32
#include <windows.h>
#else
#include "platform/Platform.h"
#include <unistd.h>
#endif
#include <cmath>
#include <cstdlib>
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
    // Opt-in (MCPP_CHUNK_CACHE=1) in-memory chunk persistence. Default off restores
    // the original erase-on-unload streaming while the in-game crash is diagnosed.
    bool chunkCacheEnabled() {
        static const bool v = (std::getenv("MCPP_CHUNK_CACHE") != nullptr);
        return v;
    }

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
    // On Windows these come from the .exe's .rc section. On Linux there is no .rc
    // mechanism, so we fall back to reading the same asset from assets.bin (which
    // the asset_packer bundles from the extracted client.jar). The IDR_* → asset path
    // mapping comes from cmake/PrepareRuntimeAssets.cmake's copy_resource calls.
    render::ITexture* loadResourceTex(render::IRenderDevice* dev, render::ICommandList* cmd, int resourceId) {
        // Map resource IDs to their asset paths in assets.bin. These MUST match
        // the copy_resource() calls in cmake/PrepareRuntimeAssets.cmake.
        const char* assetPath = nullptr;
        switch (resourceId) {
            case IDR_FONT_ASCII:    assetPath = "minecraft/textures/font/ascii.png"; break;
            case IDR_GUI_LOGO:      assetPath = "minecraft/textures/gui/title/minecraft.png"; break;
            case IDR_GUI_EDITION:   assetPath = "minecraft/textures/gui/title/edition.png"; break;
            case IDR_GUI_DIRT:      assetPath = "minecraft/textures/block/dirt.png"; break;
            case IDR_GUI_BUTTON:    assetPath = "minecraft/textures/gui/sprites/widget/button.png"; break;
            case IDR_GUI_BUTTON_HL: assetPath = "minecraft/textures/gui/sprites/widget/button_highlighted.png"; break;
            case IDR_GUI_LANG:      assetPath = "minecraft/textures/gui/sprites/icon/language.png"; break;
            case IDR_GUI_ACCESS:    assetPath = "minecraft/textures/gui/sprites/icon/accessibility.png"; break;
        }
#ifdef _WIN32
        HMODULE hmod = GetModuleHandleW(nullptr);
        HRSRC hres = FindResourceW(hmod, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (hres) {
            HGLOBAL hg = LoadResource(hmod, hres);
            const uint8_t* data = static_cast<const uint8_t*>(LockResource(hg));
            DWORD size = SizeofResource(hmod, hres);
            if (data && size > 0) return decodeTex(dev, cmd, data, (int)size);
        }
        // RCDATA not found (e.g. CI build where PrepareRuntimeAssets.cmake didn't
        // run) — fall through to the assets.bin path below.
#endif
        if (assetPath) {
            auto b = AssetManager::instance().readRaw(assetPath);
            if (!b.empty()) return decodeTex(dev, cmd, b.data(), (int)b.size());
        }
        return nullptr;
    }

    // Read a text resource (e.g. splashes.txt) embedded as RCDATA.
    // Same pattern as loadResourceTex: Windows RCDATA first, then assets.bin.
    std::string loadResourceText(int resourceId) {
        const char* assetPath = nullptr;
        switch (resourceId) {
            case IDR_SPLASHES: assetPath = "minecraft/texts/splashes.txt"; break;
        }
#ifdef _WIN32
        HMODULE hmod = GetModuleHandleW(nullptr);
        HRSRC hres = FindResourceW(hmod, MAKEINTRESOURCEW(resourceId), RT_RCDATA);
        if (hres) {
            HGLOBAL hg = LoadResource(hmod, hres);
            const char* data = static_cast<const char*>(LockResource(hg));
            DWORD size = SizeofResource(hmod, hres);
            if (data && size > 0) return std::string(data, data + size);
        }
        // RCDATA not found — fall through to assets.bin.
#endif
        if (assetPath) {
            auto b = AssetManager::instance().readRaw(assetPath);
            if (!b.empty()) return std::string(b.begin(), b.end());
        }
        return {};
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

    // Start multiple decoration worker threads for parallel chunk decoration.
    // The EngineDecorationContext uses thread_local globals so each worker
    // gets its own copy. MultiChunkLevel writes are protected by
    // m_chunkWriteMutex. Use half the cores for decoration (leave the other
    // half for generation + rendering).
    unsigned int decoThreads = std::max(1u, std::thread::hardware_concurrency() / 2);
    if (decoThreads > 4) decoThreads = 4;  // cap at 4 — more isn't helpful due to write lock contention
    MC_LOG_INFO("Starting {} decoration worker threads", decoThreads);
    for (unsigned int i = 0; i < decoThreads; ++i) {
        m_decorationThreads.emplace_back([this] { decorationWorkerLoop(); });
    }

    // Explicitly set the TitleScreen
    auto ts = std::make_unique<gui::screens::TitleScreen>();
    setScreen(std::move(ts));
}

Minecraft::~Minecraft() {
    // Signal the decoration workers to stop and wait for them to finish.
    m_decorationStop.store(true);
    m_decorationQueueCv.notify_all();
    for (auto& t : m_decorationThreads) {
        if (t.joinable()) t.join();
    }

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
    // Shared-lock the chunk map: unloadChunk takes the exclusive lock, so this
    // read can never race with an erase. Uncontended shared_lock is ~10ns.
    std::shared_lock<std::shared_mutex> lk(m_chunksMutex);
    auto it = m_chunks.find(chunkKey(pos));
    return it == m_chunks.end() ? nullptr : it->second.get();
}
LevelChunk* Minecraft::getOrCreateChunk(ChunkPos pos) {
    // Exclusive lock — getOrCreateChunk mutates the map.
    std::unique_lock<std::shared_mutex> lk(m_chunksMutex);
    auto key = chunkKey(pos);
    auto it  = m_chunks.find(key);
    if (it != m_chunks.end()) return it->second.get();
    auto chunk = std::make_unique<LevelChunk>(pos);
    LevelChunk* ptr = chunk.get();
    m_chunks[key] = std::move(chunk);
    return ptr;
}
void Minecraft::unloadChunk(ChunkPos pos) {
    // Try to acquire the exclusive lock WITHOUT blocking. If the decoration
    // worker holds a shared lock (during a 50-385ms decoration turn), we SKIP
    // the unload this tick and retry next tick. This prevents the main thread
    // from blocking for hundreds of ms, which was the root cause of the hangs.
    //
    // The chunk will be unloaded on a future tick when the decoration worker
    // is between turns. This is safe — the chunk stays loaded and visible,
    // just takes a bit longer to unload.
    std::unique_lock<std::shared_mutex> lk(m_chunksMutex, std::try_to_lock);
    if (!lk.owns_lock()) {
        // Decoration worker is busy — skip, retry next tick.
        return;
    }
    const int64_t key = chunkKey(pos);
    auto it = m_chunks.find(key);
    if (it == m_chunks.end()) return;

    // The in-memory persistence cache is OPT-IN (MCPP_CHUNK_CACHE=1). It is the
    // newest change to the streaming hot path and is the prime suspect for the
    // in-game crash, so the default is the original, known-good behaviour: just
    // erase. With the cache off, revisiting a chunk regenerates+re-decorates it
    // (the cross-chunk feature duplication returns — that is the trade-off until
    // the crash is understood). Enable the cache to test/keep the duplication fix.
    if (!chunkCacheEnabled()) { m_chunks.erase(it); return; }

    // Only PERSIST a chunk that is fully decorated and not still awaiting its
    // decoration turn. A chunk whose decoration was submitted but not yet completed
    // (still in m_decorationQueue) must NOT be cached: if we cached it, on restore
    // it would look "decorated" and never actually receive its features. Erase it
    // instead — revisiting regenerates it from scratch and decorates it freshly
    // (self-healing, exactly as before this cache existed). Note: a chunk the worker
    // is actively decorating can't be here yet, because the worker holds the shared
    // lock for its whole turn and we hold the exclusive lock — so we already waited
    // for it to finish, after which it is decorated and out of the queue.
    bool cacheable = it->second && it->second->decorated;
    if (cacheable) {
        std::lock_guard<std::mutex> qlk(m_decorationQueueMutex);
        for (const ChunkPos& q : m_decorationQueue)
            if (chunkKey(q) == key) { cacheable = false; break; }
    }
    if (!cacheable) {
        m_chunks.erase(it);
        return;
    }

    // Persist the chunk in memory instead of destroying it. Revisiting restores
    // it verbatim, so its terrain + decoration (including features that spilled in
    // from neighbours) are NOT regenerated — and, crucially, its OWN features are
    // never re-spilled into neighbours. That re-spill on every regeneration is what
    // produced the artificial over-density of trees/fossils/structures near the
    // streaming edge. Vanilla persists chunks to disk; this is the in-memory form.
    // Moving the unique_ptr does not move the LevelChunk object, so any LevelChunk*
    // held elsewhere stays valid.
    m_chunkCache[key] = std::move(it->second);
    m_chunks.erase(it);
    m_chunkCacheOrder.push_back(key);
    // Drop stale front entries (keys already restored or evicted) so the order
    // deque can't grow unboundedly under repeated unload/restore cycles (pacing
    // back and forth over the same boundary).
    while (!m_chunkCacheOrder.empty() &&
           m_chunkCache.find(m_chunkCacheOrder.front()) == m_chunkCache.end())
        m_chunkCacheOrder.pop_front();
    // Evict the oldest live entries beyond capacity (truly free them).
    while (m_chunkCache.size() > kChunkCacheCapacity && !m_chunkCacheOrder.empty()) {
        const int64_t oldest = m_chunkCacheOrder.front();
        m_chunkCacheOrder.pop_front();
        m_chunkCache.erase(oldest);
        // Skip any stale entries newly exposed at the front.
        while (!m_chunkCacheOrder.empty() &&
               m_chunkCache.find(m_chunkCacheOrder.front()) == m_chunkCache.end())
            m_chunkCacheOrder.pop_front();
    }
}

void Minecraft::restoreCachedChunksInRadius(int px, int pz, int radius) {
    // Main-thread only. m_chunkCache is mutated solely on the main thread, so the
    // empty() check and the collection pass do not race the decoration worker
    // (which never touches m_chunkCache). The actual move back into m_chunks takes
    // the exclusive lock so the worker never observes a half-moved map.
    if (!chunkCacheEnabled() || m_chunkCache.empty()) return;
    std::vector<ChunkPos> toRestore;
    {
        std::shared_lock<std::shared_mutex> lk(m_chunksMutex);
        for (int cz = -radius; cz <= radius; ++cz) {
            for (int cx = -radius; cx <= radius; ++cx) {
                const ChunkPos cp{ px + cx, pz + cz };
                const int64_t key = chunkKey(cp);
                if (m_chunks.find(key) != m_chunks.end()) continue;   // already live
                if (m_chunkCache.find(key) != m_chunkCache.end()) toRestore.push_back(cp);
            }
        }
    }
    if (toRestore.empty()) return;
    std::unique_lock<std::shared_mutex> lk(m_chunksMutex);
    static constexpr int kCardinal[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
    for (const ChunkPos cp : toRestore) {
        const int64_t key = chunkKey(cp);
        auto cit = m_chunkCache.find(key);
        if (cit == m_chunkCache.end()) continue;
        LevelChunk* ptr = cit->second.get();
        m_chunks[key] = std::move(cit->second);
        m_chunkCache.erase(cit);
        // Rebuild the mesh (GPU meshes are reconciled from meshDirty) and re-seam
        // the cardinal neighbours so the borders stitch to whatever is loaded now.
        ptr->meshDirty = true;
        for (const auto& d : kCardinal) {
            auto itN = m_chunks.find(chunkKey({ cp.x + d[0], cp.z + d[1] }));
            if (itN != m_chunks.end() && itN->second) itN->second->meshDirty = true;
        }
    }
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
#ifdef _WIN32
        wchar_t buf[MAX_PATH];
        DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH);
        if (n > 0 && n < MAX_PATH) {
            if (auto r = probe(fs::path(buf).parent_path()); !r.empty()) return r;
        }
#else
        // Linux: also check the executable's directory via /proc/self/exe
        char exePath[4096];
        ssize_t len = readlink("/proc/self/exe", exePath, sizeof(exePath) - 1);
        if (len > 0) {
            exePath[len] = '\0';
            if (auto r = probe(fs::path(exePath).parent_path()); !r.empty()) return r;
        }
#endif
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
    // Phase 3: the ENTIRE decoration pipeline (beginFeatureTurn + runStructures
    // + decorateChunk) now runs on the worker thread. The main thread just
    // checks eligibility and submits.
    //
    // Thread-safety verification:
    //  - beginFeatureTurn: uses EngineDecorationContext, Phase 1 made its
    //    globals thread_local → safe.
    //  - runStructures: calls m_localGenerator->getBaseHeight/getNoiseBiome,
    //    which are const and only read pure density functions + the
    //    thread-safe Climate RTree → safe.
    //  - runStructures: calls generateStructures → Runtime::generate, which
    //    now takes a std::shared_mutex lock on its caches → safe.
    //  - decorateChunk: calls applyBiomeDecoration → same EngineDecorationContext
    //    path as before → safe (Phase 1).

    // Take shared lock to read the chunk and its neighbours. We need to verify
    // all 9 chunks exist before submitting — if any neighbour is missing, the
    // decoration would clip cross-chunk writes.
    std::shared_lock<std::shared_mutex> chunksLk(m_chunksMutex);
    auto it = m_chunks.find(chunkKey(cp));
    LevelChunk* c = (it != m_chunks.end()) ? it->second.get() : nullptr;
    if (!c || c->decorated) return;
    for (int dz = -1; dz <= 1; ++dz)
        for (int dx = -1; dx <= 1; ++dx)
            if ((dx || dz) && m_chunks.find(chunkKey({ cp.x + dx, cp.z + dz })) == m_chunks.end())
                return;  // neighbour missing — wait for next tick

    c->decorated = true;  // claim it synchronously so we don't resubmit
    chunksLk.unlock();

    // Submit the entire decoration pipeline to the worker.
    {
        std::lock_guard<std::mutex> lk(m_decorationQueueMutex);
        m_decorationQueue.push_back(cp);
    }
    m_decorationQueueCv.notify_all();  // wake all workers for parallel decoration
}

void Minecraft::decorationWorkerLoop() {
    // Phase 3 of Option B refactor: decoration worker thread.
    //
    // Pops chunks from m_decorationQueue and runs the FULL decoration pipeline:
    //   beginFeatureTurn → runStructures → decorateChunk
    //
    // All three are now off the main thread. The main thread never blocks on
    // decoration or structures.
    //
    // The worker holds a shared_lock on m_chunksMutex for the duration of each
    // turn so the engine cannot unload the chunks being decorated.
    while (true) {
        ChunkPos cp;
        {
            std::unique_lock<std::mutex> lk(m_decorationQueueMutex);
            m_decorationQueueCv.wait(lk, [this] {
                return m_decorationStop.load() || !m_decorationQueue.empty();
            });
            if (m_decorationStop.load() && m_decorationQueue.empty()) return;
            cp = m_decorationQueue.front();
            m_decorationQueue.erase(m_decorationQueue.begin());
        }
        // Log queue depth + decoration start.
        auto decoStart = std::chrono::steady_clock::now();
        // Briefly take the shared lock to look up the chunk pointer, then
        // RELEASE it. The previous code held the shared lock for the entire
        // 50-385ms decoration turn, which blocked the main thread's try_to_lock
        // on m_chunksMutex (for chunk integration) and caused genTasks to pile
        // up indefinitely.
        //
        // Safety: the chunk won't be unloaded during decoration because
        // unloadChunk now uses try_to_lock — if it can't get the exclusive lock
        // (because we might be writing), it skips. And we hold m_chunkWriteMutex
        // during the actual block writes, which prevents the mesh snapshot from
        // reading partial data.
        LevelChunk* c = nullptr;
        {
            std::shared_lock<std::shared_mutex> chunksLk(m_chunksMutex);
            auto it = m_chunks.find(chunkKey(cp));
            c = (it != m_chunks.end()) ? it->second.get() : nullptr;
        }
        if (!c) continue;
        // Exclude the render thread's mesh-snapshot copy while we write block data.
        // Use try_lock: if the render thread holds it (snapshotting), skip this
        // chunk and retry next iteration. This prevents workers from blocking
        // on each other (the previous blocking lock serialized ALL workers).
        std::unique_lock<std::mutex> writeLk(m_chunkWriteMutex, std::try_to_lock);
        if (!writeLk.owns_lock()) {
            // Render thread is snapshotting — put the chunk back in the queue.
            std::lock_guard<std::mutex> qlk(m_decorationQueueMutex);
            m_decorationQueue.push_back(cp);
            continue;
        }
        try {
            auto t0 = std::chrono::steady_clock::now();
            levelgen::feature::beginFeatureTurn(*c);
            auto t1 = std::chrono::steady_clock::now();
            runStructures(cp);
            auto t2 = std::chrono::steady_clock::now();
            decorateChunk(*c);
            auto t3 = std::chrono::steady_clock::now();
            double featureMs = std::chrono::duration<double, std::milli>(t1 - t0).count();
            double structMs = std::chrono::duration<double, std::milli>(t2 - t1).count();
            double decoMs = std::chrono::duration<double, std::milli>(t3 - t2).count();
            double totalMs = std::chrono::duration<double, std::milli>(t3 - t0).count();
            if (totalMs > 50.0) {
                MC_LOG_WARN("SLOW DECORATE ({},{}) total={:.1f}ms (feature={:.1f} struct={:.1f} deco={:.1f})",
                            cp.x, cp.z, totalMs, featureMs, structMs, decoMs);
            }
        } catch (const std::exception& e) {
            MC_LOG_WARN("decorationWorker failed at ({},{}): {}", cp.x, cp.z, e.what());
        }
        // Report completion — the main thread will mark meshDirty on the 3x3.
        {
            std::lock_guard<std::mutex> lk(m_decorationDoneMutex);
            m_decorationDone.push_back(cp);
        }
    }
}

void Minecraft::pollDecorationDone() {
    // Drain m_decorationDone on the main thread and mark the 3x3 neighborhood
    // for re-meshing. This is the only main-thread work that happens after the
    // worker finishes — it's a handful of atomic-flag sets, very cheap.
    std::vector<ChunkPos> done;
    {
        std::lock_guard<std::mutex> lk(m_decorationDoneMutex);
        done.swap(m_decorationDone);
    }
    if (done.empty()) return;
    PROFILE_SCOPE("remesh_9chunk_neighborhood");
    for (ChunkPos cp : done) {
        for (int dz = -1; dz <= 1; ++dz)
            for (int dx = -1; dx <= 1; ++dx)
                if (LevelChunk* n = getChunk({ cp.x + dx, cp.z + dz }))
                    n->meshDirty = true;
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
        // Structure starts are projected before NOISE with the base column height;
        // reusing the post-Beardifier chunk heightmap shifts villages on slopes.
        return m_localGenerator ? m_localGenerator->getBaseHeight(x, z) - 1 : 0;
    };
    world.placeFeature = [active](const std::string& featureId, levelgen::RandomSource& random, BlockPos origin) {
        return levelgen::feature::placeStructurePoolFeature(featureId, random, origin, active);
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

std::string Minecraft::getNoiseBiomeName(int quartX, int quartY, int quartZ) const {
    return m_localGenerator ? m_localGenerator->getNoiseBiome(quartX, quartY, quartZ) : std::string();
}

mc::levelgen::Beardifier Minecraft::buildChunkBeardifier(ChunkPos pos) {
    if (!m_localGenerator || m_dataMinecraftDir.empty()) return {};
    // WORLD_SURFACE_WG topmost-solid column height (getBaseHeight returns first-free).
    auto columnHeight = [this](int x, int z) { return m_localGenerator->getBaseHeight(x, z) - 1; };
    auto biomeGetter = [this](int x, int y, int z) {
        return m_localGenerator->getNoiseBiome(x >> 2, y >> 2, z >> 2);
    };
    try {
        return levelgen::structure::generateBeardifier(pos, m_worldSeed, columnHeight, biomeGetter,
                                                       m_dataMinecraftDir);
    } catch (const std::exception& e) {
        MC_LOG_WARN("buildChunkBeardifier failed at ({},{}): {}", pos.x, pos.z, e.what());
        return {};
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
    // Drain the decoration queue and wait for the worker to finish its current
    // turn before clearing the chunk map — otherwise the worker could be
    // writing to a chunk we're about to destroy.
    {
        std::lock_guard<std::mutex> qlk(m_decorationQueueMutex);
        m_decorationQueue.clear();
    }
    // Take the exclusive lock on the chunk map and hold it until after clear.
    // This blocks the worker from starting any new turn (it takes shared lock).
    // If the worker is mid-turn, we block here until it finishes.
    {
        std::unique_lock<std::shared_mutex> chunksLk(m_chunksMutex);
        // Drain any done notifications too so they don't fire on stale chunks.
        std::lock_guard<std::mutex> dlk(m_decorationDoneMutex);
        m_decorationDone.clear();
        m_chunks.clear();
        // Drop the persistence cache too — its chunks belong to the OLD world/seed
        // and must never be restored into the new one.
        m_chunkCache.clear();
        m_chunkCacheOrder.clear();
    }
    m_generationTasks.clear();
    m_queuedChunks.clear();
    m_localPlayer.reset();

    m_worldSeed = seed;
    m_haveLastStreamPlayerPos = false;
    m_lastLocalMovement = std::chrono::steady_clock::now();
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
        // Generate the spawn chunk + neighbours (5×5 = 25 chunks) IN PARALLEL
        // using the thread pool. The previous code did this synchronously on
        // the main thread, which took 25 × 40ms = 1 second of frozen screen.
        constexpr int STARTUP_RADIUS = 2;
        const int totalChunks = (2 * STARTUP_RADIUS + 1) * (2 * STARTUP_RADIUS + 1);
        struct StartupResult {
            ChunkPos pos;
            std::unique_ptr<LevelChunk> chunk;
            std::vector<BlockPos> genMarks;
        };
        std::vector<std::future<StartupResult>> futures;
        futures.reserve(totalChunks);
        for (int dz = -STARTUP_RADIUS; dz <= STARTUP_RADIUS; ++dz) {
            for (int dx = -STARTUP_RADIUS; dx <= STARTUP_RADIUS; ++dx) {
                const int cx = spawnChunk.x + dx;
                const int cz = spawnChunk.z + dz;
                futures.push_back(m_threadPool->enqueue(
                    [this, cx, cz, seed = m_worldSeed]() -> StartupResult {
                        StartupResult out;
                        out.pos = {cx, cz};
                        out.chunk = std::make_unique<LevelChunk>(ChunkPos{cx, cz});
                        levelgen::Beardifier beard = buildChunkBeardifier({cx, cz});
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
                                                  beard.isEmpty() ? nullptr : &beard);
                        cache.gen->buildSurface(*out.chunk);
                        cache.gen->applyCarvers(*out.chunk, &out.genMarks);
                        return out;
                    }));
            }
        }
        // Collect results and integrate into the chunk map.
        for (auto& f : futures) {
            StartupResult res = f.get();
            if (res.chunk) {
                LevelChunk* ptr = res.chunk.get();
                auto key = chunkKey(res.pos);
                std::unique_lock<std::shared_mutex> lk(m_chunksMutex);
                m_chunks[key] = std::move(res.chunk);
                levelgen::feature::freezeWorldgenHeights(*ptr, &res.genMarks);
                ptr->meshDirty = true;
            }
        }
    }

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

    // Periodic state logging — every 100 ticks (~5s at 20 TPS), log the queue
    // depths + chunk count so we can see if the game is backing up.
    static int s_tickCounter = 0;
    if (m_inGame && ++s_tickCounter % 100 == 0) {
        size_t decoQueueSize = 0;
        size_t decoDoneSize = 0;
        {
            std::lock_guard<std::mutex> qlk(m_decorationQueueMutex);
            decoQueueSize = m_decorationQueue.size();
        }
        {
            std::lock_guard<std::mutex> dlk(m_decorationDoneMutex);
            decoDoneSize = m_decorationDone.size();
        }
        MC_LOG_INFO("STATE chunks={} genTasks={} queuedChunks={} decoQueue={} decoDone={}",
                    m_chunks.size(), m_generationTasks.size(), m_queuedChunks.size(),
                    decoQueueSize, decoDoneSize);
    }

    if (m_inGame) {
        if (m_connection && m_connection->isConnected()
            && m_connection->state == net::ConnectionState::Play) {
            m_localPlayer.tick(m_connection.get(), m_window);

            for (auto& [id, entity] : g_entities) {
                if (entity) entity->tick();
            }

            if (m_soundManager) {
                const auto& s = m_localPlayer.state();
#ifdef _WIN32
                audio::SoundEngine::instance().setListenerPosition(s.x, s.y, s.z, s.yaw, s.pitch);
#else
                m_soundManager->setListenerPosition(s.x, s.y, s.z, s.yaw, s.pitch);
#endif
            }
        } else if (!m_connection) {
            updateLocalChunks();

            if (m_soundManager) {
                const auto& s = m_localPlayer.state();
#ifdef _WIN32
                audio::SoundEngine::instance().setListenerPosition(s.x, s.y, s.z, s.yaw, s.pitch);
#else
                m_soundManager->setListenerPosition(s.x, s.y, s.z, s.yaw, s.pitch);
#endif
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

        // Slider widget textures — loaded from assets.bin (widget/slider.png +
        // widget/slider_handle.png + widget/slider_handle_highlighted.png).
        // These are NOT in the .rc resource IDs because they're only used by the
        // option-screen sliders, not the title screen. loadAssetTex reads them
        // directly from the MCAS pack.
        m_sliderTrackTex    = loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/slider.png");
        m_sliderHandleTex   = loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/slider_handle.png");
        m_sliderHandleHlTex = loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/slider_handle_highlighted.png");

        // List textures (header/footer separators + scrollbar + list background).
        // Used by OptionsSubScreen to draw the scrolling option list 1:1 with vanilla.
        m_headerSepTex   = loadAssetTex(m_device, cmd, "minecraft/textures/gui/header_separator.png");
        m_footerSepTex   = loadAssetTex(m_device, cmd, "minecraft/textures/gui/footer_separator.png");
        m_scrollerTex    = loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/scroller.png");
        m_scrollerBgTex  = loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/widget/scroller_background.png");
        // menu_list_background.png is a 32x32 tiled dark texture for list backgrounds.
        // Vanilla has 2 variants: menu_list_background (out-of-world) and
        // inworld_menu_list_background (in-game). We use the in-world variant
        // since most option screens are opened from the pause menu.
        m_listBgTex      = loadAssetTex(m_device, cmd, "minecraft/textures/gui/inworld_menu_list_background.png");

        m_gui->setHotbarTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/hotbar.png"));
        m_gui->setSelectionTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/hotbar_selection.png"));
        m_gui->setCrosshairTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/crosshair.png"));
        // Heart sprites: container (outline bg) + full + half. 1:1 with the
        // sprites referenced in Gui.java (HEART_*_SPRITE constants).
        m_gui->setHeartContainerTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/heart/container.png"));
        m_gui->setHeartFullTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/heart/full.png"));
        m_gui->setHeartHalfTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/heart/half.png"));
        // Food sprites: empty (bg) + full + half. 1:1 with FOOD_*_SPRITE.
        m_gui->setFoodEmptyTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/food_empty.png"));
        m_gui->setFoodFullTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/food_full.png"));
        m_gui->setFoodHalfTexture(loadAssetTex(m_device, cmd, "minecraft/textures/gui/sprites/hud/food_half.png"));

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
    os->setSliderTextures(m_sliderTrackTex, m_sliderHandleTex, m_sliderHandleHlTex);
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
    const auto now = std::chrono::steady_clock::now();

    const PlayerState& playerState = m_localPlayer.state();
    const double mdx = playerState.x - m_lastStreamPlayerX;
    const double mdy = playerState.y - m_lastStreamPlayerY;
    const double mdz = playerState.z - m_lastStreamPlayerZ;
    if (!m_haveLastStreamPlayerPos || (mdx * mdx + mdy * mdy + mdz * mdz) > 0.0001) {
        if (m_haveLastStreamPlayerPos) {
            m_lastLocalMovement = now;
        }
        m_lastStreamPlayerX = playerState.x;
        m_lastStreamPlayerY = playerState.y;
        m_lastStreamPlayerZ = playerState.z;
        m_haveLastStreamPlayerPos = true;
    }

    const bool movementKeyDown = m_window && !m_currentScreen && (
        m_window->isKeyDown('W') || m_window->isKeyDown('A') ||
        m_window->isKeyDown('S') || m_window->isKeyDown('D') ||
        m_window->isKeyDown(VK_SPACE) || m_window->isKeyDown(VK_CONTROL) ||
        m_window->isKeyDown(VK_SHIFT));
    if (movementKeyDown) {
        m_lastLocalMovement = now;
    }
    // Phase 3: decoration runs on a worker thread, so we no longer need the
    // 750ms player-idle gate. The main thread submits decoration requests
    // regardless of player movement — the worker handles them async.
    (void)now;  // suppress unused warning if m_lastLocalMovement isn't read elsewhere
    
    constexpr int RADIUS = 6;
    
    // 1. Unload chunks outside RADIUS
    std::vector<ChunkPos> toUnload;
    {
        // Shared lock — read iteration over the chunk map. The actual erase
        // happens in unloadChunk() which takes the exclusive lock.
        std::shared_lock<std::shared_mutex> lk(m_chunksMutex);
        for (const auto& [key, chunk] : m_chunks) {
            if (chunk) {
                ChunkPos cp = chunk->pos();
                if (std::abs(cp.x - px) > RADIUS || std::abs(cp.z - pz) > RADIUS) {
                    toUnload.push_back(cp);
                }
            }
        }
    }
    for (auto cp : toUnload) {
        unloadChunk(cp);
    }

    // 1b. Restore any in-radius chunks from the persistence cache BEFORE the
    // generation pass below, so they count as already loaded and are not
    // regenerated/re-decorated (which would re-spill their cross-chunk features).
    restoreCachedChunksInRadius(px, pz, RADIUS);

    // 2. Poll completed generation tasks. Scale the integration budget with
    // how many chunks are missing — when the world is loading (few chunks),
    // integrate aggressively so the player sees terrain fast. When steady-state
    // (most chunks loaded), keep it low to avoid frame stalls.
    const size_t targetCount = static_cast<size_t>((2 * RADIUS + 1) * (2 * RADIUS + 1));
    const size_t loadedCount = m_chunks.size();
    int maxIntegrate;
    if (loadedCount < targetCount / 2) {
        maxIntegrate = 16;  // aggressive: up to 16 chunks/tick during initial load
    } else if (loadedCount < targetCount * 3 / 4) {
        maxIntegrate = 8;   // moderate: 8 chunks/tick during mid-load
    } else {
        maxIntegrate = 4;   // conservative: 4 chunks/tick steady-state
    }
    int integrated = 0;
    for (auto it = m_generationTasks.begin(); it != m_generationTasks.end(); ) {
        if (integrated >= maxIntegrate) break;
        // Only check if the future is ready. Do NOT call get() yet — get()
        // consumes the future, and if we can't get the lock below, we'd need
        // to put the result back, which is impossible with std::future.
        if (it->future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            ++it;
            continue;
        }
        // Try to acquire the exclusive lock BEFORE consuming the future.
        // If the decoration worker holds a shared lock (50-180ms decoration
        // turn), we skip this chunk this tick and retry next tick.
        std::unique_lock<std::shared_mutex> chunksLk(m_chunksMutex, std::try_to_lock);
        if (!chunksLk.owns_lock()) {
            // Decoration worker is busy — skip, retry next tick.
            // Don't consume the future; it'll still be ready next tick.
            ++it;
            continue;
        }
        // Now safe to consume the future — we hold the lock.
        GeneratedChunk generated = it->future.get();
        std::unique_ptr<LevelChunk> chunk = std::move(generated.chunk);
        bool consumed = false;
        if (chunk) {
            ChunkPos cp = chunk->pos();
            if (std::abs(cp.x - px) <= RADIUS && std::abs(cp.z - pz) <= RADIUS) {
                auto key = chunkKey(cp);
                if (m_chunks.find(key) == m_chunks.end()) {
                    LevelChunk* ptr = chunk.get();
                    m_chunks[key] = std::move(chunk);
                    levelgen::feature::freezeWorldgenHeights(*ptr, &generated.genMarks);
                    ptr->meshDirty = true;
                    static constexpr int kCardinal[4][2] = { {1,0}, {-1,0}, {0,1}, {0,-1} };
                    for (const auto& d : kCardinal) {
                        auto itN = m_chunks.find(chunkKey({ cp.x + d[0], cp.z + d[1] }));
                        if (itN != m_chunks.end() && itN->second) {
                            itN->second->meshDirty = true;
                        }
                    }
                    ++integrated;
                    consumed = true;
                }
            }
        }
        // Always clean up the task — the future was consumed by get() above.
        m_queuedChunks.erase(chunkKey(it->pos));
        it = m_generationTasks.erase(it);
    }

    // 2b. Submit decoration requests to the worker thread.
    // Decoration is now off the main thread (Phase 2 of Option B refactor),
    // so we no longer need the 750ms idle gate or the 150ms throttle — the
    // worker runs continuously and the main thread never blocks on it.
    // We still throttle SUBMISSION to one chunk per tick to avoid flooding
    // the worker queue (the worker takes hundreds of ms per chunk anyway).
    {
        std::vector<ChunkPos> ready;
        {
            // Shared lock — read iteration to find decoration candidates.
            std::shared_lock<std::shared_mutex> lk(m_chunksMutex);
            ready.reserve(m_chunks.size());
            for (const auto& [key, chunk] : m_chunks)
                if (chunk && !chunk->decorated) ready.push_back(chunk->pos());
        }
        std::sort(ready.begin(), ready.end(), [px, pz](ChunkPos a, ChunkPos b) {
            return (a.x - px) * (a.x - px) + (a.z - pz) * (a.z - pz)
                 < (b.x - px) * (b.x - px) + (b.z - pz) * (b.z - pz);
        });
        // Submit up to 8 decoration requests per tick — with multiple parallel
        // decoration workers, we can feed the queue faster.
        int decoSubmitted = 0;
        for (ChunkPos cp : ready) {
            LevelChunk* c = getChunk(cp);
            if (!c || c->decorated) continue;
            tryDecorate(cp);
            if (++decoSubmitted >= 8) break;
        }
    }

    // 2c. Poll the decoration worker for completed chunks and mark their 3x3
    // neighborhoods for re-meshing. This is the only main-thread work after
    // the worker finishes — a handful of atomic-flag sets, very cheap.
    pollDecorationDone();

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
            
            if (m_queuedChunks.find(chunkKey(posKey)) != m_queuedChunks.end()) {
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
    // Scale the generation queue budget with how many chunks are missing.
    // During initial load, submit many chunks so all gen threads stay busy.
    // During steady-state, keep it low to avoid frame stalls.
    int queueBudget;
    if (loadedCount < targetCount / 2) {
        queueBudget = 32;  // aggressive: flood the gen pool during initial load
    } else if (loadedCount < targetCount * 3 / 4) {
        queueBudget = 16;  // moderate
    } else {
        queueBudget = 8;   // conservative steady-state
    }
    for (const auto& cand : candidates) {
        if (!m_threadPool) break;
        
        m_queuedChunks.insert(chunkKey(cand.pos));
        // Build the per-chunk Beardifier on THIS (main) thread — the structure
        // runtime is single-threaded — and hand it to the worker by shared_ptr.
        // compute() is const, so the worker can read it concurrently.
        auto beard = std::make_shared<levelgen::Beardifier>(buildChunkBeardifier(cand.pos));
        m_generationTasks.push_back({
            cand.pos,
            m_threadPool->enqueue([pos = cand.pos, seed = m_worldSeed, beard]() -> GeneratedChunk {
                GeneratedChunk out;
                out.chunk = std::make_unique<LevelChunk>(pos);
                struct ThreadGeneratorCache {
                    uint64_t seed = 0;
                    std::unique_ptr<levelgen::NoiseBasedChunkGenerator> generator;
                };
                thread_local ThreadGeneratorCache cache;
                if (!cache.generator || cache.seed != seed) {
                    cache.seed = seed;
                    cache.generator = std::make_unique<levelgen::NoiseBasedChunkGenerator>(seed);
                }
                levelgen::NoiseBasedChunkGenerator& generator = *cache.generator;
                auto gt0 = std::chrono::steady_clock::now();
                generator.fillFromNoise(*out.chunk, &out.genMarks,
                                        beard->isEmpty() ? nullptr : beard.get());
                auto gt1 = std::chrono::steady_clock::now();
                generator.buildSurface(*out.chunk);
                auto gt2 = std::chrono::steady_clock::now();
                generator.applyCarvers(*out.chunk, &out.genMarks);
                auto gt3 = std::chrono::steady_clock::now();
                double noiseMs = std::chrono::duration<double, std::milli>(gt1 - gt0).count();
                double surfMs = std::chrono::duration<double, std::milli>(gt2 - gt1).count();
                double carveMs = std::chrono::duration<double, std::milli>(gt3 - gt2).count();
                double genTotal = std::chrono::duration<double, std::milli>(gt3 - gt0).count();
                if (genTotal > 30.0) {
                    MC_LOG_WARN("SLOW GEN ({},{}) total={:.1f}ms (noise={:.1f} surf={:.1f} carve={:.1f})",
                                pos.x, pos.z, genTotal, noiseMs, surfMs, carveMs);
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
