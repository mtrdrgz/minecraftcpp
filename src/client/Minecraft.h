#pragma once
#include "../platform/Window.h"
#include "../render/IRenderDevice.h"
#include "../network/Connection.h"
#include "../world/level/chunk/LevelChunk.h"
#include "../world/level/levelgen/Beardifier.h"
#include "../world/entity/Entity.h"
#include "player/LocalPlayer.h"
#include "../render/gui/GuiGraphics.h"
#include "../render/gui/Font.h"
#include "../gui/Gui.h"
#include "../gui/screens/Screen.h"
#include "../audio/SoundManager.h"
#include "../core/ThreadPool.h"
#include "Options.h"
#include <unordered_map>
#include <unordered_set>
#include <deque>
#include <condition_variable>
#include <memory>
#include <shared_mutex>
#include <string>
#include <atomic>
#include <thread>
#include <future>
#include <optional>
#include <chrono>


namespace mc {

namespace levelgen {
    class NoiseBasedChunkGenerator;
    namespace feature { class BiomeFeatures; }
}
namespace block { class BlockTags; }
namespace render { class PanoramaRenderer; }

struct PlayerInfo {
    UUID profileId{};
    std::string name;
    bool listed = false;
    int32_t latency = 0;
    int32_t gameMode = 0;
    bool showHat = true;
    int32_t listOrder = 0;
};

class Minecraft {
public:
    Minecraft(Window* window, render::IRenderDevice* device);
    ~Minecraft();

    void connectToServer(std::string_view host, uint16_t port,
                         std::string_view username);
    void startLocalGame(uint64_t seed = 0, int spawnX = 0, int spawnZ = 0,
                        std::optional<int> spawnY = std::nullopt);
    void startLocalGameFast(uint64_t seed = 0, int spawnX = 0, int spawnZ = 0,
                            std::optional<int> spawnY = std::nullopt);
    void disconnect();

    void tick();
    void render(float partialTick);

    bool isConnected() const { return m_connection && m_connection->isConnected(); }
    bool isInGame()    const { return m_inGame; }

    PlayerState&       player()       { return m_localPlayer.state(); }
    const PlayerState& player() const { return m_localPlayer.state(); }

    // Debug teleport. The free-fly camera (LevelRenderer::m_camPos) is the real
    // position authority — it overwrites player() every frame — so setting
    // player().x/z alone does nothing. The debug overlay calls requestTeleport();
    // the camera consumes it once per frame and jumps m_camPos there. Streaming
    // then loads chunks around the new position on its own (no manual chunk clear,
    // which would race the decoration worker).
    void requestTeleport(double x, double y, double z) {
        m_pendingTpX = x; m_pendingTpY = y; m_pendingTpZ = z; m_hasPendingTeleport = true;
    }
    bool consumeTeleport(double& x, double& y, double& z) {
        if (!m_hasPendingTeleport) return false;
        x = m_pendingTpX; y = m_pendingTpY; z = m_pendingTpZ;
        m_hasPendingTeleport = false;
        return true;
    }

    Window*            window()       { return m_window; }
    render::IRenderDevice* device()   { return m_device; }

    LevelChunk* getChunk(ChunkPos pos);
    LevelChunk* getOrCreateChunk(ChunkPos pos);
    void        unloadChunk(ChunkPos pos);
    // Restore any chunks within the radius that are sitting in m_chunkCache back
    // into m_chunks (verbatim, no regeneration/re-decoration). Returns nothing;
    // restored chunks are marked meshDirty. Main thread only.
    void        restoreCachedChunksInRadius(int px, int pz, int radius);
    const auto& chunks() const { return m_chunks; }

    // Guards chunk BLOCK DATA against concurrent read/write between the decoration
    // worker (writes blocks cross-chunk during a turn) and the render thread's
    // mesh-snapshot copy (reads/copies a chunk's blocks). Without this, copying a
    // LevelChunk while decoration mutates its PalettedContainer (palette realloc)
    // is a data race → heap corruption → crash on fast GPUs (where meshing runs
    // hot). m_chunksMutex only protects the MAP structure, not per-chunk blocks.
    // Lock order is deadlock-free: the mesher takes this then only ever takes
    // m_chunksMutex SHARED (compatible with the decoration worker's shared hold);
    // exclusive m_chunksMutex holders are main-thread-only and cannot run while the
    // main-thread mesher runs.
    std::mutex& chunkWriteMutex() { return m_chunkWriteMutex; }

