#include "EntityRenderDispatcher.h"
#include "../../world/entity/Entities.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>

namespace mc::render {

namespace {
    glm::vec4 colorFor(const Entity& entity) {
        switch (entity.type) {
            case EntityType::PLAYER: return {0.25f, 0.62f, 1.00f, 0.92f};
            case EntityType::ZOMBIE:
            case EntityType::SKELETON:
            case EntityType::CREEPER:
            case EntityType::SPIDER:
            case EntityType::ENDERMAN:
            case EntityType::WITCH:
            case EntityType::BLAZE:
            case EntityType::GHAST:
            case EntityType::WITHER:
            case EntityType::ENDER_DRAGON:
                return {0.86f, 0.18f, 0.16f, 0.90f};
            case EntityType::ITEM:
            case EntityType::EXPERIENCE_ORB:
                return {0.95f, 0.82f, 0.22f, 0.90f};
            default:
                return {0.78f, 0.78f, 0.82f, 0.86f};
        }
    }
}

EntityRenderDispatcher::EntityRenderDispatcher(IRenderDevice* device)
    : m_device(device) {
    setupPipeline();
    setupCubeGeometry();
}

EntityRenderDispatcher::~EntityRenderDispatcher() {
    if (m_cubeVbo) m_device->destroyBuffer(m_cubeVbo);
    if (m_cubeIbo) m_device->destroyBuffer(m_cubeIbo);
    if (m_pipeline) m_device->destroyPipeline(m_pipeline);
}

void EntityRenderDispatcher::setupPipeline() {
    PipelineDesc desc;
    desc.vsSource = ENTITY_VS;
    desc.fsSource = ENTITY_FS;
    desc.depth = DepthTest::ReadWrite;
    desc.cull = CullMode::None;
    desc.blend = BlendMode::Alpha;
    desc.layout = VertexLayout::Simple;
    m_pipeline = m_device->createPipeline(desc);
}

void EntityRenderDispatcher::setupCubeGeometry() {
    m_cubeVbo = m_device->createBuffer({8 * 12, BufferUsage::Vertex, false});
    m_cubeIbo = m_device->createBuffer({36 * 4, BufferUsage::Index, false});
}

void EntityRenderDispatcher::drawBox(const glm::mat4& model, const glm::vec3& boxMin, const glm::vec3& boxMax, const glm::vec4& color, const glm::mat4& vp, ICommandList* cmd) {
    cmd->bindPipeline(m_pipeline);
    cmd->setUniformMat4("uMVP", glm::value_ptr(vp));
    cmd->setUniformMat4("uModel", glm::value_ptr(model));
    cmd->setUniform4f("uBoxMin", boxMin.x, boxMin.y, boxMin.z, 0.0f);
    cmd->setUniform4f("uBoxMax", boxMax.x, boxMax.y, boxMax.z, 0.0f);
    cmd->setUniform4f("uColor", color.r, color.g, color.b, color.a);
    cmd->bindVertexBuffer(m_cubeVbo, 12);
    cmd->bindIndexBuffer(m_cubeIbo, false);
    cmd->drawIndexed(36, 0);
}

void EntityRenderDispatcher::drawHumanoid(const Entity& entity, const glm::mat4& vp, const glm::vec4& headCol, const glm::vec4& bodyCol, const glm::vec4& armCol, const glm::vec4& legCol, bool skeleton, float customScale, ICommandList* cmd) {
    bool crouching = entity.getMetadataFlag(0, 1);
    bool invisible = entity.getMetadataFlag(0, 5);
    float alpha = invisible ? 0.2f : 1.0f;
    constexpr float S = 0.9375f / 16.0f;
    glm::mat4 model(1.0f);
    model = glm::translate(model, glm::vec3((float)entity.position.x, (float)entity.position.y, (float)entity.position.z));
    model = glm::rotate(model, glm::radians(180.0f - entity.yaw), {0,1,0});
    model = glm::translate(model, {0, (crouching ? 21.0f : 24.0f) * S * customScale, 0});
    model = glm::scale(model, {S * customScale, -S * customScale, S * customScale});

    auto c = [&](glm::vec4 base) { glm::vec4 r = base; r.a *= alpha; return r; };
    struct Part { glm::vec3 min, max; glm::vec4 color; glm::vec3 offset{0}; float xRot{0}; };

    Part wide[] = {
        {{-4,-8,-4}, {4,0,4}, c(headCol)}, {{-4,0,-2}, {4,12,2}, c(bodyCol)},
        {{-8,0,-2}, {-4,12,2}, c(armCol), {0,2,0}}, {{4,0,-2}, {8,12,2}, c(armCol), {0,2,0}},
        {{-3.9f,0,-2}, {0.1f,12,2}, c(legCol), {-2,12,0}}, {{-0.1f,0,-2}, {3.9f,12,2}, c(legCol), {2,12,0}}
    };

    float walk = entity.walkAnimPos * 0.6662f, walkSpeed = entity.walkAnimSpeed, swing = 0.0f;
    if (entity.swingTime > 0) swing = std::sin(((float)entity.swingTime/6.0f)*3.14159f)*1.2f;

    if (crouching) { wide[0].offset={0,4.2f,0}; wide[1].offset={0,3.2f,0}; wide[1].xRot=0.5f; wide[2].offset={0,5.2f,0}; wide[2].xRot=0.4f-swing; wide[3].offset={0,5.2f,0}; wide[3].xRot=0.4f; wide[4].offset={-2,12.2f,0}; wide[5].offset={2,12.2f,0}; }
    else { wide[2].xRot = std::cos(walk+3.14159f)*1.4f*walkSpeed-swing; wide[3].xRot = std::cos(walk)*1.4f*walkSpeed; wide[4].xRot = std::cos(walk)*1.4f*walkSpeed; wide[5].xRot = std::cos(walk+3.14159f)*1.4f*walkSpeed; }

    for (int i=0; i<6; ++i) {
        glm::mat4 pM = glm::translate(model, wide[i].offset);
        if (wide[i].xRot != 0) pM = glm::rotate(pM, wide[i].xRot, {1,0,0});
        drawBox(pM, wide[i].min, wide[i].max, wide[i].color, vp, cmd);
    }
}

void EntityRenderDispatcher::renderEntities(ICommandList* cmd, const glm::mat4& viewProjection) {
    static bool cubeUploaded = false;
    if (!cubeUploaded) {
        static constexpr float v[] = {0,0,0, 1,0,0, 1,1,0, 0,1,0, 0,0,1, 1,0,1, 1,1,1, 0,1,1};
        static constexpr uint32_t idx[] = {0,1,2,0,2,3, 4,6,5,4,7,6, 0,4,5,0,5,1, 3,2,6,3,6,7, 1,5,6,1,6,2, 0,3,7,0,7,4};
        cmd->uploadBuffer(m_cubeVbo, v, sizeof(v)); cmd->uploadBuffer(m_cubeIbo, idx, sizeof(idx));
        cubeUploaded = true;
    }

    for (const auto& [id, entity] : g_entities) {
        if (!entity || entity->removed) continue;
        if (entity->type == EntityType::PLAYER || entity->type == EntityType::ZOMBIE) {
            drawHumanoid(*entity, viewProjection, {0.32,0.56,0.28,1}, {0.18,0.52,0.62,1}, {0.32,0.56,0.28,1}, {0.25,0.20,0.55,1}, false, 1.0f, cmd);
        } else {
             const glm::dvec3 minD = entity->getBBMin(), maxD = entity->getBBMax();
             drawBox(glm::mat4(1.0f), {(float)minD.x,(float)minD.y,(float)minD.z}, {(float)maxD.x,(float)maxD.y,(float)maxD.z}, colorFor(*entity), viewProjection, cmd);
        }
    }
}

} // namespace mc::render
