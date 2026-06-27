#pragma once
#include <cstdio>
#include <string>
#include <string_view>
#include <format>
#include <fstream>
#include <mutex>
#include <chrono>
#include <atomic>

#ifdef _WIN32
#include <windows.h>
#endif

namespace mc::log {

enum class Level { Debug, Info, Warn, Error };

inline const char* levelStr(Level l) {
    switch (l) {
        case Level::Debug: return "DBG";
        case Level::Info:  return "INF";
        case Level::Warn:  return "WRN";
        case Level::Error: return "ERR";
    }
    return "???";
}

// ── File logger ──────────────────────────────────────────────────────────────
// Writes to mcpp.log next to the executable (or cwd). Auto-flushes after every
// line so crash dumps are never lost. Thread-safe via a mutex.
class FileLogger {
public:
    static FileLogger& instance() {
        static FileLogger s;
        return s;
    }

    // Open the log file. Call once at startup, before any MC_LOG_*.
    // If the file can't be opened, logging silently degrades to stdout-only.
    void open(const std::string& path) {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_file.is_open()) return;  // already open
        m_file.open(path, std::ios::out | std::ios::trunc);
        m_path = path;
        if (m_file.is_open()) {
            // Write a header with build info + timestamp.
            m_file << "=== mcpp.log ===" << std::endl;
            m_file << "opened: " << timestamp() << std::endl;
            m_file << "================" << std::endl;
            m_file.flush();
        }
    }

    bool isOpen() const { return m_file.is_open(); }

    // Write a line to BOTH stdout and the file (if open). Flushes immediately
    // so a crash never loses the last few lines.
    void write(Level level, std::string_view msg) {
        std::string line = std::format("[{}] {} {}\n", timestamp(), levelStr(level), msg);
        // stdout/stderr
        fputs(line.c_str(), level == Level::Error ? stderr : stdout);
#if defined(_WIN32)
        OutputDebugStringA(line.c_str());
#endif
        // file
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_file.is_open()) {
            m_file << line;
            m_file.flush();  // critical: flush every line so crashes don't lose data
        }
    }

    // Direct write (no formatting) — used by the crash handler to write a
    // raw string without going through vformat (which could itself crash
    // if the heap is corrupted).
    void writeRaw(std::string_view msg) {
        std::string line = std::format("[{}] FATAL {}\n", timestamp(), msg);
        fputs(line.c_str(), stderr);
#if defined(_WIN32)
        OutputDebugStringA(line.c_str());
#endif
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_file.is_open()) {
            m_file << line;
            m_file.flush();
        }
    }

    // Flush the file — called by the crash handler after writing the backtrace.
    void flush() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (m_file.is_open()) m_file.flush();
    }

    const std::string& path() const { return m_path; }

private:
    FileLogger() = default;

    static std::string timestamp() {
        using namespace std::chrono;
        auto now = system_clock::now();
        auto t = system_clock::to_time_t(now);
        auto ms = duration_cast<milliseconds>(now.time_since_epoch()) % 1000;
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &t);
#else
        localtime_r(&t, &tm);
#endif
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d.%03d",
                       tm.tm_hour, tm.tm_min, tm.tm_sec, (int)ms.count());
        return buf;
    }

    std::ofstream m_file;
    std::string   m_path;
    std::mutex    m_mutex;
};

template<class... Args>
inline void print(Level level, std::string_view fmt, Args&&... args) {
    std::string msg = std::vformat(fmt, std::make_format_args(args...));
    FileLogger::instance().write(level, msg);
}

} // namespace mc::log

#define MC_LOG_DEBUG(...) mc::log::print(mc::log::Level::Debug, __VA_ARGS__)
#define MC_LOG_INFO(...)  mc::log::print(mc::log::Level::Info,  __VA_ARGS__)
#define MC_LOG_WARN(...)  mc::log::print(mc::log::Level::Warn,  __VA_ARGS__)
#define MC_LOG_ERROR(...) mc::log::print(mc::log::Level::Error, __VA_ARGS__)
