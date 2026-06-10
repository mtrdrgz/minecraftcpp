#include "Profiler.h"
#include <algorithm>
#include <iomanip>
#include <iostream>
#include <thread>

namespace mc::profiling {

Profiler::Profiler()
    : m_profileStart(std::chrono::high_resolution_clock::now()) {
}

void Profiler::beginEvent(const std::string& eventName, const std::string& chunkKey) {
    std::lock_guard<std::mutex> lock(m_mutex);
    std::string key = eventName;
    m_activeTimings[key] = {
        eventName,
        chunkKey,
        std::chrono::high_resolution_clock::now()
    };
}

void Profiler::endEvent(const std::string& eventName) {
    auto now = std::chrono::high_resolution_clock::now();
    std::lock_guard<std::mutex> lock(m_mutex);
    
    auto it = m_activeTimings.find(eventName);
    if (it == m_activeTimings.end()) {
        return;  // Event not started or already ended
    }

    const auto& state = it->second;
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(now - state.startTime);
    double durationMs = duration.count() / 1000.0;
    double timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::high_resolution_clock::now() - m_profileStart
    ).count() / 1000.0;

    std::ostringstream threadIdStream;
    threadIdStream << std::this_thread::get_id();
    std::string threadId = threadIdStream.str();

    m_timings.push_back({
        state.eventName,
        state.chunkKey,
        durationMs,
        timestamp,
        threadId
    });

    m_activeTimings.erase(it);
}

void Profiler::flushToCSV(const std::string& filename) {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Failed to open profiling output: " << filename << std::endl;
        return;
    }

    // CSV header
    file << "Timestamp(s),Event,ChunkKey,Duration(ms),ThreadID\n";

    // Sort by timestamp for readability
    std::vector<TimingEntry> sorted = m_timings;
    std::sort(sorted.begin(), sorted.end(), 
        [](const TimingEntry& a, const TimingEntry& b) {
            return a.timestamp < b.timestamp;
        });

    for (const auto& entry : sorted) {
        file << std::fixed << std::setprecision(3) << entry.timestamp << ","
             << entry.eventName << ","
             << entry.chunkKey << ","
             << std::fixed << std::setprecision(3) << entry.durationMs << ","
             << entry.threadId << "\n";
    }

    file.close();
    std::cout << "Profiling data written to: " << filename << std::endl;
}

std::vector<Profiler::Stats> Profiler::getStats() const {
    std::lock_guard<std::mutex> lock(m_mutex);
    
    std::unordered_map<std::string, Stats> statsMap;
    
    for (const auto& entry : m_timings) {
        auto& stat = statsMap[entry.eventName];
        stat.eventName = entry.eventName;
        stat.count++;
        stat.totalMs += entry.durationMs;
        stat.minMs = std::min(stat.minMs, entry.durationMs);
        stat.maxMs = std::max(stat.maxMs, entry.durationMs);
    }

    // Compute averages
    for (auto& [name, stat] : statsMap) {
        stat.avgMs = stat.count > 0 ? stat.totalMs / stat.count : 0.0;
    }

    std::vector<Stats> result;
    for (auto& [name, stat] : statsMap) {
        result.push_back(stat);
    }
    
    // Sort by total time descending
    std::sort(result.begin(), result.end(),
        [](const Stats& a, const Stats& b) {
            return a.totalMs > b.totalMs;
        });

    return result;
}

void Profiler::printStats() const {
    auto stats = getStats();
    
    std::cout << "\n===== PROFILING STATISTICS =====\n";
    std::cout << std::left << std::setw(25) << "Event"
              << std::right << std::setw(10) << "Count"
              << std::setw(15) << "Total(ms)"
              << std::setw(15) << "Avg(ms)"
              << std::setw(15) << "Min(ms)"
              << std::setw(15) << "Max(ms)\n";
    std::cout << std::string(95, '-') << "\n";

    for (const auto& stat : stats) {
        std::cout << std::left << std::setw(25) << stat.eventName
                  << std::right << std::setw(10) << stat.count
                  << std::setw(15) << std::fixed << std::setprecision(2) << stat.totalMs
                  << std::setw(15) << std::fixed << std::setprecision(2) << stat.avgMs
                  << std::setw(15) << std::fixed << std::setprecision(2) << stat.minMs
                  << std::setw(15) << std::fixed << std::setprecision(2) << stat.maxMs << "\n";
    }
    std::cout << std::string(95, '=') << "\n";
}

} // namespace mc::profiling
