#pragma once
#include "../platform/Window.h"
#include "../render/IRenderDevice.h"
#include "../network/Connection.h"
#include "../world/level/chunk/LevelChunk.h"
#include "../world/entity/Entity.h"
#include "player/LocalPlayer.h"
#include "../render/gui/GuiGraphics.h"
#include "../render/gui/Font.h"
#include "../gui/Gui.h"
#include "../gui/screens/Screen.h"
#include "../audio/SoundManager.h"
#include "../core/ThreadPool.h"
#include <unordered_map>
#include <memory>
#include <string>
#include <atomic>
#include <thread>
#include <future>


namespace mc {

// Forward declarations — kept out of the header to avoid pulling the worldgen
// decoration/placement headers (and their VerticalAnchor clash) into every TU
// that includes Minecraft.h. Stored as unique_ptr, built in Minecraft.cpp.
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

// Port of net.minecraft.client.Minecraft (main game class)
class Minecraft {
public:
    Minecraft(Window* window, render::IRenderDevice* device);
    ~Minecraft();

    void connectToServer(std::string_view host, uint16_t port,
                         std::string_view username);
    void startLocalGame(uint64_t seed = 0);
    void disconnect();

    // Main tick — called once per frame
    void tick();

    // Render current state
    void render(float partialTick);

    bool isConnected() const { return m_connection && m_connection->isConnected(); }
    bool isInGame()    const { return m_inGame; }

    PlayerState&       player()       { return m_localPlayer.state(); }
    const PlayerState& player() const { return m_localPlayer.state(); }

    Window*            window()       { return m_window; }
    render::IRenderDevice* device()   { return m_device; }

    // Chunk storage
    LevelChunk* getChunk(ChunkPos pos);
    LevelChunk* getOrCreateChunk(ChunkPos pos);
    void        unloadChunk(ChunkPos pos);
    const auto& chunks() const { return m_chunks; }

    // GUI System
    render::GuiGraphics* guiGraphics() { return m_guiGraphics.get(); }
    render::Font*        font()        { return m_font.get(); }
    gui::Gui*           gui()         { return m_gui.get(); }
    gui::Screen*        screen()      { return m_currentScreen.get(); }
    audio::SoundManager* soundManager() { return m_soundManager.get(); }
    
    void setScreen(std::unique_ptr<gui::Screen> screen);
    // Centralised screen construction (the GUI textures are owned here and reused).
    void openTitleScreen();
    void openOptionsScreen();

    // Title panorama background (rotating cubemap). renderPanorama draws it to cmd;
    // panoramaOverlay/panoramaLoaded let the title screen blit the overlay + fall back
    // to the dirt background if the panorama art isn't available.
    void renderPanorama(render::ICommandList* cmd, int w, int h, float dtSeconds);
    render::ITexture* panoramaOverlay();
    bool panoramaLoaded() const;

private:
    void handlePackets();
    void handlePlayPacket(int32_t id, net::PacketBuffer& buf);
    void sendLoginSequence(std::string_view username);
    void applyPlayerInfoToEntity(const PlayerInfo& info);
    void updateLocalChunks();

    // Worldgen decoration (trees + vegetation). Loads the data-driven biome
    // feature lists + block tags once from the local 26.1.2/data tree, then runs
    // the faithful applyBiomeDecoration() pass over a freshly generated chunk.
    // No-op (logs once) if the data tree can't be located.
    void ensureWorldgenData();
    void decorateChunk(LevelChunk& chunk);

    // Decorate + place structures for a chunk, but ONLY once and ONLY after all 8
    // neighbours are loaded — guarantees cross-chunk feature writes (trees, etc.)
    // land in real chunks instead of being clipped at the border. No-op otherwise.
    void tryDecorate(ChunkPos cp);

    // Structure generation pass for one chunk. Builds a cross-chunk block writer
    // over the currently loaded chunks and runs the spaced-structure + dungeon
    // placement. No-op if the local generator / worldgen data isn't ready.
    void runStructures(ChunkPos active);


    Window*                m_window  = nullptr;
    render::IRenderDevice* m_device  = nullptr;

    std::unique_ptr<net::Connection> m_connection;
    client::LocalPlayer m_localPlayer;
    bool        m_inGame = false;
    int32_t     m_pendingTeleportId = -1;
    uint32_t    m_tickCounter = 0;
    uint64_t    m_worldSeed = 0;


    // Worldgen decoration state (see ensureWorldgenData / decorateChunk).
    std::unique_ptr<levelgen::NoiseBasedChunkGenerator> m_localGenerator;
    std::unique_ptr<levelgen::feature::BiomeFeatures>    m_biomeFeatures;
    std::unique_ptr<block::BlockTags>                    m_blockTags;
    std::string m_worldgenDir;
    bool        m_worldgenReady = false;
    bool        m_worldgenTried = false;

    std::unique_ptr<ThreadPool>          m_threadPool;
    struct ChunkGenTask {
        ChunkPos pos;
        std::future<std::unique_ptr<LevelChunk>> future;
    };
    std::vector<ChunkGenTask>            m_generationTasks;

    std::unordered_map<int64_t, std::unique_ptr<LevelChunk>> m_chunks;
    std::unordered_map<UUID, PlayerInfo, UUIDHash> m_playerInfo;

    std::unique_ptr<render::GuiGraphics>     m_guiGraphics;
    std::unique_ptr<render::PanoramaRenderer> m_panorama;
    std::unique_ptr<render::Font>        m_font;

    // GUI textures (loaded once from embedded resources; reused to (re)build screens).
    render::ITexture* m_logoTex = nullptr;
    render::ITexture* m_editionTex = nullptr;
    render::ITexture* m_dirtTex = nullptr;
    render::ITexture* m_btnTex = nullptr;
    render::ITexture* m_btnHlTex = nullptr;
    render::ITexture* m_langTex = nullptr;
    render::ITexture* m_accessTex = nullptr;
    std::string       m_splashText;
    std::unique_ptr<gui::Gui>           m_gui;
    std::unique_ptr<audio::SoundManager> m_soundManager;
    std::unique_ptr<gui::Screen>        m_currentScreen;

    static int64_t chunkKey(ChunkPos p) {
        return ((int64_t)(uint32_t)p.x << 32) | (uint32_t)p.z;
    }
};

} // namespace mc
