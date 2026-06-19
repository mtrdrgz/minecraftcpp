#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace mc::client {

// Port of com.mojang.blaze3d.platform.InputConstants.Key — identifies one
// input source: a keyboard virtual-key, a mouse button, or "unknown" (unbound).
struct InputKey {
    enum Type { Keyboard, MouseButton, Unknown };

    Type    type = Unknown;
    int32_t code = 0;       // VK_xxx for Keyboard; 0=LMB, 1=RMB, 2=MMB for MouseButton

    bool operator==(const InputKey&) const = default;

    static InputKey keyboard(int vk) { return {Keyboard, vk}; }
    static InputKey mouse(int btn)   { return {MouseButton, btn}; }
    static InputKey unknown()        { return {Unknown, 0}; }
};

// Port of net.minecraft.client.KeyMapping.
// A KeyMapping holds the name, default & current bindings, category, and
// pressed-state for one configurable input. The static registry mirrors
// KeyMapping.ALL / KeyMapping.MAP from the Java source.
class KeyMapping {
public:
    KeyMapping(std::string name, InputKey defaultKey, std::string category);

    const std::string& name()     const { return m_name; }
    const std::string& category() const { return m_category; }
    InputKey defaultKey()         const { return m_default; }
    InputKey currentKey()         const { return m_current; }

    void setKey(InputKey k);
    void resetToDefault() { m_current = m_default; }

    bool isDefault() const { return m_current == m_default; }
    bool isUnbound() const { return m_current.type == InputKey::Unknown; }

    bool isDown()      const { return m_isDown; }
    bool consumeClick();      // pop one pending click

    // Match the Java setDown(boolean) / click() / release() primitives.
    void setDown(bool down) { m_isDown = down; }
    void click()            { ++m_clickCount; }
    void release();

    // ── Static input dispatcher ──────────────────────────────────────────────
    // Called from the Window event loop. Updates every KeyMapping bound to
    // this physical input: sets its pressed state, and on a transition to
    // pressed also bumps the click count (matches KeyMapping.set + click).
    static void setAll(InputKey k, bool pressed);

    // Release every KeyMapping (clears isDown and clears pending clicks).
    static void releaseAll();

    // Lookup by registered name, e.g. "key.forward". Returns nullptr if missing.
    static KeyMapping* find(std::string_view name);

private:
    std::string m_name;
    std::string m_category;
    InputKey    m_default;
    InputKey    m_current;
    bool        m_isDown     = false;
    int         m_clickCount = 0;

    static std::vector<KeyMapping*>& all();
};

// ── Default key mappings ─────────────────────────────────────────────────────
// Pointers populated by initKeyMappings(). These mirror the public KeyMapping
// fields on Options.java (keyUp, keyDown, ...). Values default to the Win32
// VK_ codes equivalent to the GLFW key codes used in the Java source.
namespace keys {
    extern KeyMapping* FORWARD;      // W
    extern KeyMapping* BACK;         // S
    extern KeyMapping* LEFT;         // A
    extern KeyMapping* RIGHT;        // D
    extern KeyMapping* JUMP;         // VK_SPACE
    extern KeyMapping* SNEAK;        // VK_SHIFT
    extern KeyMapping* SPRINT;       // VK_CONTROL
    extern KeyMapping* ATTACK;       // mouse 0 (LMB)
    extern KeyMapping* USE;          // mouse 1 (RMB)
    extern KeyMapping* PICK_ITEM;    // mouse 2 (MMB)
    extern KeyMapping* DROP;         // Q
    extern KeyMapping* INVENTORY;    // E
    extern KeyMapping* SWAP_OFFHAND; // F
    extern KeyMapping* CHAT;         // T
    extern KeyMapping* COMMAND;      // / (VK_OEM_2)
    extern KeyMapping* PLAYER_LIST;  // VK_TAB
    extern KeyMapping* SCREENSHOT;   // VK_F2
    extern KeyMapping* TOGGLE_PERSP; // VK_F5
    extern KeyMapping* FULLSCREEN;   // VK_F11
    extern KeyMapping* PAUSE;        // VK_ESCAPE

    extern KeyMapping* HOTBAR_1;     // '1'
    extern KeyMapping* HOTBAR_2;     // '2'
    extern KeyMapping* HOTBAR_3;     // '3'
    extern KeyMapping* HOTBAR_4;     // '4'
    extern KeyMapping* HOTBAR_5;     // '5'
    extern KeyMapping* HOTBAR_6;     // '6'
    extern KeyMapping* HOTBAR_7;     // '7'
    extern KeyMapping* HOTBAR_8;     // '8'
    extern KeyMapping* HOTBAR_9;     // '9'
}

// Creates every keys::xxx KeyMapping. Idempotent — safe to call multiple times.
// Must be called once at startup before dispatching input events.
void initKeyMappings();

} // namespace mc::client