    // Biome id at QUART coordinates from the local generator's biome source. Empty
    // if no local generator. Used on the main thread to build per-chunk biome
    // snapshots for the mesher (the underlying noise router has mutable caches, so
    // it must not be called concurrently from mesh workers).
    std::string getNoiseBiomeName(int quartX, int quartY, int quartZ) const;
    const std::string& dataMinecraftDir() const { return m_dataMinecraftDir; }

    render::GuiGraphics* guiGraphics() { return m_guiGraphics.get(); }
    render::Font*        font()        { return m_font.get(); }
    gui::Gui*           gui()         { return m_gui.get(); }
    gui::Screen*        screen()      { return m_currentScreen.get(); }
    audio::SoundManager* soundManager() { return m_soundManager.get(); }
    
    void setScreen(std::unique_ptr<gui::Screen> screen);
    void openTitleScreen();
    void openOptionsScreen();
    void openPauseScreen();
    void resumeGame();

    GameOptions& options() { return m_options; }
    int guiScale() const;
    int guiScaledWidth() const;
    int guiScaledHeight() const;
    double guiMouseX() const;
    double guiMouseY() const;
    void resizeGui();

    void renderPanorama(render::ICommandList* cmd, int w, int h, float dtSeconds);
    render::ITexture* panoramaOverlay();
    bool panoramaLoaded() const;
    // Slider widget textures — exposed so PauseScreen/OptionsScreen can pass them
    // to the OptionsSubScreen they create (which forwards to Slider widgets).
    render::ITexture* sliderTrackTex()    { return m_sliderTrackTex; }
    render::ITexture* sliderHandleTex()   { return m_sliderHandleTex; }
    render::ITexture* sliderHandleHlTex() { return m_sliderHandleHlTex; }

private:
    friend struct DebugOverlay;  // Development tool — not part of 1:1 port
    void handlePackets();
    void handlePlayPacket(int32_t id, net::PacketBuffer& buf);
    void sendLoginSequence(std::string_view username);
    void applyPlayerInfoToEntity(const PlayerInfo& info);
    void updateLocalChunks();

    void ensureWorldgenData();
    void decorateChunk(LevelChunk& chunk);
    void tryDecorate(ChunkPos cp);
    void runStructures(ChunkPos active);
    // Phase 2 of Option B refactor: decoration runs on a worker thread.
    void decorationWorkerLoop();
    void pollDecorationDone();
    // Build the per-chunk Beardifier on the MAIN thread (the structure runtime is
    // single-threaded) so it can be passed to fillFromNoise on any worker. Empty
    // when no terrain-adapting structure is near `pos`.
    mc::levelgen::Beardifier buildChunkBeardifier(ChunkPos pos);

    Window*                m_window  = nullptr;
    render::IRenderDevice* m_device  = nullptr;

    std::unique_ptr<net::Connection> m_connection;
    client::LocalPlayer m_localPlayer;
    bool        m_inGame = false;
    int32_t     m_pendingTeleportId = -1;
    uint32_t    m_tickCounter = 0;
    uint64_t    m_worldSeed = 0;

    // Debug-overlay teleport request, consumed by LevelRenderer::updateCamera.
    bool        m_hasPendingTeleport = false;
    double      m_pendingTpX = 0.0, m_pendingTpY = 0.0, m_pendingTpZ = 0.0;

    std::unique_ptr<levelgen::NoiseBasedChunkGenerator> m_localGenerator;
    std::unique_ptr<levelgen::feature::BiomeFeatures>    m_biomeFeatures;
    std::unique_ptr<block::BlockTags>                    m_blockTags;
    std::string m_worldgenDir;
    std::string m_dataMinecraftDir;   // …/data/minecraft on disk ("" when not found)
    bool        m_worldgenReady = false;
    bool        m_worldgenTried = false;

    std::unique_ptr<ThreadPool>          m_threadPool;
    // A generated chunk plus the noise+carver postprocess marks the generator
    // collected for it (consumed by the decoration context's freeze step).
    struct GeneratedChunk {
        std::unique_ptr<LevelChunk> chunk;
        std::vector<BlockPos> genMarks;
    };
    struct ChunkGenTask {
        ChunkPos pos;
        std::future<GeneratedChunk> future;
    };
    std::vector<ChunkGenTask>            m_generationTasks;
    std::unordered_set<int64_t>          m_queuedChunks;

    std::unordered_map<int64_t, std::unique_ptr<LevelChunk>> m_chunks;
    std::unordered_map<UUID, PlayerInfo, UUIDHash> m_playerInfo;

