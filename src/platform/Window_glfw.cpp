// Window_glfw.cpp — Linux window implementation using GLFW.
// Used when _WIN32 is NOT defined. Provides the same Window interface as
// Window.cpp (Win32) but via GLFW.

#include "Window.h"
#include "Platform.h"
#include "../core/Log.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace mc {

// Map GLFW key codes to our 0-511 range (GLFW keys are already < 350)
static void glfwKeyCallback(GLFWwindow* win, int key, int scancode, int action, int mods) {
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!w) return;
    if (key < 0 || key >= 512) return;
    if (action == GLFW_PRESS || action == GLFW_REPEAT) {
        w->onKeyDown(key);
    } else if (action == GLFW_RELEASE) {
        w->onKeyUp(key);
    }
}

static void glfwMouseButtonCallback(GLFWwindow* win, int button, int action, int mods) {
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!w) return;
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) w->onLButtonDown();
        else if (action == GLFW_RELEASE) w->onLButtonUp();
    }
}

static void glfwCursorPosCallback(GLFWwindow* win, double xpos, double ypos) {
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!w) return;
    w->onMouseMove((int)xpos, (int)ypos);
}

static void glfwFramebufferSizeCallback(GLFWwindow* win, int width, int height) {
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!w) return;
    w->onResize(width, height);
}

static void glfwWindowCloseCallback(GLFWwindow* win) {
    Window* w = static_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!w) return;
    w->onClose();
}

Window::Window(const WindowDesc& desc) {
    if (!glfwInit()) {
        MC_LOG_ERROR("GLFW: failed to initialize");
        return;
    }

    // Request OpenGL 4.6 Core (fallback to 4.1 if not available)
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
    glfwWindowHint(GLFW_STENCIL_BITS, 8);

    m_native = glfwCreateWindow(desc.width, desc.height,
                                std::string(desc.title).c_str(),
                                nullptr, nullptr);
    if (!m_native) {
        MC_LOG_ERROR("GLFW: failed to create window");
        glfwTerminate();
        return;
    }

    m_width = desc.width;
    m_height = desc.height;
    m_shouldClose = false;

    glfwSetWindowUserPointer(static_cast<GLFWwindow*>(m_native), this);
    glfwSetKeyCallback(static_cast<GLFWwindow*>(m_native), glfwKeyCallback);
    glfwSetMouseButtonCallback(static_cast<GLFWwindow*>(m_native), glfwMouseButtonCallback);
    glfwSetCursorPosCallback(static_cast<GLFWwindow*>(m_native), glfwCursorPosCallback);
    glfwSetFramebufferSizeCallback(static_cast<GLFWwindow*>(m_native), glfwFramebufferSizeCallback);
    glfwSetWindowCloseCallback(static_cast<GLFWwindow*>(m_native), glfwWindowCloseCallback);

    // Make context current for GL device creation
    glfwMakeContextCurrent(static_cast<GLFWwindow*>(m_native));
    glfwSwapInterval(desc.vsync ? 1 : 0);

    MC_LOG_INFO("GLFW window created: {}x{}", desc.width, desc.height);
}

Window::~Window() {
    if (m_native) {
        glfwDestroyWindow(static_cast<GLFWwindow*>(m_native));
    }
    glfwTerminate();
}

bool Window::pollEvents() {
    if (!m_native) return false;
    glfwPollEvents();
    if (glfwWindowShouldClose(static_cast<GLFWwindow*>(m_native))) {
        m_shouldClose = true;
    }
    return !m_shouldClose;
}

bool Window::isKeyDown(int vkey) const {
    if (vkey < 0 || vkey >= 512) return false;
    return m_keys[vkey & 0x1FF];
}

void Window::captureMouse(bool capture) {
    m_mouseCaptured = capture;
    if (!m_native) return;
    auto* win = static_cast<GLFWwindow*>(m_native);
    if (capture) {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    } else {
        glfwSetInputMode(win, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
    }
}

void Window::onResize(int32_t w, int32_t h) {
    m_width = w;
    m_height = h;
}

void Window::onClose() {
    m_shouldClose = true;
}

void Window::onKeyDown(int vkey) {
    if (vkey >= 0 && vkey < 512) m_keys[vkey] = true;
}

void Window::onKeyUp(int vkey) {
    if (vkey >= 0 && vkey < 512) m_keys[vkey] = false;
}

void Window::onMouseMove(int x, int y) {
    if (m_mouseCaptured) {
        if (m_ignoreNextMove) {
            m_ignoreNextMove = false;
            m_lastMouseX = x;
            m_lastMouseY = y;
            return;
        }
        m_mouseDx += x - m_lastMouseX;
        m_mouseDy += y - m_lastMouseY;

        // Warp back to center
        int cx = m_width / 2;
        int cy = m_height / 2;
        m_lastMouseX = cx;
        m_lastMouseY = cy;
        m_ignoreNextMove = true;
        glfwSetCursorPos(static_cast<GLFWwindow*>(m_native), cx, cy);
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
    m_lButtonClicked = true;
    m_lButtonDown = true;
}

void Window::onLButtonUp() {
    m_lButtonReleased = true;
    m_lButtonDown = false;
}

void Window::clearLButtonState() {
    m_lButtonClicked = false;
    m_lButtonReleased = false;
    m_lButtonDown = false;
    m_mouseDx = m_mouseDy = 0;
    m_dragDx = m_dragDy = 0;
}

} // namespace mc
