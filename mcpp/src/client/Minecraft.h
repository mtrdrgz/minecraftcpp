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

private:
    void handlePackets();
    void handlePlayPacket(int32_t id, net::PacketBuffer& buf);
    void sendLoginSequence(std::string_view username);
    void applyPlayerInfoToEntity(const PlayerInfo& info);
    void updateLocalChunks();


    Window*                m_window  = nullptr;
    render::IRenderDevice* m_device  = nullptr;

    std::unique_ptr<net::Connection> m_connection;
    client::LocalPlayer m_localPlayer;
    bool        m_inGame = false;
    int32_t     m_pendingTeleportId = -1;
    uint32_t    m_tickCounter = 0;
    uint64_t    m_worldSeed = 0;


    std::unique_ptr<ThreadPool>          m_threadPool;
    struct ChunkGenTask {
        ChunkPos pos;
        std::future<std::unique_ptr<LevelChunk>> future;
    };
    std::vector<ChunkGenTask>            m_generationTasks;

    std::unordered_map<int64_t, std::unique_ptr<LevelChunk>> m_chunks;
    std::unordered_map<UUID, PlayerInfo, UUIDHash> m_playerInfo;

    std::unique_ptr<render::GuiGraphics> m_guiGraphics;
    std::unique_ptr<render::Font>        m_font;
    std::unique_ptr<gui::Gui>           m_gui;
    std::unique_ptr<audio::SoundManager> m_soundManager;
    std::unique_ptr<gui::Screen>        m_currentScreen;

    static int64_t chunkKey(ChunkPos p) {
        return ((int64_t)(uint32_t)p.x << 32) | (uint32_t)p.z;
    }
};

} // namespace mc
