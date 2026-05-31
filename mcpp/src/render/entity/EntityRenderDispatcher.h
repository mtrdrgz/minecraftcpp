#pragma once
#include "../IRenderDevice.h"
#include <glm/glm.hpp>
#include <memory>

namespace mc {
class Entity;
}

namespace mc::render {

// Phase 10 port of net.minecraft.client.renderer.entity.EntityRenderDispatcher.
class EntityRenderDispatcher {
public:
    EntityRenderDispatcher(IRenderDevice* device);
    ~EntityRenderDispatcher();

    void renderEntities(ICommandList* cmd, const glm::mat4& viewProjection);

    void drawBox(const glm::mat4& model,
                 const glm::vec3& boxMin,
                 const glm::vec3& boxMax,
                 const glm::vec4& color,
                 const glm::mat4& viewProjection,
                 ICommandList* cmd);

    void drawHumanoid(const Entity& entity,
                      const glm::mat4& viewProjection,
                      const glm::vec4& headColor,
                      const glm::vec4& bodyColor,
                      const glm::vec4& armColor,
                      const glm::vec4& legColor,
                      bool skeleton,
                      float scale,
                      ICommandList* cmd);

private:
    void setupPipeline();
    void setupCubeGeometry();

    IRenderDevice* m_device = nullptr;
    IPipeline*     m_pipeline = nullptr;
    IBuffer*       m_cubeVbo = nullptr;
    IBuffer*       m_cubeIbo = nullptr;

    // ── Entity Shaders ─────────────────────────────────────────────────────────

    static constexpr const char* ENTITY_VS = R"(
#version 460 core
layout(location=0) in vec3 aPos;
uniform mat4 uMVP;
uniform mat4 uModel;
uniform vec3 uBoxMin;
uniform vec3 uBoxMax;
out float vLight;
void main() {
    vec3 localPos = mix(uBoxMin, uBoxMax, aPos);
    gl_Position = uMVP * uModel * vec4(localPos, 1.0);
    vLight = aPos.y * 0.4 + 0.6; // simple top-down shading
}
)";

    static constexpr const char* ENTITY_FS = R"(
#version 460 core
in float vLight;
out vec4 fragColor;
uniform vec4 uColor;
void main() {
    fragColor = vec4(uColor.rgb * vLight, uColor.a);
}
)";
};

} // namespace mc::render
