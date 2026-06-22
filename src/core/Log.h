#pragma once
#include <cstdio>
#include <string_view>
#include <format>

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

template<class... Args>
inline void print(Level level, std::string_view fmt, Args&&... args) {
    std::string msg = std::vformat(fmt, std::make_format_args(args...));
    std::string line = std::format("[{}] {}\n", levelStr(level), msg);
    fputs(line.c_str(), level == Level::Error ? stderr : stdout);
#if defined(_WIN32)
    OutputDebugStringA(line.c_str());  // visible in VS debugger output
#endif
}

} // namespace mc::log

#define MC_LOG_DEBUG(...) mc::log::print(mc::log::Level::Debug, __VA_ARGS__)
#define MC_LOG_INFO(...)  mc::log::print(mc::log::Level::Info,  __VA_ARGS__)
#define MC_LOG_WARN(...)  mc::log::print(mc::log::Level::Warn,  __VA_ARGS__)
#define MC_LOG_ERROR(...) mc::log::print(mc::log::Level::Error, __VA_ARGS__)
