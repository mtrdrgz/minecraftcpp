# PROFILING SETUP COMPLETE ✅

Comprehensive performance profiling infrastructure has been deployed.

## 🚀 Quick Start (5 minutes)

### Step 1: Build the Project

```powershell
cd c:\Users\Mateo\minecraftcpp
cd mcpp
cmake --build build --config Release
cd ..
```

### Step 2: Run with Profiling

```powershell
.\run_profile.bat run
```

This launches the game with profiling enabled. Play for ~30 seconds to let chunks generate and decoration happen, then **exit the game**.

### Step 3: Analyze Results

```powershell
.\run_profile.bat analyze
```

You'll see a table like this:

```
===== PROFILING STATISTICS =====
Event                     Count      Total(ms)  Avg(ms)        Min(ms)        Max(ms)
decorateChunk              25       12345.67    493.83         234.56        1234.56  [42.3%]
fillFromNoise             120        8765.43     73.04          45.67         156.78  [30.1%]
buildSurface              120        4567.89     38.07          28.34          89.12  [15.7%]
applyCarvers              120        2345.67     19.55          12.34          56.78  [8.1%]
```

**The event with the HIGHEST percentage is your bottleneck.**

### Step 4: Generate Visualizations (Optional)

```powershell
.\run_profile.bat visualize
```

This creates graphs in `profiling/profiles/viz/`:
- `event_distribution.png` - Pie chart of time by event
- `event_timings.png` - Bar chart comparing events
- `timeline.png` - Timeline of when events happened
- `cumulative.png` - Cumulative time (shows dominance)

## 📊 What Gets Measured

### Chunk Generation (Async - Worker Threads)
- `fillFromNoise` (45-150ms/chunk) — Terrain noise sampling
- `buildSurface` (28-89ms/chunk) — Surface rule application
- `applyCarvers` (12-56ms/chunk) — Cave/canyon carving

### Chunk Decoration (Main Thread Only - BOTTLENECK!)
- `decorateChunk` (234-1234ms/chunk) — Feature placement (trees, flowers, ores)
- `runStructures` — Structure piece assembly
- `remesh_9chunk_neighborhood` — Mesh updates after cross-chunk writes

### Startup (Blocking)
- `startup_terrain_generation` — Initial 5×5 chunk terrain gen
- `startup_decoration_and_structures` — Initial decoration (very slow!)

## 🎯 Expected Results

### Current Bottleneck (Likely)
**`decorateChunk` dominates**: 40-60% of total time
- Average: 250-500ms per chunk
- Problem: Trees + features + cross-chunk writes are expensive
- Single-threaded (main thread only)

### What You'll See

```
decorateChunk: 42.3%  ← BOTTLENECK - decoration is the problem
fillFromNoise: 30.1%  ← Secondary
buildSurface: 15.7%
applyCarvers: 8.1%
remesh: 3.8%
```

## 🔍 Interpreting Results

### If decorateChunk > 50%
**Root cause:** Feature placement (trees, ores) is too slow
**Solutions:**
- Parallelize decoration with fine-grained locking
- Increase MAX_DECORATE_PER_TICK from 2 to 4+
- Cache feature lookups/RNG results
- Reduce feature count or complexity

### If fillFromNoise > 40%
**Root cause:** Terrain noise sampling is slow
**Solutions:**
- Profile individual noise functions
- Add more CPU cache locality
- Consider GPU noise sampling
- Cache noise results

### If buildSurface > 30%
**Root cause:** Surface rules are complex/slow
**Solutions:**
- Optimize rule evaluation order
- Cache biome lookups
- Reduce rule complexity

## 📁 File Structure

```
profiling/                          ← Git-ignored folder
├── include/
│   └── Profiler.h                 ← Timing library
├── src/
│   └── Profiler.cpp               ← Implementation
├── tools/
│   ├── analyze_profile.py         ← Statistical analysis
│   └── visualize_profile.py       ← Graphical viz
├── profiles/                       ← Generated CSV data
│   ├── profile_20260608_141530.csv
│   └── viz/                        ← Generated charts
├── README.md                       ← Usage guide
└── WORKFLOW.md                     ← Detailed optimization workflow

run_profile.bat                      ← Helper script (repo root)
```

## 📝 Example Workflow

```bash
# 1. Generate baseline
.\run_profile.bat run     # Play ~30s, exit
.\run_profile.bat analyze # Note the bottleneck %

# 2. Identify root cause by looking at visualization
.\run_profile.bat visualize
# Open cumulative.png - if decorateChunk line is steep, that's the bottleneck

# 3. Implement fix (e.g., increase MAX_DECORATE_PER_TICK)
# Edit: mcpp/src/client/Minecraft.cpp
# Change: constexpr int MAX_DECORATE_PER_TICK = 2;  →  = 4;

# 4. Rebuild and verify improvement
cd mcpp
cmake --build build --config Release
cd ..
.\run_profile.bat run     # Play again
.\run_profile.bat analyze # Compare metrics - should improve!
```

## 💡 Pro Tips

1. **Always profile in Release mode** — Debug mode is slow and misleading
2. **Run multiple profiles** — Single runs can have variance, average 2-3 runs
3. **Profile different scenarios** — Startup vs. runtime, small tree vs. forest
4. **Check thread utilization** — Last part of analyze output shows thread activity
5. **Commit profile baselines** — Track performance over time in git

## 🛠️ Help & Debugging

### "No profile file generated"
- Ensure game exited normally (didn't crash)
- Check that `profiling/` directory exists
- Look for error messages in console

### "Profile shows all zeros"
- Events might be running faster than timer
- Increase scope to include multiple iterations
- Or timing resolution is too low

### "profiling/tools/analyze_profile.py not found"
- Make sure Python 3 is installed
- Run from repo root: `python profiling/tools/analyze_profile.py ...`

## 📌 Next Steps

1. **Run profiler** with `.\run_profile.bat run`
2. **Analyze** with `.\run_profile.bat analyze`
3. **Identify bottleneck** (likely `decorateChunk`)
4. **Implement targeted fix** based on findings
5. **Re-profile** to verify improvement
6. **Commit baseline** when satisfied

The profiling data will help us systematically optimize, not guess.

---

**Questions?** Check `profiling/README.md` or `profiling/WORKFLOW.md` for detailed guides.
