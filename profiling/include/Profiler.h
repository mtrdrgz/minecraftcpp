#pragma once

#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>
#include <sstream>
#include <atomic>
#include <mutex>

namespace mc::profiling {

// Thread-safe profiling system for measuring chunk generation performance
class Profiler {
public:
    struct TimingEntry {
        std::string eventName;
        std::string chunkKey;  // "cx,cz" or "stage"
        double durationMs;
        double timestamp;      // Wall clock seconds since profiler started
        std::string threadId;
    };

    static Profiler& instance() {
        static Profiler prof;
        return prof;
    }

    Profiler();

    // Start recording time for a named event
    void beginEvent(const std::string& eventName, const std::string& chunkKey = "");
    
    // End the event and record timing
    void endEvent(const std::string& eventName);

    // RAII scoped timer
    class ScopedTimer {
    public:
        ScopedTimer(const std::string& name, const std::string& chunkKey = "")
            : m_name(name), m_chunkKey(chunkKey) {
            Profiler::instance().beginEvent(m_name, m_chunkKey);
        }
        ~ScopedTimer() {
            Profiler::instance().endEvent(m_name);
        }
    private:
        std::string m_name;
        std::string m_chunkKey;
    };

    // Flush all recorded timings to CSV
    void flushToCSV(const std::string& filename);

    // Get stats summary
    struct Stats {
        std::string eventName;
        int count = 0;
        double totalMs = 0.0;
        double minMs = 1e9;
        double maxMs = 0.0;
        double avgMs = 0.0;
    };
    
    std::vector<Stats> getStats() const;
    void printStats() const;

private:
    struct TimingState {
        std::string eventName;
        std::string chunkKey;
        std::chrono::high_resolution_clock::time_point startTime;
    };

    std::chrono::high_resolution_clock::time_point m_profileStart;
    std::unordered_map<std::string, TimingState> m_activeTimings;  // per-thread, keyed by event name
    std::vector<TimingEntry> m_timings;
    mutable std::mutex m_mutex;
};

// Macro for easier usage
#define PROFILE_SCOPE(name) \
    mc::profiling::Profiler::ScopedTimer _profile_timer_##__LINE__(name)

#define PROFILE_SCOPE_CHUNK(name, cx, cz) \
    std::string _chunk_key = std::to_string(cx) + "," + std::to_string(cz); \
    mc::profiling::Profiler::ScopedTimer _profile_timer_##__LINE__(name, _chunk_key)

} // namespace mc::profiling
