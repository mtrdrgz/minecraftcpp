#include "LocalPlayer.h"
#include "../../network/protocol/game/PlayPackets.h"

#include <windows.h>

#include <cmath>

namespace mc::client {

namespace {
    constexpr int32_t POSITION_REMINDER_INTERVAL = 20;
    constexpr double MIN_MOVE_DELTA_SQUARED = 2.0E-4 * 2.0E-4;

    bool relativeX(int32_t relatives) { return (relatives & (1 << 0)) != 0; }
    bool relativeY(int32_t relatives) { return (relatives & (1 << 1)) != 0; }
    bool relativeZ(int32_t relatives) { return (relatives & (1 << 2)) != 0; }
    bool relativeYaw(int32_t relatives) { return (relatives & (1 << 3)) != 0; }
    bool relativePitch(int32_t relatives) { return (relatives & (1 << 4)) != 0; }

    void sendPacket(net::Connection* connection, const net::Packet& packet) {
        net::PacketBuffer out;
        net::encodePacket(packet, out);
        connection->sendPacket(out);
    }
}

void LocalPlayer::reset() {
    *this = LocalPlayer{};
}

void LocalPlayer::applyLogin(int32_t entityId, uint8_t gameMode) {
    m_state.entityId = entityId;
    m_state.gameMode = gameMode;
}

void LocalPlayer::applyServerPosition(double x, double y, double z,
                                      float yaw, float pitch, int32_t relatives) {
    m_state.x = relativeX(relatives) ? m_state.x + x : x;
    m_state.y = relativeY(relatives) ? m_state.y + y : y;
    m_state.z = relativeZ(relatives) ? m_state.z + z : z;
    m_state.yaw = relativeYaw(relatives) ? m_state.yaw + yaw : yaw;
    m_state.pitch = relativePitch(relatives) ? m_state.pitch + pitch : pitch;
}

void LocalPlayer::tick(net::Connection* connection, const Window* window) {
    if (!connection || !connection->isConnected() || connection->state != net::ConnectionState::Play) {
        return;
    }
    if (m_state.entityId < 0) {
        return;
    }

    readInput(window);
    sendInputIfNeeded(connection);
    sendPosition(connection);
}

void LocalPlayer::readInput(const Window* window) {
    if (!window) {
        m_input = {};
        return;
    }

    m_input.forward  = window->isKeyDown('W') || window->isKeyDown(VK_UP);
    m_input.backward = window->isKeyDown('S') || window->isKeyDown(VK_DOWN);
    m_input.left     = window->isKeyDown('A') || window->isKeyDown(VK_LEFT);
    m_input.right    = window->isKeyDown('D') || window->isKeyDown(VK_RIGHT);
    m_input.jump     = window->isKeyDown(VK_SPACE);
    m_input.shift    = window->isKeyDown(VK_SHIFT);
    m_input.sprint   = window->isKeyDown(VK_CONTROL);
}

void LocalPlayer::sendInputIfNeeded(net::Connection* connection) {
    if (m_input == m_lastSentInput) {
        return;
    }

    net::C2SPlayerInputPacket packet;
    packet.forward = m_input.forward;
    packet.backward = m_input.backward;
    packet.left = m_input.left;
    packet.right = m_input.right;
    packet.jump = m_input.jump;
    packet.shift = m_input.shift;
    packet.sprint = m_input.sprint;
    sendPacket(connection, packet);
    m_lastSentInput = m_input;
}

void LocalPlayer::sendIsSprintingIfNeeded(net::Connection* connection) {
    if (m_input.sprint == m_wasSprinting) {
        return;
    }

    net::C2SPlayerCommandPacket packet;
    packet.entityId = m_state.entityId;
    packet.action = m_input.sprint
        ? net::C2SPlayerCommandPacket::START_SPRINTING
        : net::C2SPlayerCommandPacket::STOP_SPRINTING;
    sendPacket(connection, packet);
    m_wasSprinting = m_input.sprint;
}

void LocalPlayer::sendPosition(net::Connection* connection) {
    sendIsSprintingIfNeeded(connection);

    const double deltaX = m_state.x - m_xLast;
    const double deltaY = m_state.y - m_yLast;
    const double deltaZ = m_state.z - m_zLast;
    const double deltaYaw = static_cast<double>(m_state.yaw - m_yawLast);
    const double deltaPitch = static_cast<double>(m_state.pitch - m_pitchLast);

    ++m_positionReminder;
    const bool move = deltaX * deltaX + deltaY * deltaY + deltaZ * deltaZ > MIN_MOVE_DELTA_SQUARED
                   || m_positionReminder >= POSITION_REMINDER_INTERVAL;
    const bool rot = deltaYaw != 0.0 || deltaPitch != 0.0;

    if (move && rot) {
        net::C2SPlayerMovePositionRotationPacket packet;
        packet.x = m_state.x;
        packet.y = m_state.y;
        packet.z = m_state.z;
        packet.yaw = m_state.yaw;
        packet.pitch = m_state.pitch;
        packet.onGround = m_state.onGround;
        packet.horizontalCollision = m_state.horizontalCollision;
        sendPacket(connection, packet);
    } else if (move) {
        net::C2SPlayerMovePositionPacket packet;
        packet.x = m_state.x;
        packet.y = m_state.y;
        packet.z = m_state.z;
        packet.onGround = m_state.onGround;
        packet.horizontalCollision = m_state.horizontalCollision;
        sendPacket(connection, packet);
    } else if (rot) {
        net::C2SPlayerMoveRotationPacket packet;
        packet.yaw = m_state.yaw;
        packet.pitch = m_state.pitch;
        packet.onGround = m_state.onGround;
        packet.horizontalCollision = m_state.horizontalCollision;
        sendPacket(connection, packet);
    } else if (m_lastOnGround != m_state.onGround || m_lastHorizontalCollision != m_state.horizontalCollision) {
        net::C2SPlayerMoveStatusOnlyPacket packet;
        packet.onGround = m_state.onGround;
        packet.horizontalCollision = m_state.horizontalCollision;
        sendPacket(connection, packet);
    }

    if (move) {
        m_xLast = m_state.x;
        m_yLast = m_state.y;
        m_zLast = m_state.z;
        m_positionReminder = 0;
    }

    if (rot) {
        m_yawLast = m_state.yaw;
        m_pitchLast = m_state.pitch;
    }

    m_lastOnGround = m_state.onGround;
    m_lastHorizontalCollision = m_state.horizontalCollision;
}

} // namespace mc::client
