#include "CrashHandler.h"
#include "FrameProfiler.h"
#include "Log.h"
#include <atomic>
#include <chrono>
#include <csignal>
#include <cstdlib>
#include <cstring>
#include <thread>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dbghelp.h>
#include <crtdbg.h>
#pragma comment(lib, "dbghelp.lib")
#else
#include <execinfo.h>
#include <unistd.h>
#include <syscall.h>
#endif

namespace mc::debug {

// ── Frame heartbeat + phase tracking (for hang detection) ────────────────────
static std::atomic<int64_t> g_lastFrameMs{0};  // ms since epoch of last heartbeat
static std::atomic<const char*> g_phase{"startup"};
static std::atomic<bool> g_watchdogRunning{false};
static std::thread g_watchdogThread;

static int64_t nowMs() {
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

void frameHeartbeat() {
    g_lastFrameMs.store(nowMs(), std::memory_order_relaxed);
}

void setPhase(const char* phase) {
    g_phase.store(phase ? phase : "?", std::memory_order_relaxed);
}

// ── Stack trace (best-effort) ────────────────────────────────────────────────
static void writeStackTrace() {
    mc::log::FileLogger::instance().writeRaw("--- stack trace (best-effort) ---");
#ifdef _WIN32
    // Capture stack frames using CaptureStackBackTrace (simple, no symbols
    // needed at runtime — SymFromAddr needs dbghelp but we don't have PDBs
    // in the CI build, so we just log the addresses).
    void* frames[64];
    USHORT n = CaptureStackBackTrace(0, 64, frames, nullptr);
    for (USHORT i = 0; i < n; ++i) {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "  [%2d] %p", i, frames[i]);
        mc::log::FileLogger::instance().writeRaw(buf);
    }
#else
    // Linux: backtrace() + backtrace_symbols().
    void* frames[64];
    int n = backtrace(frames, 64);
    char** syms = backtrace_symbols(frames, n);
    if (syms) {
        for (int i = 0; i < n; ++i) {
            mc::log::FileLogger::instance().writeRaw(std::string("  ") + syms[i]);
        }
        free(syms);
    }
#endif
    mc::log::FileLogger::instance().writeRaw("--- end stack trace ---");
}

// ── Crash handler ────────────────────────────────────────────────────────────
// This function is called when a fatal signal/exception is raised. It writes
// the crash info to mcpp.log, flushes, then re-raises the signal so the OS
// can produce a core dump (Linux) or WER crash dump (Windows).
//
// IMPORTANT: this function must be async-signal-safe as much as possible.
// We use snprintf (not std::format) and write() to avoid heap allocation.
// The FileLogger uses a mutex, which is technically not async-signal-safe,
// but in practice it works because the crash is usually on the main thread
// and the mutex is only held briefly.
static void crashHandler(const char* reason) {
    mc::log::FileLogger::instance().writeRaw("========================================");
    mc::log::FileLogger::instance().writeRaw("!!! CRASH DETECTED !!!");
    mc::log::FileLogger::instance().writeRaw(std::string("reason: ") + reason);
    // Thread ID
    char tid[64];
#ifdef _WIN32
    std::snprintf(tid, sizeof(tid), "thread id: %lu", (unsigned long)GetCurrentThreadId());
#else
    std::snprintf(tid, sizeof(tid), "thread id: %d", (int)syscall(SYS_gettid));
#endif
    mc::log::FileLogger::instance().writeRaw(tid);
    // Phase
    const char* phase = g_phase.load(std::memory_order_relaxed);
    mc::log::FileLogger::instance().writeRaw(std::string("phase: ") + (phase ? phase : "?"));
    // Stack trace
    writeStackTrace();
    mc::log::FileLogger::instance().writeRaw("========================================");
    mc::log::FileLogger::instance().flush();
}

// ── Windows exception handler ────────────────────────────────────────────────
#ifdef _WIN32
static LONG WINAPI winExceptionFilter(EXCEPTION_POINTERS* ep) {
    char buf[256];
    std::snprintf(buf, sizeof(buf),
        "Windows exception: code=0x%08X address=%p",
        (unsigned)ep->ExceptionRecord->ExceptionCode,
        ep->ExceptionRecord->ExceptionAddress);
    crashHandler(buf);
    // Let the OS terminate (produces WER dump).
    return EXCEPTION_EXECUTE_HANDLER;
}

static void __cdecl pureCallHandler() {
    crashHandler("pure virtual function call");
    std::abort();
}

static void __cdecl invalidParamHandler(const wchar_t*, const wchar_t*, const wchar_t*, unsigned int, uintptr_t) {
    crashHandler("invalid parameter (CRT)");
    std::abort();
}
#endif

// ── POSIX signal handler (Linux) ─────────────────────────────────────────────
#ifndef _WIN32
static void posixSignalHandler(int sig) {
    const char* name = "unknown signal";
    switch (sig) {
        case SIGSEGV: name = "SIGSEGV (segmentation fault)"; break;
        case SIGABRT: name = "SIGABRT (abort)"; break;
        case SIGFPE:  name = "SIGFPE (floating point exception)"; break;
        case SIGILL:  name = "SIGILL (illegal instruction)"; break;
        case SIGBUS:  name = "SIGBUS (bus error)"; break;
    }
    char buf[128];
    std::snprintf(buf, sizeof(buf), "signal %d: %s", sig, name);
    crashHandler(buf);
    // Restore default handler and re-raise so we get a core dump.
    std::signal(sig, SIG_DFL);
    std::raise(sig);
}
#endif

// ── std::terminate handler (C++ uncaught exception) ──────────────────────────
static void terminateHandler() {
    std::exception_ptr ep = std::current_exception();
    if (ep) {
        try {
            std::rethrow_exception(ep);
        } catch (const std::exception& e) {
            std::string msg = std::string("uncaught exception: ") + e.what();
            crashHandler(msg.c_str());
        } catch (...) {
            crashHandler("uncaught unknown exception");
        }
    } else {
        crashHandler("std::terminate (no exception)");
    }
    std::abort();
}

void initCrashHandlers() {
#ifdef _WIN32
    SetUnhandledExceptionFilter(winExceptionFilter);
    _set_purecall_handler(pureCallHandler);
    _set_invalid_parameter_handler(invalidParamHandler);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::signal(SIGABRT, [](int) { crashHandler("SIGABRT"); std::abort(); });
#else
    std::signal(SIGSEGV, posixSignalHandler);
    std::signal(SIGABRT, posixSignalHandler);
    std::signal(SIGFPE,  posixSignalHandler);
    std::signal(SIGILL,  posixSignalHandler);
    std::signal(SIGBUS,  posixSignalHandler);
#endif
    std::set_terminate(terminateHandler);
}

// ── Hang watchdog ────────────────────────────────────────────────────────────
// A background thread that checks every 500ms if the main loop has stalled.
// If no frameHeartbeat() in >2 seconds, it logs a HANG warning with the
// current phase. It does NOT kill the process — just logs so we can see
// what was happening when the hang occurred.
static void watchdogLoop() {
    using namespace std::chrono;
    while (g_watchdogRunning.load(std::memory_order_relaxed)) {
        std::this_thread::sleep_for(milliseconds(500));
        int64_t last = g_lastFrameMs.load(std::memory_order_relaxed);
        int64_t now = nowMs();
        int64_t delta = now - last;
        if (delta > 2000) {  // >2 seconds since last heartbeat = hang
            const char* phase = g_phase.load(std::memory_order_relaxed);
            char buf[256];
            std::snprintf(buf, sizeof(buf),
                "HANG DETECTED: no frame for %lld ms, phase='%s'",
                (long long)delta, phase ? phase : "?");
            mc::log::FileLogger::instance().writeRaw(buf);
            // Dump the frame profiler immediately so we can see which section
            // was running when the hang occurred.
            mc::debug::FrameProfiler::instance().dumpNow();
        }
    }
}

void startWatchdog() {
    if (g_watchdogRunning.load()) return;
    g_watchdogRunning.store(true);
    frameHeartbeat();  // initial heartbeat
    g_watchdogThread = std::thread(watchdogLoop);
}

void stopWatchdog() {
    g_watchdogRunning.store(false);
    if (g_watchdogThread.joinable()) g_watchdogThread.join();
}

} // namespace mc::debug
