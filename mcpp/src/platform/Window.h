#pragma once
#include <cstdint>
#include <string_view>
#include <functional>
#include <array>

#include <windows.h>

namespace mc {

struct WindowDesc {
    std::string_view title  = "Minecraft";
    int32_t          width  = 1280;
    int32_t          height = 720;
    bool             vsync  = true;
};

class Window {
public:
    explicit Window(const WindowDesc& desc);
    ~Window();

    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    // Returns false when the window should close
    bool pollEvents();

    HWND    hwnd()   const { return m_hwnd; }
    int32_t width()  const { return m_width; }
    int32_t height() const { return m_height; }
    bool    shouldClose() const { return m_shouldClose; }

    // ── Input ─────────────────────────────────────────────────────────────────
    // Virtual-key state (updated by WM_KEYDOWN / WM_KEYUP)
    bool isKeyDown(int vkey) const { return m_keys[vkey & 0xFF]; }

    int  mouseX() const { return m_lastMouseX; }
    int  mouseY() const { return m_lastMouseY; }
    bool isLButtonDown() const { return m_lButtonDown; }
    bool consumeLButtonClicked() { bool b = m_lButtonClicked; m_lButtonClicked = false; return b; }
    bool consumeLButtonReleased() { bool b = m_lButtonReleased; m_lButtonReleased = false; return b; }
    bool consumeMouseDrag(int& dx, int& dy) {
        dx = m_dragDx;
        dy = m_dragDy;
        m_dragDx = m_dragDy = 0;
        return dx != 0 || dy != 0;
    }

    // Accumulated mouse delta since last consumeMouseDelta call.
    // Reset to zero on each call.
    void consumeMouseDelta(int& dx, int& dy) {
        dx = m_mouseDx; dy = m_mouseDy;
        m_mouseDx = m_mouseDy = 0;
    }

    // Hide/show cursor and confine it to the window.
    void captureMouse(bool capture);
    bool isMouseCaptured() const { return m_mouseCaptured; }

    // Called by WndProc
    void onResize(int32_t w, int32_t h);
    void onClose();
    void onKeyDown(int vkey) { m_keys[vkey & 0xFF] = true; }
    void onKeyUp  (int vkey) { m_keys[vkey & 0xFF] = false; }
    void onMouseMove(int x, int y);
    void onLButtonDown();
    void onLButtonUp();
    void clearLButtonState();

private:
    HWND    m_hwnd        = nullptr;
    int32_t m_width       = 0;
    int32_t m_height      = 0;
    bool    m_shouldClose = false;

    // Keyboard
    std::array<bool, 256> m_keys{};

    // Mouse
    int  m_mouseDx = 0, m_mouseDy = 0;   // accumulated delta
    int  m_dragDx = 0, m_dragDy = 0;     // accumulated visible-cursor LMB drag delta
    int  m_lastMouseX = 0, m_lastMouseY = 0;
    bool m_lButtonClicked = false;
    bool m_lButtonReleased = false;
    bool m_lButtonDown = false;
    bool m_mouseCaptured  = false;
    bool m_ignoreNextMove = false;        // skip the warp-back WM_MOUSEMOVE

    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
};

} // namespace mc
