#include "ThreadPool.h"

#include <algorithm>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#ifdef _WIN32
#include <windows.h>
#else
#include "platform/Platform.h"
#endif
#endif

namespace mc {

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    // Terrain generation is CPU-heavy and currently competes with render/input.
    // Keep the streaming pool deliberately small; correctness is unchanged, only
    // how aggressively we spend spare CPU while chunks stream in.
    const size_t workerCap = threads >= 7 ? 2 : 1;
    const size_t workerCount = std::max<size_t>(1, std::min<size_t>(threads, workerCap));
    for (size_t i = 0; i < workerCount; ++i) {
        workers.emplace_back([this]() {
#ifdef _WIN32
            SetThreadPriority(GetCurrentThread(), THREAD_MODE_BACKGROUND_BEGIN);
#endif
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
