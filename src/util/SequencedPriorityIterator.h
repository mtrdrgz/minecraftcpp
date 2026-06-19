#pragma once

// 1:1 port of net.minecraft.util.SequencedPriorityIterator<T> (Minecraft 26.1.2).
// A Guava AbstractIterator that yields elements highest-priority-first, and within a
// priority in FIFO insertion order. Used by the chunk task scheduler. Fields and method
// bodies are translated VERBATIM from
// 26.1.2/src/net/minecraft/util/SequencedPriorityIterator.java:
//
//   private static final int MIN_PRIO = Integer.MIN_VALUE;
//   private @Nullable Deque<T> highestPrioQueue = null;
//   private int highestPrio = Integer.MIN_VALUE;
//   private final Int2ObjectMap<Deque<T>> queuesByPriority = new Int2ObjectOpenHashMap();
//
//   public void add(final T data, final int priority) {
//      if (priority == this.highestPrio && this.highestPrioQueue != null) {
//         this.highestPrioQueue.addLast(data);
//      } else {
//         Deque<T> queue = this.queuesByPriority.computeIfAbsent(priority, order -> newArrayDeque());
//         queue.addLast(data);
//         if (priority >= this.highestPrio) {
//            this.highestPrioQueue = queue;
//            this.highestPrio = priority;
//         }
//      }
//   }
//
//   protected @Nullable T computeNext() {
//      if (this.highestPrioQueue == null) return this.endOfData();
//      T result = this.highestPrioQueue.removeFirst();
//      if (result == null) return this.endOfData();
//      if (this.highestPrioQueue.isEmpty()) this.switchCacheToNextHighestPrioQueue();
//      return result;
//   }
//
//   private void switchCacheToNextHighestPrioQueue() {
//      int foundHighestPrio = Integer.MIN_VALUE;
//      Deque<T> foundHighestPrioQueue = null;
//      for (entry : fastIterable(queuesByPriority)) {
//         int prio = entry.getIntKey();
//         Deque<T> queue = entry.getValue();
//         if (prio > foundHighestPrio && !queue.isEmpty()) {
//            foundHighestPrio = prio;
//            foundHighestPrioQueue = queue;
//            if (prio == this.highestPrio - 1) break;
//         }
//      }
//      this.highestPrio = foundHighestPrio;
//      this.highestPrioQueue = foundHighestPrioQueue;
//   }
//
// 1:1 TRAPS reproduced here:
//  * highestPrio is a signed 32-bit int seeded at INT32_MIN. The "priority >= highestPrio"
//    and "prio > foundHighestPrio" comparisons are signed; the very first add (any
//    priority, including INT32_MIN) installs that queue as the cache.
//  * The "prio == this.highestPrio - 1" early-break uses int subtraction that WRAPS:
//    when highestPrio == INT32_MIN, highestPrio - 1 == INT32_MAX (Java int overflow).
//    We reproduce that with int32 two's-complement wrap, never a widened compare.
//  * Guava AbstractIterator semantics: once computeNext() returns endOfData() the
//    iterator is permanently DONE — a later add() does NOT revive it. removeFirst()
//    on an empty deque throws in Java, so the "result == null" path is unreachable for
//    ArrayDeque (it never stores null and never returns null from removeFirst); we mirror
//    the structure but the live path is: null-cache => endOfData, else pop the front.
//  * The selection in switchCacheToNextHighestPrioQueue picks the maximum priority among
//    NON-EMPTY queues. Priorities are distinct map keys, so the winner is unique and does
//    NOT depend on the (fastutil hash) iteration order — output is fully deterministic.
//    We therefore iterate our own (priority -> queue) map; the early-break is a pure
//    micro-optimization that cannot change which queue is chosen.

#include <cstdint>
#include <deque>
#include <map>
#include <optional>

namespace mc::util {

template <typename T>
class SequencedPriorityIterator {
public:
    // Guava AbstractIterator drives computeNext() through hasNext()/next(). We expose a
    // single nextOrEnd() that returns std::nullopt exactly when Java's next() would throw
    // NoSuchElementException (i.e. hasNext()==false / state==DONE).
    std::optional<T> nextOrEnd() {
        if (done_) {
            return std::nullopt;
        }
        T result;
        if (!computeNext(result)) {
            done_ = true;  // endOfData(): AbstractIterator transitions to DONE permanently.
            return std::nullopt;
        }
        return result;
    }

    // public void add(final T data, final int priority)
    void add(const T& data, int32_t priority) {
        if (priority == highestPrio_ && highestPrioQueue_ != nullptr) {
            highestPrioQueue_->push_back(data);
        } else {
            std::deque<T>& queue = queuesByPriority_[priority];  // computeIfAbsent(newArrayDeque)
            queue.push_back(data);
            if (priority >= highestPrio_) {
                highestPrioQueue_ = &queue;
                highestPrio_ = priority;
            }
        }
    }

private:
    // protected @Nullable T computeNext()  — returns false to signal endOfData().
    bool computeNext(T& out) {
        if (highestPrioQueue_ == nullptr) {
            return false;  // endOfData()
        }

        // ArrayDeque.removeFirst() — the front element (FIFO within a priority).
        out = highestPrioQueue_->front();
        highestPrioQueue_->pop_front();

        if (highestPrioQueue_->empty()) {
            switchCacheToNextHighestPrioQueue();
        }

        return true;
    }

    // private void switchCacheToNextHighestPrioQueue()
    void switchCacheToNextHighestPrioQueue() {
        int32_t foundHighestPrio = INT32_MIN;
        std::deque<T>* foundHighestPrioQueue = nullptr;

        for (auto& entry : queuesByPriority_) {
            int32_t prio = entry.first;
            std::deque<T>& queue = entry.second;
            if (prio > foundHighestPrio && !queue.empty()) {
                foundHighestPrio = prio;
                foundHighestPrioQueue = &queue;
                // Java: if (prio == this.highestPrio - 1) break;  (int subtraction WRAPS)
                if (prio == static_cast<int32_t>(highestPrio_ - 1)) {
                    break;
                }
            }
        }

        highestPrio_ = foundHighestPrio;
        highestPrioQueue_ = foundHighestPrioQueue;
    }

    // Std::map keeps priorities in ascending key order. The chosen queue is the unique
    // max non-empty priority, identical regardless of traversal order (see header notes).
    std::map<int32_t, std::deque<T>> queuesByPriority_;
    std::deque<T>* highestPrioQueue_ = nullptr;
    int32_t highestPrio_ = INT32_MIN;
    bool done_ = false;  // Guava AbstractIterator DONE state (latched).
};

}  // namespace mc::util
