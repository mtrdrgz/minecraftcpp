#pragma once
#include <cstdint>
#include <string_view>
#include <functional>
#include <array>

#ifdef _WIN32
#include <windows.h>
#else
// On Linux, use void* for native window handle (GLFWwindow* casted)
typedef void* HWND;
#endif

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

    // Native window handle (HWND on Windows, GLFWwindow* on Linux)
#ifdef _WIN32
    HWND    nativeHandle() const { return m_native; }
#else
    void*   nativeHandle() const { return m_native; }
#endif
#ifdef _WIN32
    HWND    hwnd() const { return m_native; }
#else
    void*   hwnd() const { return nullptr; }  // No HWND on Linux
#endif
    int32_t width()  const { return m_width; }
    int32_t height() const { return m_height; }
    bool    shouldClose() const { return m_shouldClose; }

    // ── Input ─────────────────────────────────────────────────────────────────
    bool isKeyDown(int vkey) const;

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

    void consumeMouseDelta(int& dx, int& dy) {
        dx = m_mouseDx; dy = m_mouseDy;
        m_mouseDx = m_mouseDy = 0;
    }

    void captureMouse(bool capture);
    bool isMouseCaptured() const { return m_mouseCaptured; }

    // Called by platform event handler
    void onResize(int32_t w, int32_t h);
    void onClose();
    void onKeyDown(int vkey);
    void onKeyUp  (int vkey);
    void onMouseMove(int x, int y);
    void onLButtonDown();
    void onLButtonUp();
    void clearLButtonState();

private:
#ifdef _WIN32
    HWND    m_native      = nullptr;
#else
    void*   m_native      = nullptr;  // GLFWwindow*
#endif
    int32_t m_width       = 0;
    int32_t m_height      = 0;
    bool    m_shouldClose = false;

    // Keyboard (key codes 0-511 to cover both VK_ and GLFW ranges)
    std::array<bool, 512> m_keys{};

    // Mouse
    int  m_mouseDx = 0, m_mouseDy = 0;
    int  m_dragDx = 0, m_dragDy = 0;
    int  m_lastMouseX = 0, m_lastMouseY = 0;
    bool m_lButtonClicked = false;
    bool m_lButtonReleased = false;
    bool m_lButtonDown = false;
    bool m_mouseCaptured  = false;
    bool m_ignoreNextMove = false;

#ifdef _WIN32
    static LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
#endif
};

} // namespace mc
