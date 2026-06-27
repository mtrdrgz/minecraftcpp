#pragma once
#include "Log.h"
#include <string>

namespace mc::debug {

// Initialize crash handlers. Call once at startup, BEFORE any game code.
// On Windows: installs SetUnhandledExceptionFilter + _set_purecall_handler +
//   _set_invalid_parameter_handler + _set_abort_behavior + signal(SIGABRT).
// On Linux: installs signal handlers for SIGSEGV, SIGABRT, SIGFPE, SIGILL,
//   SIGBUS + std::set_terminate.
// All handlers write a crash dump to the FileLogger (mcpp.log) with:
//   - signal/exception code
//   - stack trace (best-effort)
//   - thread id
//   - timestamp
// Then flush the log and re-raise the signal so the OS can produce a core dump.
void initCrashHandlers();

// Mark the start of a frame for the hang watchdog. Called by the main loop
// at the beginning of each frame. The watchdog thread checks if
// frameHeartbeat() hasn't been called in >2 seconds; if so, it logs a
// "HANG DETECTED" message with the last known frame state.
void frameHeartbeat();

// Set a human-readable description of what the main thread is currently doing.
// Called at the start of each game phase (render, mesh, decorate, tick, etc.)
// so the watchdog can report what was happening when the hang occurred.
void setPhase(const char* phase);

// Start the hang watchdog thread. Called once at startup after initCrashHandlers().
void startWatchdog();

// Stop the watchdog (on clean shutdown).
void stopWatchdog();

} // namespace mc::debug