    // In-memory chunk persistence cache. Chunks that leave the active radius are
    // kept here (their LevelChunk is MOVED out of m_chunks, not destroyed) so that
    // revisiting restores them VERBATIM instead of regenerating + re-decorating.
    // This matches vanilla, where chunks are persisted to disk and never re-run:
    // it is what prevents cross-chunk feature spill (trees overhanging borders,
    // fossils spanning ±16, structure pieces) from being placed AGAIN every time a
    // chunk is regenerated — the cause of the "absurd/artificial" feature density
    // near streaming edges. Mutated only on the main thread; moving a chunk between
    // this map and m_chunks keeps its LevelChunk* heap pointer valid. m_chunksMutex
    // (exclusive) guards the m_chunks side of every move so the decoration worker
    // never sees a half-moved map.
    std::unordered_map<int64_t, std::unique_ptr<LevelChunk>> m_chunkCache;
    std::deque<int64_t>                  m_chunkCacheOrder;   // FIFO eviction (oldest front)
    // ~100 KB/chunk typical → ~100-150 MB cap. Covers a recently-visited window far
    // larger than the active RADIUS=6 (169 chunks), so normal wandering/backtracking
    // never re-decorates. Bump if more RAM is available and you roam farther.
    static constexpr std::size_t         kChunkCacheCapacity = 1024;

    std::unique_ptr<render::GuiGraphics>     m_guiGraphics;
    std::unique_ptr<render::PanoramaRenderer> m_panorama;
    std::unique_ptr<render::Font>        m_font;

    render::ITexture* m_logoTex = nullptr;
    render::ITexture* m_editionTex = nullptr;
    render::ITexture* m_dirtTex = nullptr;
    render::ITexture* m_btnTex = nullptr;
    render::ITexture* m_btnHlTex = nullptr;
    render::ITexture* m_langTex = nullptr;
    render::ITexture* m_accessTex = nullptr;
    // Slider widget textures (widget/slider.png + widget/slider_handle.png +
    // widget/slider_handle_highlighted.png). Loaded once at first render and
    // passed to every OptionsSubScreen so sliders render with their real track
    // + handle instead of falling back to the button texture + grey fill.
    render::ITexture* m_sliderTrackTex = nullptr;
    render::ITexture* m_sliderHandleTex = nullptr;
    render::ITexture* m_sliderHandleHlTex = nullptr;
    std::string       m_splashText;
    GameOptions       m_options;
    int               m_cachedGuiScale = 1;
    int               m_cachedGuiWidth = 0;
    int               m_cachedGuiHeight = 0;
    std::unique_ptr<gui::Gui>           m_gui;
    std::unique_ptr<audio::SoundManager> m_soundManager;
    std::unique_ptr<gui::Screen>        m_currentScreen;

    // Throttle main-thread decoration — each chunk is hundreds of ms of CPU.
    std::chrono::steady_clock::time_point m_lastDecorateStart{};
    std::chrono::steady_clock::time_point m_lastLocalMovement{};
    double m_lastStreamPlayerX = 0.0;
    double m_lastStreamPlayerY = 0.0;
    double m_lastStreamPlayerZ = 0.0;
    bool   m_haveLastStreamPlayerPos = false;

    // ── Decoration worker (Phase 2 of Option B refactor) ──────────────────
    // Decoration is moved OFF the main thread to eliminate the hundreds-of-ms
    // stutter each tryDecorate() call caused. The main thread now submits
    // "decorate (cx,cz)" requests to m_decorationQueue; the single decoration
    // worker pops them and runs engineDecorateChunk. Results are integrated
    // back on the main thread via m_decorationDone.
    //
    // Chunk-map safety: m_chunksMutex is a shared_mutex. Unload takes it
    // exclusively (no decoration running on those chunks). The decoration
    // worker takes it shared for the duration of ONE decoration turn, so it
    // can read all 9 chunks (3x3) without them being unloaded mid-turn.
    // Main-thread reads (render, player physics) take it shared too — they
    // don't block each other or the worker.
    std::shared_mutex                      m_chunksMutex;
    std::mutex                             m_chunkWriteMutex;  // see chunkWriteMutex()
    std::vector<ChunkPos>                  m_decorationQueue;
    std::mutex                             m_decorationQueueMutex;
    std::condition_variable                m_decorationQueueCv;
    std::thread                            m_decorationThread;
    std::atomic<bool>                      m_decorationStop{false};
    // Chunks finished by the worker, awaiting main-thread integration
    // (meshDirty marking on the 3x3 neighborhood, etc.)
    std::vector<ChunkPos>                  m_decorationDone;
    std::mutex                             m_decorationDoneMutex;
    // Per-request: the worker sets this when its decoration turn finishes.
    // The main thread polls and integrates.

    static int64_t chunkKey(ChunkPos p) {
        return ((int64_t)(uint32_t)p.x << 32) | (uint32_t)p.z;
    }
};

} // namespace mc
