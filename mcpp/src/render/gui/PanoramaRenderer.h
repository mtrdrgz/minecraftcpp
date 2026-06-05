#pragma once
#include "../IRenderDevice.h"

namespace mc {
class AssetManager;
}

namespace mc::render {

// Port of net.minecraft.client.renderer.{CubeMap,Panorama}: the rotating title
// panorama. 26.1.2 renders it as a GL cubemap; this draws the equivalent 6 textured
// cube faces (camera at the centre, perspective FOV 85). The full-res panorama faces
// ship in assets.bin (the jar only has 1x1 stubs), loaded via AssetManager.
//
// Cubemap face order (from CubeMapTexture SUFFIXES = {_1,_3,_5,_4,_0,_2} into GL faces
// +X,-X,+Y,-Y,+Z,-Z): +X=panorama_1, -X=panorama_3, +Y=panorama_5, -Y=panorama_4,
// +Z=panorama_0, -Z=panorama_2.
class PanoramaRenderer {
public:
    explicit PanoramaRenderer(IRenderDevice* device);
    ~PanoramaRenderer();

    // Loads the 6 faces (and the overlay) from assets.bin. Safe to call repeatedly;
    // only loads once. Returns true if all faces are available.
    bool ensureLoaded(ICommandList* cmd);

    // Advance the slow spin and draw the cube. dtSeconds is the real frame delta.
    void render(ICommandList* cmd, int width, int height, float dtSeconds);

    bool loaded() const { return m_loaded; }
    ITexture* overlay() const { return m_overlay; }

private:
    IRenderDevice* m_device = nullptr;
    IPipeline*     m_pipeline = nullptr;
    IBuffer*       m_vbo = nullptr;
    IBuffer*       m_ibo = nullptr;
    ITexture*      m_faces[6] = {};   // indexed by cube face: +X,-X,+Y,-Y,+Z,-Z
    ITexture*      m_overlay = nullptr;
    bool           m_loaded = false;
    bool           m_tried = false;
    bool           m_geomUploaded = false;
    float          m_spin = 0.0f;     // degrees

    static constexpr const char* PANO_VS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec2 aUV;
layout(location=2) in vec4 aColor;
uniform mat4 uMVP;
out vec2 vUV;
void main() { gl_Position = uMVP * vec4(aPos, 1.0); vUV = aUV; }
)";
    static constexpr const char* PANO_FS = R"(
#version 460 core
in vec2 vUV;
out vec4 fragColor;
uniform sampler2D uTex;
void main() { fragColor = texture(uTex, vUV); }
)";
};

} // namespace mc::render
