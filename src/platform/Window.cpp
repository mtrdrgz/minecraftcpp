#ifdef _WIN32

#include "Window.h"
#include "../core/Log.h"
#include <stdexcept>

namespace mc {

static constexpr wchar_t CLASS_NAME[] = L"McppWindow";

Window::Window(const WindowDesc& desc)
    : m_width(desc.width), m_height(desc.height)
{
    HINSTANCE hInst = GetModuleHandleW(nullptr);

    WNDCLASSEXW wc{};
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursorW(nullptr, IDC_ARROW);
    wc.lpszClassName = CLASS_NAME;
    RegisterClassExW(&wc);

    // Client area = requested size
    RECT rc{0, 0, m_width, m_height};
    DWORD style = WS_OVERLAPPEDWINDOW;
    AdjustWindowRect(&rc, style, FALSE);

    std::wstring wtitle;
    if (!desc.title.empty()) {
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, desc.title.data(), (int)desc.title.size(), NULL, 0);
        wtitle.resize(size_needed);
        MultiByteToWideChar(CP_UTF8, 0, desc.title.data(), (int)desc.title.size(), &wtitle[0], size_needed);
    } else {
        wtitle = CLASS_NAME;
    }
    m_native = CreateWindowExW(
        0, CLASS_NAME, wtitle.c_str(), style,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        nullptr, nullptr, hInst, this
    );


    if (!m_native)
        throw std::runtime_error("CreateWindowExW failed");

    ShowWindow(m_native, SW_SHOW);
    UpdateWindow(m_native);
    MC_LOG_INFO("Window created ({}x{})", m_width, m_height);
}

Window::~Window() {
    captureMouse(false);
    if (m_native) {
        DestroyWindow(m_native);
        m_native = nullptr;
    }
    UnregisterClassW(CLASS_NAME, GetModuleHandleW(nullptr));
}

bool Window::pollEvents() {
    MSG msg{};
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) {
            m_shouldClose = true;
            return false;
        }
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    
    // Also check if the window was closed via WM_CLOSE
    return !m_shouldClose;
}

void Window::onResize(int32_t w, int32_t h) {
    m_width  = w;
    m_height = h;
    if (m_mouseCaptured) captureMouse(true); // re-clip to new rect
    MC_LOG_DEBUG("Window resized to {}x{}", w, h);
}

void Window::onClose() {
    m_shouldClose = true;
}

void Window::captureMouse(bool capture) {
    if (capture == m_mouseCaptured) return;
    m_mouseCaptured = capture;

    if (capture) {
        ShowCursor(FALSE);
        // Confine cursor to client area
        RECT r;
        GetClientRect(m_native, &r);
        POINT tl{r.left, r.top}, br{r.right, r.bottom};
        ClientToScreen(m_native, &tl);
        ClientToScreen(m_native, &br);
        RECT clipRect{tl.x, tl.y, br.x, br.y};
        ClipCursor(&clipRect);
        // Warp to center and track from there
        int cx = (tl.x + br.x) / 2;
        int cy = (tl.y + br.y) / 2;
        SetCursorPos(cx, cy);
        POINT pt{cx, cy};
        ScreenToClient(m_native, &pt);
        m_lastMouseX = pt.x;
        m_lastMouseY = pt.y;
        m_ignoreNextMove = true;
    } else {
        ShowCursor(TRUE);
        ClipCursor(nullptr);
    }
    m_mouseDx = m_mouseDy = 0;
}

void Window::onMouseMove(int x, int y) {
    if (m_mouseCaptured) {
        if (m_ignoreNextMove) {
            // This is the synthetic event from our own SetCursorPos
            m_ignoreNextMove = false;
            m_lastMouseX = x;
            m_lastMouseY = y;
            return;
        }
        m_mouseDx += x - m_lastMouseX;
        m_mouseDy += y - m_lastMouseY;

        // Warp back to center of client area
        RECT r;
        GetClientRect(m_native, &r);
        int cx = (r.left + r.right)  / 2;
        int cy = (r.top  + r.bottom) / 2;
        m_lastMouseX = cx;
        m_lastMouseY = cy;

        POINT screenCenter{cx, cy};
        ClientToScreen(m_native, &screenCenter);
        m_ignoreNextMove = true;
        SetCursorPos(screenCenter.x, screenCenter.y);
    } else {
        if (m_lButtonDown) {
            m_dragDx += x - m_lastMouseX;
            m_dragDy += y - m_lastMouseY;
        }
        m_lastMouseX = x;
        m_lastMouseY = y;
    }
}

void Window::onLButtonDown() {
    // Just record the click. Whether it should grab the mouse is a GAME decision
    // (in-game = grab for the camera; on a menu = keep the cursor visible so the
    // user can click widgets) — handled in the main loop, not here. Capturing on
    // every click hid the cursor on the title screen and broke button clicks.
    m_lButtonClicked = true;
    m_lButtonDown = true;
    if (!m_mouseCaptured) SetCapture(m_native);
}

void Window::onLButtonUp() {
    m_lButtonReleased = true;
    m_lButtonDown = false;
    if (!m_mouseCaptured && GetCapture() == m_native) ReleaseCapture();
}

void Window::clearLButtonState() {
    m_lButtonDown = false;
    m_dragDx = m_dragDy = 0;
    if (!m_mouseCaptured && GetCapture() == m_native) ReleaseCapture();
}

LRESULT CALLBACK Window::WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    Window* self = nullptr;
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lp);
        self = static_cast<Window*>(cs->lpCreateParams);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    } else {
        self = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    }

    switch (msg) {
    case WM_SIZE:
        if (self && wp != SIZE_MINIMIZED)
            self->onResize(LOWORD(lp), HIWORD(lp));
        return 0;

    case WM_CLOSE:
        if (self) self->onClose();
        return 0;

    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;

    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (self) {
            self->onKeyDown((int)wp);
            // ESC releases mouse capture
            if (wp == VK_ESCAPE && self->m_mouseCaptured)
                self->captureMouse(false);
        }
        return 0;

    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (self) self->onKeyUp((int)wp);
        return 0;

    case WM_MOUSEMOVE:
        if (self) self->onMouseMove((int)(short)LOWORD(lp), (int)(short)HIWORD(lp));
        return 0;

    case WM_LBUTTONDOWN:
        if (self) self->onLButtonDown();
        return 0;

    case WM_LBUTTONUP:
        if (self) self->onLButtonUp();
        return 0;

    case WM_ACTIVATE:
        // Release mouse capture when window loses focus
        if (self && LOWORD(wp) == WA_INACTIVE) {
            self->clearLButtonState();
            self->captureMouse(false);
        }
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

bool Window::isKeyDown(int vkey) const {
    if (vkey < 0 || vkey >= 512) return false;
    return m_keys[vkey & 0x1FF];
}

void Window::onKeyDown(int vkey) {
    if (vkey >= 0 && vkey < 512) m_keys[vkey] = true;
}

void Window::onKeyUp(int vkey) {
    if (vkey >= 0 && vkey < 512) m_keys[vkey] = false;
}

} // namespace mc

#endif // _WIN32
