#pragma once
#include "../../network/Connection.h"
#include "../../platform/Window.h"

#include <cstdint>

namespace mc {

struct PlayerState {
    double x = 0, y = 64, z = 0;
    float  yaw = 0, pitch = 0;
    int32_t entityId = -1;
    uint8_t gameMode = 0;
    bool    onGround = false;
    bool    horizontalCollision = false;
};

namespace client {

// Port of net.minecraft.world.entity.player.Input.
struct LocalPlayerInput {
    bool forward = false;
    bool backward = false;
    bool left = false;
    bool right = false;
    bool jump = false;
    bool shift = false;
    bool sprint = false;

    bool operator==(const LocalPlayerInput&) const = default;
};

// Minimal client-side port of net.minecraft.client.player.LocalPlayer.
//
// This owns the local player's network movement state and mirrors the Java
// sendPosition() packet decision tree. Physics, inventory, sounds, permissions,
// and interaction-heavy methods remain deferred until their backing systems
// exist.
class LocalPlayer {
public:
    PlayerState& state() { return m_state; }
    const PlayerState& state() const { return m_state; }

    void reset();
    void applyLogin(int32_t entityId, uint8_t gameMode);
    void applyServerPosition(double x, double y, double z,
                             float yaw, float pitch, int32_t relatives);
    void tick(net::Connection* connection, const Window* window);

private:
    void readInput(const Window* window);
    void sendInputIfNeeded(net::Connection* connection);
    void sendIsSprintingIfNeeded(net::Connection* connection);
    void sendPosition(net::Connection* connection);

    PlayerState m_state;
    LocalPlayerInput m_input;
    LocalPlayerInput m_lastSentInput;

    double m_xLast = 0.0;
    double m_yLast = 0.0;
    double m_zLast = 0.0;
    float  m_yawLast = 0.0f;
    float  m_pitchLast = 0.0f;
    bool   m_lastOnGround = false;
    bool   m_lastHorizontalCollision = false;
    bool   m_wasSprinting = false;
    int32_t m_positionReminder = 0;
};

} // namespace client
} // namespace mc
