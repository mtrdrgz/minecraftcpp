#include "DeviceGL.h"
#include "CommandListGL.h"
#include "../../core/Log.h"
#include <glad/gl.h>
#include <stdexcept>
#include <string>

// ── WGL extension types and constants ───────────────────────────────────────
#define WGL_DRAW_TO_WINDOW_ARB          0x2001
#define WGL_SUPPORT_OPENGL_ARB          0x2010
#define WGL_DOUBLE_BUFFER_ARB           0x2011
#define WGL_PIXEL_TYPE_ARB              0x2013
#define WGL_TYPE_RGBA_ARB               0x202B
#define WGL_COLOR_BITS_ARB              0x2014
#define WGL_DEPTH_BITS_ARB              0x2022
#define WGL_STENCIL_BITS_ARB            0x2023
#define WGL_CONTEXT_MAJOR_VERSION_ARB   0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB   0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB    0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_FLAGS_ARB           0x2094
#define WGL_CONTEXT_DEBUG_BIT_ARB       0x00000001

typedef BOOL  (WINAPI* PFNWGLCHOOSEPIXELFORMATARBPROC)(HDC, const int*, const float*, UINT, int*, UINT*);
typedef HGLRC (WINAPI* PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
typedef BOOL  (WINAPI* PFNWGLSWAPINTERVALEXTPROC)(int);

static PFNWGLCHOOSEPIXELFORMATARBPROC    wglChoosePixelFormatARB    = nullptr;
static PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = nullptr;
static PFNWGLSWAPINTERVALEXTPROC         wglSwapIntervalEXT         = nullptr;

// ── GLAD loader ─────────────────────────────────────────────────────────────
static HMODULE s_opengl32 = nullptr;

static void* gladLoader(const char* name) {
    void* p = (void*)wglGetProcAddress(name);
    // wglGetProcAddress returns nullptr (or sentinel values) for legacy core funcs
    if (!p || (intptr_t)p == 1 || (intptr_t)p == 2 ||
               (intptr_t)p == 3 || (intptr_t)p == -1)
    {
        p = (void*)GetProcAddress(s_opengl32, name);
    }
    return p;
}

// ── WGL extensions loader (via dummy context) ────────────────────────────────
static void loadWGLExtensions() {
    static const char* DUMMY_CLASS = "WGLDummy";
    HINSTANCE hinst = GetModuleHandleA(nullptr);

    WNDCLASSEXA wc{};
    wc.cbSize        = sizeof(wc);
    wc.lpfnWndProc   = DefWindowProcA;
    wc.hInstance     = hinst;
    wc.lpszClassName = DUMMY_CLASS;
    RegisterClassExA(&wc);

    HWND dummyWnd = CreateWindowExA(0, DUMMY_CLASS, "dummy", WS_POPUP,
                                    0, 0, 1, 1, nullptr, nullptr, hinst, nullptr);
    HDC  dummyDC  = GetDC(dummyWnd);

    PIXELFORMATDESCRIPTOR pfd{};
    pfd.nSize      = sizeof(pfd);
    pfd.nVersion   = 1;
    pfd.dwFlags    = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER;
    pfd.iPixelType = PFD_TYPE_RGBA;
    pfd.cColorBits = 32;
    pfd.cDepthBits = 24;
    pfd.cStencilBits = 8;
    pfd.iLayerType = PFD_MAIN_PLANE;

    int fmt = ChoosePixelFormat(dummyDC, &pfd);
    SetPixelFormat(dummyDC, fmt, &pfd);

    HGLRC dummyRC = wglCreateContext(dummyDC);
    wglMakeCurrent(dummyDC, dummyRC);

    wglChoosePixelFormatARB    = (PFNWGLCHOOSEPIXELFORMATARBPROC)   wglGetProcAddress("wglChoosePixelFormatARB");
    wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");
    wglSwapIntervalEXT         = (PFNWGLSWAPINTERVALEXTPROC)        wglGetProcAddress("wglSwapIntervalEXT");

    wglMakeCurrent(nullptr, nullptr);
    wglDeleteContext(dummyRC);
    ReleaseDC(dummyWnd, dummyDC);
    DestroyWindow(dummyWnd);
    UnregisterClassA(DUMMY_CLASS, hinst);
}

// ── GL debug callback ────────────────────────────────────────────────────────
static void GLAPIENTRY glDebugCb(GLenum src, GLenum type, GLuint id, GLenum severity,
                                  GLsizei, const GLchar* msg, const void*)
{
    if (severity == GL_DEBUG_SEVERITY_NOTIFICATION) return;
    if (type == GL_DEBUG_TYPE_ERROR)
        MC_LOG_ERROR("GL [{}]: {}", id, msg);
    else
        MC_LOG_WARN("GL [{}]: {}", id, msg);
}

// ── DeviceGL ─────────────────────────────────────────────────────────────────
namespace mc::render {

std::unique_ptr<DeviceGL> DeviceGL::create(HWND hwnd, bool vsync) {
    auto dev = std::unique_ptr<DeviceGL>(new DeviceGL());
    if (!dev->init(hwnd, vsync)) return nullptr;
    return dev;
}

bool DeviceGL::init(HWND hwnd, bool vsync) {
    s_opengl32 = LoadLibraryA("opengl32.dll");
    if (!s_opengl32) { MC_LOG_ERROR("Cannot load opengl32.dll"); return false; }

    loadWGLExtensions();
    if (!wglCreateContextAttribsARB) {
        MC_LOG_ERROR("wglCreateContextAttribsARB not available");
        return false;
    }

    m_hwnd = hwnd;
    m_hdc  = GetDC(hwnd);

    // Choose pixel format with sRGB + multisampling
    const int pfAttribs[] = {
        WGL_DRAW_TO_WINDOW_ARB, GL_TRUE,
        WGL_SUPPORT_OPENGL_ARB, GL_TRUE,
        WGL_DOUBLE_BUFFER_ARB,  GL_TRUE,
        WGL_PIXEL_TYPE_ARB,     WGL_TYPE_RGBA_ARB,
        WGL_COLOR_BITS_ARB,     32,
        WGL_DEPTH_BITS_ARB,     24,
        WGL_STENCIL_BITS_ARB,   8,
        0
    };
    int  pixelFmt = 0;
    UINT numFmts  = 0;
    if (!wglChoosePixelFormatARB(m_hdc, pfAttribs, nullptr, 1, &pixelFmt, &numFmts) || numFmts == 0) {
        MC_LOG_ERROR("wglChoosePixelFormatARB failed");
        return false;
    }

    PIXELFORMATDESCRIPTOR pfdOut{};
    DescribePixelFormat(m_hdc, pixelFmt, sizeof(pfdOut), &pfdOut);
    SetPixelFormat(m_hdc, pixelFmt, &pfdOut);

    const int ctxAttribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB, 4,
        WGL_CONTEXT_MINOR_VERSION_ARB, 6,
        WGL_CONTEXT_PROFILE_MASK_ARB,  WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifdef _DEBUG
        WGL_CONTEXT_FLAGS_ARB,         WGL_CONTEXT_DEBUG_BIT_ARB,
#endif
        0
    };
    m_hglrc = wglCreateContextAttribsARB(m_hdc, nullptr, ctxAttribs);
    if (!m_hglrc) { MC_LOG_ERROR("wglCreateContextAttribsARB failed"); return false; }

    wglMakeCurrent(m_hdc, m_hglrc);

    if (!gladLoadGL((GLADloadfunc)gladLoader)) {
        MC_LOG_ERROR("gladLoadGL failed");
        return false;
    }

    MC_LOG_INFO("OpenGL {}", (const char*)glGetString(GL_VERSION));
    MC_LOG_INFO("GPU: {}", (const char*)glGetString(GL_RENDERER));

#ifdef _DEBUG
    glEnable(GL_DEBUG_OUTPUT);
    glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
    glDebugMessageCallback(glDebugCb, nullptr);
#endif

    if (wglSwapIntervalEXT) wglSwapIntervalEXT(vsync ? 1 : 0);

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);

    m_cmdList = std::make_unique<CommandListGL>(m_hdc);
    return true;
}

DeviceGL::~DeviceGL() {
    if (m_hglrc) {
        wglMakeCurrent(nullptr, nullptr);
        wglDeleteContext(m_hglrc);
    }
    if (m_hdc && m_hwnd) ReleaseDC(m_hwnd, m_hdc);
    if (s_opengl32) { FreeLibrary(s_opengl32); s_opengl32 = nullptr; }
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
    SwapBuffers(m_hdc);
}

void DeviceGL::waitIdle() {
    glFinish();
}

void DeviceGL::setVsync(bool v) {
    if (wglSwapIntervalEXT) wglSwapIntervalEXT(v ? 1 : 0);
}

IBuffer* DeviceGL::createBuffer(const BufferDesc& desc) {
    return new struct BufferGL(desc); // defined in BufferGL.cpp — stub for now
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
