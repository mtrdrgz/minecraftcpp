#pragma once
#include "Goal.h"
#include "../Entity.h"
#include <optional>

namespace mc {

// Reference implementation of RandomStrollGoal.
// In a full implementation, this would likely use a PathfinderMob instead of Entity
// to access navigation and pathfinding capabilities.
class RandomStrollGoal : public Goal {
public:
    static constexpr int DEFAULT_INTERVAL = 120;

    RandomStrollGoal(Entity* entity, double speedModifier, int interval = DEFAULT_INTERVAL, bool checkNoActionTime = true);

    bool canUse() override;
    bool canContinueToUse() override;
    void start() override;
    void stop() override;

    void trigger();
    void setInterval(int interval);

protected:
    virtual std::optional<glm::dvec3> getPosition();

    Entity* m_entity;
    double m_wantedX = 0;
    double m_wantedY = 0;
    double m_wantedZ = 0;
    double m_speedModifier;
    int m_interval;
    bool m_forceTrigger = false;
    bool m_checkNoActionTime;
};

} // namespace mc
