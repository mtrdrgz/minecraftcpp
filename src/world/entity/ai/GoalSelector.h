#pragma once
#include "Goal.h"
#include <vector>
#include <map>
#include <set>
#include <memory>
#include <functional>

namespace mc {

class GoalSelector {
public:
    void addGoal(int priority, std::unique_ptr<Goal> goal);
    void removeGoal(Goal* goal);
    
    void tick();
    void tickRunningGoals(bool forceTickAllRunningGoals);

    void setControlFlag(Goal::Flag flag, bool enabled);
    void enableControlFlag(Goal::Flag flag);
    void disableControlFlag(Goal::Flag flag);

private:
    bool goalContainsAnyFlags(const WrappedGoal& goal, const std::set<Goal::Flag>& disabledFlags) const;
    bool goalCanBeReplacedForAllFlags(const WrappedGoal& goal, const std::map<Goal::Flag, WrappedGoal*>& lockedFlags) const;

    std::vector<std::unique_ptr<WrappedGoal>> m_availableGoals;
    std::map<Goal::Flag, WrappedGoal*> m_lockedFlags;
    std::set<Goal::Flag> m_disabledFlags;
};

} // namespace mc
