#include "ThreadPool.h"

namespace mc {

ThreadPool::ThreadPool(size_t threads) : stop(false) {
    // Use the requested thread count directly. The previous code capped the
    // worker count to 1 (or 2 if threads >= 7), which defeated the purpose of
    // having a thread pool on multi-core CPUs. The cap was originally added to
    // avoid competing with the render thread, but on modern multi-core CPUs
    // (e.g. R9 9950x with 32 threads) capping to 1-2 workers means the mesh
    // build pipeline is serialized while 30 cores sit idle.
    //
    // The caller (LevelRenderer) now passes a reasonable count based on
    // hardware_concurrency, leaving headroom for the render thread.
    const size_t workerCount = std::max<size_t>(1, threads);
    for (size_t i = 0; i < workerCount; ++i) {
        workers.emplace_back([this]() {
            // NOTE: The previous code called SetThreadPriority(
            // THREAD_MODE_BACKGROUND_BEGIN) on Windows. This tells the Windows
            // scheduler to run these threads at BELOW-NORMAL priority, which
            // means ANY foreground thread (including the render thread, input
            // handling, OS background tasks) preempts them. On a CPU with many
            // cores (16+), the scheduler spreads threads across cores but still
            // yields the background threads whenever anything else needs CPU —
            // causing constant context switches and cache thrashing. On a
            // 2-core Celeron, there's nothing to yield TO, so the background
            // threads run uninterrupted. This is why the Celeron was faster.
            // Removed: let the threads run at default (NORMAL) priority.
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
