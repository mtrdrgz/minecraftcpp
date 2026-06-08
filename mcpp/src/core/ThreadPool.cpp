#include "ThreadPool.h"

#include <algorithm>

namespace mc {

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    // Terrain generation is CPU-heavy. Using hardware_concurrency()-1 workers can
    // still starve the render/input thread on typical desktop CPUs. Cap workers so
    // local chunk generation stays asynchronous without making the whole game feel
    // frozen while the queue drains.
    const size_t workerCount = std::max<size_t>(1, std::min<size_t>(threads, 4));
    for (size_t i = 0; i < workerCount; ++i) {
        workers.emplace_back([this]() {
            for (;;) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(this->queue_mutex);
                    this->condition.wait(lock, [this]() {
                        return this->stop || !this->tasks.empty();
                    });
                    if (this->stop && this->tasks.empty()) {
                        return;
                    }
                    task = std::move(this->tasks.front());
                    this->tasks.pop();
                }
                task();
            }
        });
    }
}

ThreadPool::~ThreadPool() {
    {
        std::unique_lock<std::mutex> lock(queue_mutex);
        stop = true;
    }
    condition.notify_all();
    for (std::thread& worker : workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}

} // namespace mc
