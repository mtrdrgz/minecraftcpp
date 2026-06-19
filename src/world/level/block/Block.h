#pragma once
#include "../../../core/ResourceLocation.h"
#include <cstdint>
#include <string>
#include <string_view>

namespace mc {

// Port of net.minecraft.world.level.block.Block
class Block {
public:
    struct Properties {
        float    destroyTime    = 0.0f;
        float    explosionResistance = 0.0f;
        bool     hasCollision   = true;
        bool     isAir          = false;
        bool     isOpaque       = true;  // light-blocking
        bool     isSolid        = true;
        bool     noOcclusion    = false; // transparent faces
        bool     lightEmission  = false;
        uint8_t  lightLevel     = 0;
        bool     isSlab         = false;
        bool     isFluid        = false;
        bool     isLeaves       = false;
    };

    // Per-face texture names into the block atlas.
    // face: 0=+X, 1=-X, 2=+Y, 3=-Y, 4=+Z, 5=-Z
    struct FaceTextures {
        std::string all;   // default for all faces
        std::string top;   // +Y override
        std::string bot;   // -Y override
        std::string side;  // ±X, ±Z override

        const std::string& forFace(int face) const {
            if (face == 2 && !top.empty())  return top;
            if (face == 3 && !bot.empty())  return bot;
            if (face != 2 && face != 3 && !side.empty()) return side;
            return all;
        }
    };

    explicit Block(Properties props) : m_props(props) {}
    virtual ~Block() = default;

    const Properties& properties() const { return m_props; }
    bool     isAir()     const { return m_props.isAir; }
    bool     isOpaque()  const { return m_props.isOpaque && !m_props.noOcclusion; }
    bool     isSolid()   const { return m_props.isSolid; }
    bool     isFluid()   const { return m_props.isFluid; }
    uint8_t  lightLevel() const { return m_props.lightLevel; }

    uint32_t   registryId = 0; // set by Registry
    std::string name;
    uint32_t defaultStateId = 0;
    FaceTextures textures;

protected:
    Properties m_props;
};

} // namespace mc
