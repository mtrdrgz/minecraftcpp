#pragma once
#include "Log.h"
#include <chrono>
#include <string>
#include <unordered_map>
#include <vector>
#include <algorithm>
#include <numeric>
#include <mutex>

namespace mc::debug {

// ── FrameProfiler ────────────────────────────────────────────────────────────
// A lightweight always-on profiler that records timing for named sections
// within a single frame, then dumps a summary to mcpp.log every N frames.
//
// Usage:
//   FrameProfiler::Section prof("renderLevel");
//   ... code to time ...
//   // prof auto-records on destruction
//
// Or manual:
//   FrameProfiler::begin("mesh.snapshot");
//   ...
//   FrameProfiler::end("mesh.snapshot");
//
// At the end of each frame, call FrameProfiler::endFrame(). Every 60 frames
// (1 second at 60fps), it logs a summary like:
//   [12:34:56.789] PROF frame=60 avg=16.2ms max=45.1ms
//     renderLevel     avg=8.1ms  max=32.0ms  count=60
//     mesh.snapshot   avg=2.3ms  max=5.1ms   count=12
//     mesh.build      avg=15.1ms max=28.3ms  count=12
//     decorate        avg=0.0ms  max=0.0ms   count=0
//     tick            avg=0.1ms  max=0.5ms   count=60
//     gpu.endFrame    avg=3.2ms  max=8.1ms   count=60
//
// This makes it immediately obvious which section is slow when the game
// hangs or stutters. The overhead is <1μs per section (just a chrono::now
// + map lookup), so it's safe to leave on in release builds.

class FrameProfiler {
public:
    static FrameProfiler& instance() {
        static FrameProfiler s;
        return s;
    }

    // RAII scoped timer — the most common usage.
    class Section {
    public:
        Section(const char* name) : m_name(name), m_start(now()) {}
        ~Section() {
            double ms = std::chrono::duration<double, std::milli>(now() - m_start).count();
            instance().record(m_name, ms);
        }
        // Non-copyable
        Section(const Section&) = delete;
        Section& operator=(const Section&) = delete;
    private:
        const char* m_name;
        std::chrono::steady_clock::time_point m_start;
    };

    // Manual begin/end (for cases where RAII is awkward).
    static void begin(const char* name) {
        instance().m_starts[name] = now();
    }

    static void end(const char* name) {
        auto it = instance().m_starts.find(name);
        if (it == instance().m_starts.end()) return;
        double ms = std::chrono::duration<double, std::milli>(now() - it->second).count();
        instance().record(name, ms);
        instance().m_starts.erase(it);
    }

    // Call once at the end of each frame. Every `dumpInterval` frames, logs
    // a summary to mcpp.log.
    void endFrame() {
        m_frameCount++;
        double frameMs = std::chrono::duration<double, std::milli>(
            now() - m_frameStart).count();
        m_frameTimes.push_back(frameMs);
        if (m_frameTimes.size() > 120) m_frameTimes.erase(m_frameTimes.begin());

        if (m_frameCount % m_dumpInterval == 0) {
            dumpSummary();
        }
        m_frameStart = now();
        // Reset section accumulators for the next interval.
        m_sectionData.clear();
    }

    void setDumpInterval(int frames) { m_dumpInterval = std::max(1, frames); }

    // Dump the current summary immediately (called by the hang watchdog).
    void dumpNow() {
        dumpSummary();
        m_sectionData.clear();
    }

    // Get the last frame time in ms (for the watchdog / slow frame logging).
    double lastFrameMs() const {
        return m_frameTimes.empty() ? 0.0 : m_frameTimes.back();
    }

private:
    FrameProfiler() : m_frameStart(now()) {}

    struct SectionData {
        double totalMs = 0.0;
        double maxMs = 0.0;
        int count = 0;
    };

    void record(const char* name, double ms) {
        auto& d = m_sectionData[name];
        d.totalMs += ms;
        if (ms > d.maxMs) d.maxMs = ms;
        d.count++;
    }

    void dumpSummary() {
        // Compute frame stats.
        double avgFrame = 0.0, maxFrame = 0.0;
        if (!m_frameTimes.empty()) {
            double sum = std::accumulate(m_frameTimes.begin(), m_frameTimes.end(), 0.0);
            avgFrame = sum / m_frameTimes.size();
            maxFrame = *std::max_element(m_frameTimes.begin(), m_frameTimes.end());
        }

        // Log the frame summary.
        mc::log::FileLogger::instance().write(mc::log::Level::Info,
            std::format("PROF frame={} avg={:.1f}ms max={:.1f}ms fps={:.0f}",
                m_frameCount, avgFrame, maxFrame,
                avgFrame > 0 ? 1000.0 / avgFrame : 0.0));

        // Log each section sorted by total time (descending).
        std::vector<std::pair<std::string, SectionData>> sorted(
            m_sectionData.begin(), m_sectionData.end());
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second.totalMs > b.second.totalMs; });

        for (const auto& [name, d] : sorted) {
            double avg = d.count > 0 ? d.totalMs / d.count : 0.0;
            mc::log::FileLogger::instance().write(mc::log::Level::Debug,
                std::format("  {:<28} avg={:6.1f}ms max={:6.1f}ms count={}",
                    name, avg, d.maxMs, d.count));
        }
    }

    static std::chrono::steady_clock::time_point now() {
        return std::chrono::steady_clock::now();
    }

    int m_frameCount = 0;
    int m_dumpInterval = 60;  // dump every 60 frames (~1s at 60fps)
    std::chrono::steady_clock::time_point m_frameStart;
    std::vector<double> m_frameTimes;  // rolling window of last 120 frame times

    std::unordered_map<std::string, SectionData> m_sectionData;
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> m_starts;
};

} // namespace mc::debug

// Convenience macro: mc::debug::FrameProfiler::Section prof("name");
// becomes: FRAME_PROF("name");
#define FRAME_PROF(name) mc::debug::FrameProfiler::Section _fp_##name(#name)
