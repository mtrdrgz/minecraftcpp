#include "GoalSelector.h"
#include <algorithm>

namespace mc {

void GoalSelector::addGoal(int priority, std::unique_ptr<Goal> goal) {
    m_availableGoals.push_back(std::make_unique<WrappedGoal>(priority, std::move(goal)));
}

void GoalSelector::removeGoal(Goal* goal) {
    auto it = std::remove_if(m_availableGoals.begin(), m_availableGoals.end(), [goal](const std::unique_ptr<WrappedGoal>& wrapped) {
        if (wrapped->getGoal() == goal) {
            if (wrapped->isRunning()) {
                wrapped->stop();
            }
            return true;
        }
        return false;
    });
    m_availableGoals.erase(it, m_availableGoals.end());
}

void GoalSelector::tick() {
    // 1. Cleanup: stop goals that can no longer run
    for (auto& goal : m_availableGoals) {
        if (goal->isRunning() && (goalContainsAnyFlags(*goal, m_disabledFlags) || !goal->canContinueToUse())) {
            goal->stop();
        }
    }

    // 2. Remove inactive goals from locked flags
    for (auto it = m_lockedFlags.begin(); it != m_lockedFlags.end(); ) {
        if (!it->second->isRunning()) {
            it = m_lockedFlags.erase(it);
        } else {
            ++it;
        }
    }

    // 3. Start new goals
    for (auto& goal : m_availableGoals) {
        if (!goal->isRunning() && !goalContainsAnyFlags(*goal, m_disabledFlags) && 
            goalCanBeReplacedForAllFlags(*goal, m_lockedFlags) && goal->canUse()) {
            
            for (auto flag : goal->getFlags()) {
                auto it = m_lockedFlags.find(flag);
                if (it != m_lockedFlags.end()) {
                    it->second->stop();
                }
                m_lockedFlags[flag] = goal.get();
            }
            goal->start();
        }
    }

    tickRunningGoals(true);
}

void GoalSelector::tickRunningGoals(bool forceTickAllRunningGoals) {
    for (auto& goal : m_availableGoals) {
        if (goal->isRunning() && (forceTickAllRunningGoals || goal->requiresUpdateEveryTick())) {
            goal->tick();
        }
    }
}

void GoalSelector::setControlFlag(Goal::Flag flag, bool enabled) {
    if (enabled) {
        enableControlFlag(flag);
    } else {
        disableControlFlag(flag);
    }
}

void GoalSelector::enableControlFlag(Goal::Flag flag) {
    m_disabledFlags.erase(flag);
}

void GoalSelector::disableControlFlag(Goal::Flag flag) {
    m_disabledFlags.insert(flag);
}

bool GoalSelector::goalContainsAnyFlags(const WrappedGoal& goal, const std::set<Goal::Flag>& disabledFlags) const {
    for (auto flag : goal.getFlags()) {
        if (disabledFlags.count(flag)) {
            return true;
        }
    }
    return false;
}

bool GoalSelector::goalCanBeReplacedForAllFlags(const WrappedGoal& goal, const std::map<Goal::Flag, WrappedGoal*>& lockedFlags) const {
    for (auto flag : goal.getFlags()) {
        auto it = lockedFlags.find(flag);
        if (it != lockedFlags.end()) {
            if (!it->second->canBeReplacedBy(goal)) {
                return false;
            }
        }
    }
    return true;
}

} // namespace mc
