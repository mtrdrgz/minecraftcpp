#pragma once
// DeviceGL_glfw.h — Linux OpenGL device using GLFW for context management.
// GLFW creates and manages the GL context; this device wraps it and uses
// glad for GL function loading (same as the Windows DeviceGL).

#include "../IRenderDevice.h"

namespace mc::render {

class DeviceGL_glfw : public IRenderDevice {
public:
    static std::unique_ptr<IRenderDevice> create(void* glfwWindow);
    ~DeviceGL_glfw() override;

    // IRenderDevice interface
    ICommandList* beginFrame(int width, int height) override;
    void endFrame() override;
    void waitIdle() override;

    IPipeline* createPipeline(const PipelineDesc& desc) override;
    void destroyPipeline(IPipeline* p) override;
    IBuffer* createBuffer(const BufferDesc& desc) override;
    void destroyBuffer(IBuffer* b) override;
    ITexture* createTexture(int w, int h, TextureFormat fmt) override;
    void destroyTexture(ITexture* t) override;

    void uploadBuffer(IBuffer* buf, const void* data, size_t size) override;

    Backend backend() const override { return Backend::OpenGL; }
    const char* backendName() const override { return "OpenGL (GLFW)"; }

private:
    DeviceGL_glfw(void* window);
    bool init();

    void* m_window = nullptr;  // GLFWwindow*
};

} // namespace mc::render
