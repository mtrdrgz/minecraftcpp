#pragma once
#include <cstdint>
#include <set>
#include <vector>
#include <memory>

namespace mc {

class Goal {
public:
    enum class Flag : uint8_t {
        MOVE = 0,
        LOOK = 1,
        JUMP = 2,
        TARGET = 3
    };

    virtual ~Goal() = default;

    virtual bool canUse() = 0;
    virtual bool canContinueToUse() { return canUse(); }
    virtual bool isInterruptible() const { return true; }
    
    virtual void start() {}
    virtual void stop() {}
    virtual bool requiresUpdateEveryTick() const { return false; }
    virtual void tick() {}

    void setFlags(const std::set<Flag>& flags) {
        m_flags = flags;
    }

    const std::set<Flag>& getFlags() const {
        return m_flags;
    }

protected:
    static int adjustedTickDelay(int ticks, bool requiresUpdateEveryTick) {
        return requiresUpdateEveryTick ? ticks : (ticks + 1) / 2;
    }

private:
    std::set<Flag> m_flags;
};

class WrappedGoal : public Goal {
public:
    WrappedGoal(int priority, std::unique_ptr<Goal> goal);

    bool canUse() override { return m_goal->canUse(); }
    bool canContinueToUse() override { return m_goal->canContinueToUse(); }
    bool isInterruptible() const override { return m_goal->isInterruptible(); }
    
    void start() override;
    void stop() override;
    bool requiresUpdateEveryTick() const override { return m_goal->requiresUpdateEveryTick(); }
    void tick() override { m_goal->tick(); }

    bool isRunning() const { return m_isRunning; }
    int getPriority() const { return m_priority; }
    Goal* getGoal() const { return m_goal.get(); }

    bool canBeReplacedBy(const WrappedGoal& other) const;

private:
    std::unique_ptr<Goal> m_goal;
    int m_priority;
    bool m_isRunning = false;
};

} // namespace mc
