#include "RandomStrollGoal.h"
#include <random>

namespace mc {

RandomStrollGoal::RandomStrollGoal(Entity* entity, double speedModifier, int interval, bool checkNoActionTime)
    : m_entity(entity), m_speedModifier(speedModifier), m_interval(interval), m_checkNoActionTime(checkNoActionTime) {
    setFlags({Flag::MOVE});
}

bool RandomStrollGoal::canUse() {
    if (m_entity->removed) {
        return false;
    }

    if (!m_forceTrigger) {
        // Mocking mob.getNoActionTime() and mob.getRandom()
        // In a real implementation, we would use the entity's random generator
        static std::mt19937 gen(std::random_device{}());
        
        if (m_checkNoActionTime) {
            // Assuming 0 for now as we don't have noActionTime
        }

        int delay = adjustedTickDelay(m_interval, requiresUpdateEveryTick());
        std::uniform_int_distribution<> dis(0, delay - 1);
        if (dis(gen) != 0) {
            return false;
        }
    }

    auto pos = getPosition();
    if (!pos) {
        return false;
    }

    m_wantedX = pos->x;
    m_wantedY = pos->y;
    m_wantedZ = pos->z;
    m_forceTrigger = false;
    return true;
}

bool RandomStrollGoal::canContinueToUse() {
    // In Minecraft: return !this.mob.getNavigation().isDone() && !this.mob.hasControllingPassenger();
    // Since we don't have navigation yet, we'll just return false to make it a one-shot or mock it.
    return false; 
}

void RandomStrollGoal::start() {
    // In Minecraft: this.mob.getNavigation().moveTo(this.wantedX, this.wantedY, this.wantedZ, this.speedModifier);
    // For now, we just update the entity position directly or just log it.
    m_entity->setPosition(m_wantedX, m_wantedY, m_wantedZ);
}

void RandomStrollGoal::stop() {
    // In Minecraft: this.mob.getNavigation().stop();
}

void RandomStrollGoal::trigger() {
    m_forceTrigger = true;
}

void RandomStrollGoal::setInterval(int interval) {
    m_interval = interval;
}

std::optional<glm::dvec3> RandomStrollGoal::getPosition() {
    // Mocking DefaultRandomPos.getPos(this.mob, 10, 7)
    static std::mt19937 gen(std::random_device{}());
    std::uniform_real_distribution<> dis(-10.0, 10.0);
    
    return glm::dvec3(
        m_entity->position.x + dis(gen),
        m_entity->position.y + dis(gen) * 0.7, // 7 is vertical range in MC
        m_entity->position.z + dis(gen)
    );
}

} // namespace mc
