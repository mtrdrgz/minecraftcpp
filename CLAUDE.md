# CLAUDE.md — read this first

This repo is a **1:1 reverse-engineering port of Minecraft Java Edition 26.1.2 to C++**.
The full agent guide is [AGENTS.md](AGENTS.md) — read it. This file exists so the rule
that matters most is never missed.

## ⛔ THE ONE RULE: port from the source, never make it up

You are translating Java → C++. Nothing else. **Every value, constant, formula,
ordering and algorithm MUST come from the decompiled Java (`26.1.2/src/`) or the
worldgen data JSON (`26.1.2/data/`). Never invent, guess, approximate, simplify, tune,
or "make it look reasonable."**

- If you can't find the source for something, STOP and read the Java. Do not fill the
  gap with a plausible number — a wrong-but-plausible value hides the bug and looks done.
- A not-yet-ported feature/predicate/type must be a **hard no-op that does nothing**
  and is listed as unported — NEVER a silent `return true;` / "pass everything", which
  disables the exact gating that makes vanilla correct.
- No placeholder hashes/curves/noise (e.g. an FNV standing in for MD5, a lerp for a
  spline). They corrupt everything downstream and are nearly invisible until you see
  the world.
- Prefer a `*_parity` test against ground truth from the real jar/data over eyeballing.

**Do not tune frequencies/densities to "look right."** If pumpkins or grass or biomes
look wrong, the cause is a skipped/incorrect port (a missing predicate, a placeholder
hash, an unhandled feature type) — find and port the real algorithm. See the Session 40
notes in AGENTS.md for concrete examples (real MD5, block predicates, plant rendering).

## Build / run (Windows, llvm-mingw + Ninja)

See AGENTS.md and the per-agent `memory/` notes for the exact toolchain paths. Build via
`mcpp/tools/run_with_timeout.ps1` with an explicit `-TimeoutSec`. Run the engine from the
**repo root** (so `26.1.2/data` resolves): `mcpp\build\mcpp.exe --quickPlaySingleplayer`.
