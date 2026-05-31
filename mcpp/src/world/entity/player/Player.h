#pragma once
#include "../Entity.h"

#include <string>

namespace mc {

// Minimal client-side port of net.minecraft.world.entity.player.Player.
// Full inventory, food, abilities, and interaction logic remain Phase 15 work;
// this class exists so player entities can be distinguished from generic
// entities in networking and rendering.
class Player : public Entity {
public:
    explicit Player(int32_t id);

    std::string profileName;
    int32_t score = 0;
    int32_t experienceLevel = 0;
    int32_t totalExperience = 0;
    float experienceProgress = 0.0f;
    bool reducedDebugInfo = false;
};

} // namespace mc
