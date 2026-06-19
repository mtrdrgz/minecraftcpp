#include "Goal.h"

namespace mc {

WrappedGoal::WrappedGoal(int priority, std::unique_ptr<Goal> goal)
    : m_goal(std::move(goal)), m_priority(priority) {
}

void WrappedGoal::start() {
    if (!m_isRunning) {
        m_isRunning = true;
        m_goal->start();
    }
}

void WrappedGoal::stop() {
    if (m_isRunning) {
        m_isRunning = false;
        m_goal->stop();
    }
}

bool WrappedGoal::canBeReplacedBy(const WrappedGoal& other) const {
    return isInterruptible() && other.getPriority() < getPriority();
}

} // namespace mc
