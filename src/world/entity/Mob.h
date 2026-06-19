#pragma once
#include "Entity.h"
#include "ai/GoalSelector.h"

namespace mc {

// Port of net.minecraft.world.entity.Mob
class Mob : public Entity {
public:
    Mob(int32_t id, EntityType type);
    ~Mob() override = default;

    void tick() override;

    GoalSelector& goalSelector() { return m_goalSelector; }
    GoalSelector& targetSelector() { return m_targetSelector; }

protected:
    GoalSelector m_goalSelector;
    GoalSelector m_targetSelector;
};

} // namespace mc
