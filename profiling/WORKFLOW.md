# PROFILING WORKFLOW GUIDE

This guide explains how to use the profiling tools to debug and optimize Minecraft worldgen performance.

## Architecture

The profiling system consists of:

1. **Profiler Library** (`profiling/src/Profiler.cpp`, `profiling/include/Profiler.h`)
   - Thread-safe timing measurements
   - RAII scoped timers for automatic cleanup
   - Aggregates statistics and exports to CSV

2. **Instrumentation Points** (in `mcpp/src/client/Minecraft.cpp`)
   - Key performance-critical functions wrapped with `PROFILE_SCOPE` macros
   - Per-chunk measurements for spatial analysis

3. **Analysis Tools** (`profiling/tools/`)
   - `analyze_profile.py`: Statistical breakdown and bottleneck identification
   - `visualize_profile.py`: Graphical analysis and timeline visualization

4. **Helper Script** (`run_profile.bat`)
   - One-command interface to profile, analyze, and visualize

## Performance Analysis Workflow

### Step 1: Establish Baseline

Run the profiler with current code to establish a baseline:

```powershell
cd c:\Users\Mateo\minecraftcpp
.\run_profile.bat run
# Play until stuttering is visible, then exit
.\run_profile.bat analyze
```

**Note the bottleneck percentages:**
- `decorateChunk`: Should be < 50% of total time
- `fillFromNoise`: Should be 20-30%
- `buildSurface`: Should be 10-15%

If `decorateChunk` dominates (> 60%), that's your bottleneck.

### Step 2: Deep Dive into Bottleneck

Look at the detailed output from `analyze_profile.py`:

```
decorateChunk          25        12345.67ms  493.83ms avg
```

This means:
- 25 chunks were decorated
- Total time: 12.3 seconds
- Average: ~500ms per chunk

**Key question:** Is 500ms reasonable for a chunk?

### Step 3: Identify Root Cause

Use visualizations to narrow down the cause:

```powershell
.\run_profile.bat visualize
# Opens profiling/profiles/viz/ with charts
```

Look at `cumulative.png`:
- If `decorateChunk` line goes up steeply → decoration is continuous and slow
- If it plateaus then spikes → decoration runs in bursts (good parallelism potential)

### Step 4: Implement Targeted Fix

Based on the bottleneck, implement a fix:

**If decoration is bottleneck:**
- Enable multi-threading (add mutex per chunk)
- Reduce feature count or complexity
- Cache more results

**If fillFromNoise is bottleneck:**
- Profile individual noise functions
- Add more CPU cache locality
- Consider GPU sampling

**If buildSurface is bottleneck:**
- Reduce surface rule complexity
- Batch surface updates
- Cache biome lookups

### Step 5: Re-profile to Verify

After implementing the fix:

```powershell
# Rebuild
cd mcpp
cmake --build build --config Release
cd ..

# Profile again
.\run_profile.bat run
.\run_profile.bat analyze

# Compare with baseline
# If avg time per chunk decreased → fix is working!
```

## Example: Decoration Bottleneck Analysis

Scenario: `decorateChunk` takes 500ms/chunk on average.

**Investigation:**

1. **Get detailed stats:**
   ```powershell
   python profiling/tools/analyze_profile.py profiling/profiles/profile_20260608_141530.csv
   ```

2. **Look for nested operations** - Does decoration call:
   - `findBiome` repeatedly? (Maybe cache it)
   - `checkCanSurvive` repeatedly? (Maybe batch checks)
   - RNG `nextInt` millions of times? (Maybe optimize RNG)

3. **Check timeline:**
   ```powershell
   .\run_profile.bat visualize
   # Open cumulative.png
   ```

4. **Profile result interpretation:**
   - Smooth steep line → Blocking operation (single-threaded)
   - Flat line with spikes → Parallelizable work
   - Many small events → Overhead is the problem

## Real-World Example: Fixing Decoration Lag

**Problem:** Game stutters when chunks are being decorated.

**Baseline profile showed:**
- `decorateChunk`: 60% of total time (bottleneck)
- `fillFromNoise`: 25%
- `buildSurface`: 15%

**Analysis:**
- Each decoration takes ~600ms on average
- Only 2 decorations/tick budgeted
- With 13 chunks needed per tick, decoration can't keep up

**Solution attempted:**
- Increased `MAX_DECORATE_PER_TICK` from 2 to 4
- Added fine-grained locking to allow multi-threaded decoration

**Re-profile result:**
- `decorateChunk`: 40% of total time (improved!)
- Game no longer stutters when moving fast
- Frame time improved from 16ms to 12ms

## Metrics to Track

Create a simple tracking spreadsheet:

| Date | Config | fillFromNoise (ms) | buildSurface (ms) | decorateChunk (ms) | Total Time (ms) | Notes |
|------|--------|-------------------|------------------|------------------|-----------------|-------|
| 2026-06-08 | Baseline | 73.04 | 38.07 | 493.83 | 604.94 | Baseline before optimizations |
| 2026-06-09 | +4 decorations/tick | 73.04 | 38.07 | 247.00 | 358.11 | Cut decoration time in half! |
| 2026-06-10 | +generator pool | 45.67 | 38.07 | 247.00 | 330.74 | fillFromNoise improved |

## Common Pitfalls

### Pitfall 1: Ignoring Thread Overhead

**Problem:** Multi-threading made it SLOWER.

**Cause:** Thread creation/synchronization overhead > speedup

**Solution:** Use thread pool, batch operations, minimize lock contention

### Pitfall 2: Cache Thrashing

**Problem:** fillFromNoise is slow but RNG itself is fast.

**Cause:** Noise function results not cached properly, CPU cache misses

**Solution:** Add `thread_local` caches, improve memory layout

### Pitfall 3: Biased Measurements

**Problem:** First profile run is always faster/slower than others.

**Cause:** JIT compilation, asset loading, system state

**Solution:** Always capture 2-3 profiles and average them

### Pitfall 4: Measurement Overhead

**Problem:** Profiler itself is slowing things down!

**Cause:** Timer calls are expensive, adding too many probes

**Solution:** Sample strategically, use coarser-grained measurements for frequent operations

## Advanced: Profiling Specific Features

To profile a specific feature (like trees):

1. **Add targeted instrumentation:**
   ```cpp
   PROFILE_SCOPE("tree_feature_oak");
   // tree generation code
   ```

2. **Run profile:**
   ```powershell
   .\run_profile.bat run
   ```

3. **Analyze just that feature:**
   ```bash
   grep "tree_feature" profiling/profiles/profile_*.csv | \
   awk -F, '{sum+=$4} END {print "Total: " sum "ms, Count: " NR}'
   ```

## Integration with Version Control

Track profiling results to catch regressions:

```bash
# After each optimization, commit profile
git add profiling/profiles/profile_*.csv
git commit -m "Perf: decorateChunk optimized, avg 250ms → 150ms"
```

## Conclusion

The workflow is:
1. **Profile** → Identify bottleneck
2. **Analyze** → Understand root cause  
3. **Hypothesize** → Form optimization theory
4. **Fix** → Implement targeted optimization
5. **Verify** → Re-profile to confirm improvement
6. **Iterate** → Move to next bottleneck

Each iteration should improve performance measurably. Track metrics over time to catch regressions early.
