#include "KeyMapping.h"

#include <windows.h>

#include <memory>
#include <utility>

namespace mc::client {

// ── Static registry ──────────────────────────────────────────────────────────
// Mirrors KeyMapping.ALL from the Java source. A simple vector keeps the
// registration order stable (useful for the controls screen) and lookups by
// name are infrequent enough that a linear scan is fine.
std::vector<KeyMapping*>& KeyMapping::all() {
    static std::vector<KeyMapping*> instance;
    return instance;
}

KeyMapping::KeyMapping(std::string name, InputKey defaultKey, std::string category)
    : m_name(std::move(name))
    , m_category(std::move(category))
    , m_default(defaultKey)
    , m_current(defaultKey) {
    all().push_back(this);
}

void KeyMapping::setKey(InputKey k) {
    m_current = k;
}

bool KeyMapping::consumeClick() {
    if (m_clickCount == 0) return false;
    --m_clickCount;
    return true;
}

void KeyMapping::release() {
    m_clickCount = 0;
    m_isDown = false;
}

// ── Static input dispatch ────────────────────────────────────────────────────
// Port of KeyMapping.set(key, state) + KeyMapping.click(key). The Java version
// maintains a separate Map<Key, List<KeyMapping>>; we just iterate ALL and
// match — the registry has <100 entries so this is plenty fast and avoids the
// invalidation issues when setKey() rebinds a mapping.
void KeyMapping::setAll(InputKey k, bool pressed) {
    if (k.type == InputKey::Unknown) return;

    for (KeyMapping* m : all()) {
        if (m->m_current == k) {
            const bool wasDown = m->m_isDown;
            m->m_isDown = pressed;
            // On a fresh press, accumulate one click — consumeClick() pops it.
            if (pressed && !wasDown) {
                ++m->m_clickCount;
            }
        }
    }
}

void KeyMapping::releaseAll() {
    for (KeyMapping* m : all()) {
        m->release();
    }
}

KeyMapping* KeyMapping::find(std::string_view name) {
    for (KeyMapping* m : all()) {
        if (m->m_name == name) return m;
    }
    return nullptr;
}

// ── Default key mappings ─────────────────────────────────────────────────────
// These match the values defined in Options.java (the GLFW key codes there
// translate to the Win32 VK_ codes used here — letter keys share the same
// numeric code, others are mapped explicitly below).
namespace keys {
    KeyMapping* FORWARD      = nullptr;
    KeyMapping* BACK         = nullptr;
    KeyMapping* LEFT         = nullptr;
    KeyMapping* RIGHT        = nullptr;
    KeyMapping* JUMP         = nullptr;
    KeyMapping* SNEAK        = nullptr;
    KeyMapping* SPRINT       = nullptr;
    KeyMapping* ATTACK       = nullptr;
    KeyMapping* USE          = nullptr;
    KeyMapping* PICK_ITEM    = nullptr;
    KeyMapping* DROP         = nullptr;
    KeyMapping* INVENTORY    = nullptr;
    KeyMapping* SWAP_OFFHAND = nullptr;
    KeyMapping* CHAT         = nullptr;
    KeyMapping* COMMAND      = nullptr;
    KeyMapping* PLAYER_LIST  = nullptr;
    KeyMapping* SCREENSHOT   = nullptr;
    KeyMapping* TOGGLE_PERSP = nullptr;
    KeyMapping* FULLSCREEN   = nullptr;
    KeyMapping* PAUSE        = nullptr;
    KeyMapping* HOTBAR_1     = nullptr;
    KeyMapping* HOTBAR_2     = nullptr;
    KeyMapping* HOTBAR_3     = nullptr;
    KeyMapping* HOTBAR_4     = nullptr;
    KeyMapping* HOTBAR_5     = nullptr;
    KeyMapping* HOTBAR_6     = nullptr;
    KeyMapping* HOTBAR_7     = nullptr;
    KeyMapping* HOTBAR_8     = nullptr;
    KeyMapping* HOTBAR_9     = nullptr;
} // namespace keys

namespace {

// Holders that own the KeyMapping instances. We use unique_ptr so the
// registry pointers stay valid for the lifetime of the program.
std::vector<std::unique_ptr<KeyMapping>>& storage() {
    static std::vector<std::unique_ptr<KeyMapping>> s;
    return s;
}

KeyMapping* make(std::string name, InputKey def, std::string category) {
    auto km = std::make_unique<KeyMapping>(std::move(name), def, std::move(category));
    KeyMapping* raw = km.get();
    storage().push_back(std::move(km));
    return raw;
}

// Category constants — match KeyMapping.Category identifiers in the Java source.
constexpr const char* CAT_MOVEMENT    = "movement";
constexpr const char* CAT_MISC        = "misc";
constexpr const char* CAT_MULTIPLAYER = "multiplayer";
constexpr const char* CAT_GAMEPLAY    = "gameplay";
constexpr const char* CAT_INVENTORY   = "inventory";

} // namespace

void initKeyMappings() {
    // Idempotent — calling twice would otherwise duplicate the registry.
    if (!storage().empty()) return;

    // Movement — Options.java lines 604-610 (keyUp, keyLeft, keyDown, keyRight,
    // keyJump, keyShift, keySprint). W/A/S/D share VK codes with their ASCII
    // letter values (0x57, 0x41, 0x53, 0x44).
    keys::FORWARD      = make("key.forward",      InputKey::keyboard('W'),         CAT_MOVEMENT);
    keys::BACK         = make("key.back",         InputKey::keyboard('S'),         CAT_MOVEMENT);
    keys::LEFT         = make("key.left",         InputKey::keyboard('A'),         CAT_MOVEMENT);
    keys::RIGHT        = make("key.right",        InputKey::keyboard('D'),         CAT_MOVEMENT);
    keys::JUMP         = make("key.jump",         InputKey::keyboard(VK_SPACE),    CAT_MOVEMENT);
    keys::SNEAK        = make("key.sneak",        InputKey::keyboard(VK_SHIFT),    CAT_MOVEMENT);
    keys::SPRINT       = make("key.sprint",       InputKey::keyboard(VK_CONTROL),  CAT_MOVEMENT);

    // Gameplay — Options.java lines 614-618.
    keys::ATTACK       = make("key.attack",       InputKey::mouse(0),              CAT_GAMEPLAY);
    keys::USE          = make("key.use",          InputKey::mouse(1),              CAT_GAMEPLAY);
    keys::PICK_ITEM    = make("key.pickItem",     InputKey::mouse(2),              CAT_GAMEPLAY);

    // Inventory — Options.java lines 611-613.
    keys::INVENTORY    = make("key.inventory",    InputKey::keyboard('E'),         CAT_INVENTORY);
    keys::SWAP_OFFHAND = make("key.swapOffhand",  InputKey::keyboard('F'),         CAT_INVENTORY);
    keys::DROP         = make("key.drop",         InputKey::keyboard('Q'),         CAT_INVENTORY);

    // Multiplayer — Options.java lines 619-622.
    keys::CHAT         = make("key.chat",         InputKey::keyboard('T'),         CAT_MULTIPLAYER);
    keys::PLAYER_LIST  = make("key.playerlist",   InputKey::keyboard(VK_TAB),      CAT_MULTIPLAYER);
    keys::COMMAND      = make("key.command",      InputKey::keyboard(VK_OEM_2),    CAT_MULTIPLAYER);

    // Misc — Options.java lines 623-626.
    keys::SCREENSHOT   = make("key.screenshot",   InputKey::keyboard(VK_F2),       CAT_MISC);
    keys::TOGGLE_PERSP = make("key.togglePerspective", InputKey::keyboard(VK_F5),  CAT_MISC);
    keys::FULLSCREEN   = make("key.fullscreen",   InputKey::keyboard(VK_F11),      CAT_MISC);
    keys::PAUSE        = make("key.pause",        InputKey::keyboard(VK_ESCAPE),   CAT_MISC);

    // Hotbar — Options.java lines 631-640. ASCII '1'..'9' match VK_1..VK_9.
    keys::HOTBAR_1 = make("key.hotbar.1", InputKey::keyboard('1'), CAT_INVENTORY);
    keys::HOTBAR_2 = make("key.hotbar.2", InputKey::keyboard('2'), CAT_INVENTORY);
    keys::HOTBAR_3 = make("key.hotbar.3", InputKey::keyboard('3'), CAT_INVENTORY);
    keys::HOTBAR_4 = make("key.hotbar.4", InputKey::keyboard('4'), CAT_INVENTORY);
    keys::HOTBAR_5 = make("key.hotbar.5", InputKey::keyboard('5'), CAT_INVENTORY);
    keys::HOTBAR_6 = make("key.hotbar.6", InputKey::keyboard('6'), CAT_INVENTORY);
    keys::HOTBAR_7 = make("key.hotbar.7", InputKey::keyboard('7'), CAT_INVENTORY);
    keys::HOTBAR_8 = make("key.hotbar.8", InputKey::keyboard('8'), CAT_INVENTORY);
    keys::HOTBAR_9 = make("key.hotbar.9", InputKey::keyboard('9'), CAT_INVENTORY);
}

} // namespace mc::client
