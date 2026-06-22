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
#include <memory>
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

    Window*            window()       { return m_window; }
    render::IRenderDevice* device()   { return m_device; }

    LevelChunk* getChunk(ChunkPos pos);
    LevelChunk* getOrCreateChunk(ChunkPos pos);
    void        unloadChunk(ChunkPos pos);
    const auto& chunks() const { return m_chunks; }

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

    static int64_t chunkKey(ChunkPos p) {
        return ((int64_t)(uint32_t)p.x << 32) | (uint32_t)p.z;
    }
};

} // namespace mc
