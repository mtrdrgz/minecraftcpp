// DeviceGL.cpp (Linux) — OpenGL device using GLFW for context management.
// GLFW creates the GL context in Window_glfw.cpp; this device just loads
// GL functions via glad and provides the IRenderDevice interface.
#ifndef _WIN32

#include "DeviceGL.h"
#include "CommandListGL.h"
#include "../../core/Log.h"
#include <glad/gl.h>
#include <stdexcept>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

namespace mc::render {

std::unique_ptr<DeviceGL> DeviceGL::create(void* glfwWindow, bool vsync) {
    auto dev = std::unique_ptr<DeviceGL>(new DeviceGL());
    if (!dev->init(glfwWindow, vsync)) {
        MC_LOG_ERROR("DeviceGL (GLFW): init failed");
        return nullptr;
    }
    return dev;
}

DeviceGL::~DeviceGL() {
    // GLFW owns the context; nothing to destroy here
}

bool DeviceGL::init(void* glfwWindow, bool vsync) {
    m_window = glfwWindow;
    auto* win = static_cast<GLFWwindow*>(glfwWindow);
    glfwMakeContextCurrent(win);

    // Load GL functions via glad, using glfwGetProcAddress
    if (!gladLoadGL((GLADloadfunc)glfwGetProcAddress)) {
        MC_LOG_ERROR("gladLoadGL failed (GLFW)");
        return false;
    }

    MC_LOG_INFO("OpenGL {} (GLFW)", (const char*)glGetString(GL_VERSION));
    MC_LOG_INFO("GPU: {}", (const char*)glGetString(GL_RENDERER));

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    glfwSwapInterval(vsync ? 1 : 0);

    // CommandListGL takes a GLContextHandle (void*); we pass the GLFWwindow*
    m_cmdList = std::make_unique<CommandListGL>(win);
    return true;
}

ICommandList* DeviceGL::beginFrame(int32_t w, int32_t h) {
    glViewport(0, 0, w, h);
    return m_cmdList.get();
}

void DeviceGL::endFrame() {
    GLenum err = glGetError();
    if (err != GL_NO_ERROR) {
        MC_LOG_ERROR("OpenGL error before swap: 0x{:X}", err);
    }
    glfwSwapBuffers(static_cast<GLFWwindow*>(m_window));
}

void DeviceGL::waitIdle() {
    glFinish();
}

void DeviceGL::setVsync(bool v) {
    glfwSwapInterval(v ? 1 : 0);
}

IBuffer* DeviceGL::createBuffer(const BufferDesc& desc) {
    return new struct BufferGL(desc);
}
void DeviceGL::destroyBuffer(IBuffer* b)     { delete b; }

ITexture* DeviceGL::createTexture(const TextureDesc& desc) {
    return new struct TextureGL(desc);
}
void DeviceGL::destroyTexture(ITexture* t)   { delete t; }

IPipeline* DeviceGL::createPipeline(const PipelineDesc& desc) {
    return new struct PipelineGL(desc);
}
void DeviceGL::destroyPipeline(IPipeline* p) { delete p; }

} // namespace mc::render

#endif // !_WIN32
