#pragma once
// Platform.h — cross-platform key codes and types.
// On Windows, VK_* constants come from <windows.h>.
// On Linux, we define equivalents so the rest of the codebase doesn't change.

#ifdef _WIN32
// On Windows, windows.h is included by the caller; VK_* are already defined.
#else
// ── Linux: define VK_ equivalents using GLFW key codes ──────────────────────
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#ifndef VK_ESCAPE
#define VK_ESCAPE       GLFW_KEY_ESCAPE
#define VK_F1           GLFW_KEY_F1
#define VK_F2           GLFW_KEY_F2
#define VK_F3           GLFW_KEY_F3
#define VK_F4           GLFW_KEY_F4
#define VK_F5           GLFW_KEY_F5
#define VK_F6           GLFW_KEY_F6
#define VK_F7           GLFW_KEY_F7
#define VK_F8           GLFW_KEY_F8
#define VK_F9           GLFW_KEY_F9
#define VK_F10          GLFW_KEY_F10
#define VK_F11          GLFW_KEY_F11
#define VK_F12          GLFW_KEY_F12
#define VK_SHIFT        GLFW_KEY_LEFT_SHIFT
#define VK_CONTROL      GLFW_KEY_LEFT_CONTROL
#define VK_SPACE        GLFW_KEY_SPACE
#define VK_RETURN       GLFW_KEY_ENTER
#define VK_TAB          GLFW_KEY_TAB
#define VK_BACK         GLFW_KEY_BACKSPACE
#define VK_OEM_MINUS    GLFW_KEY_MINUS
#define VK_NUMPAD0      GLFW_KEY_KP_0
#define VK_SUBTRACT     GLFW_KEY_KP_SUBTRACT
#define VK_OEM_2       GLFW_KEY_SLASH
#define VK_UP           GLFW_KEY_UP
#define VK_DOWN         GLFW_KEY_DOWN
#define VK_LEFT         GLFW_KEY_LEFT
#define VK_RIGHT        GLFW_KEY_RIGHT
#endif // VK_ESCAPE

// PostQuitMessage replacement — just set a flag
inline void PostQuitMessage(int) {
    // Handled by GLFW window close callback in Window_glfw.cpp
}

#endif // _WIN32
