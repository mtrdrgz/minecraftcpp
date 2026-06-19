# Master port coverage — the full 1:1 Minecraft 26.1.2 → C++ port

Goal (set 2026-06-09): a **fully verified 1:1 port of everything** — worldgen incl.
structures, rendering, GUI (all menus/submenus), gamemodes, movement/player physics,
multiplayer/netcode, audio, every feature — shipping as a single dependency-free
`mcpp.exe` with all assets embedded; then optimization (≥1000 fps capability, stable
frame graph, worldgen that never throttles the game thread); then cleanup.

**RULE #0 applies to everything**: behaviour comes from `26.1.2/src` (+ data/assets),
is reimplemented 1:1, and is **proven** — never assumed. Existing C++ code counts as
UNVERIFIED until a gate proves it. For subsystems built on libraries the original gets
from Java (GL via LWJGL, audio via OpenAL, ...), the port must be pixel-/byte-/
behaviour-equivalent on top of our own stack.

## The ledger
`mcpp/docs/PORT_COVERAGE.tsv` — one row per Java file (6,882 total):
`path<TAB>status<TAB>evidence`, statuses `unvisited|ported|partial|no-op|n/a`.
Maintained with `mcpp/tools/port_coverage.sh` (init/summary/mark/show). A file may
only be marked `ported` with named proof (parity gate, pixel test, protocol capture
replay, commit). The goal is **zero `unvisited` rows** and every `no-op` justified.

## Verification toolkit (extend per subsystem)
- **Worldgen**: the proven chain — tick-frozen server.jar runs (`run_server_gen.ps1`)
  → `.mca` dumps (`ServerChunkDump`) → Java GT harness (`FullChunkDecorateParity`,
  byte-matches the server) → C++ gates (`full_chunk_parity`,
  `full_chunk_decorate_parity --family all`). 15 chunks / 1.47M cells at 0 so far.
- **Deterministic logic** (RNG, math, datafix-free NBT, recipes, loot): Java
  ground-truth dump tools (the `*Parity.java` pattern) + C++ `*_parity` targets.
- **Netcode**: packet-level capture from the real server/client (offline mode) +
  byte-replay against the C++ implementation; protocol round-trip tests.
- **Rendering**: golden-image tests vs the real client (same seed/pos/settings,
  pixel-perfect for UI; tolerance-bounded for 3D where driver variance exists),
  plus mesh/light-value dumps compared numerically.
- **Movement/physics**: input-script replay — feed identical input sequences to the
  real client (via a headless bot or recorded session) and to mcpp; compare position/
  velocity traces per tick to full float precision.
- **Audio**: event-level (which sound, when, volume/pitch) vs the real client logs.

## Subsystem roadmap (each lands with its gate; order = dependency-driven)
1. **Worldgen decoration breadth** (IN PROGRESS): all biome classes × features; then
   multiple seeds. Gate: per-class server-anchored chunk sets at 0.
2. **Structures**: structure_starts/references/placement (Jigsaw, templates) — the
   explicitly-deferred half of worldgen. Gate: server runs WITH structures, full-chunk
   byte-match (extends the existing chain).
3. **Engine integration of certified worldgen**: the engine's real chunk pipeline
   (BiomeDecorator::applyBiomeDecoration is currently a no-op outside the parity
   harness) + threading model. Gate: in-engine chunk dumps == parity-harness output.
4. **Block behaviour & ticking**: block updates, fluids (flow!), random ticks,
   block entities. Gate: scenario worlds diffed against real-server ticking.
5. **Entities & AI**: movement, pathfinding, goals/behaviors, spawning. Gate:
   deterministic-seed spawn/AI traces vs server.
6. **Player & movement**: physics, collision, gamemodes, interactions, inventory.
   Gate: input-replay position traces.
7. **Items/recipes/loot**: crafting, loot tables, enchantments. Gate: table-driven
   parity dumps.
8. **Netcode/multiplayer**: full protocol (handshake/login/play), client vs real
   server and server vs real client. Gate: packet capture replay + live interop.
9. **Rendering**: chunk meshing, lighting (already partially present: dx12/vulkan/
   opengl backends exist — UNVERIFIED), entities, particles, item/GUI rendering.
   Gate: golden images + light-grid numeric parity.
10. **GUI**: every screen/submenu (client/gui: 425 files). Gate: pixel-perfect
    golden images per screen state.
11. **Audio**: sound engine + event parity.
12. **Misc client**: resource packs, options, keybinds, fonts, locales.
13. **Packaging**: single static `mcpp.exe`, assets embedded (assets.bin already
    exists — verify completeness), no external DLLs (toolchain runtime static).
14. **Optimization phase** (only after functional completeness): ≥1000 fps capable,
    stable frame graph, worldgen threading that never starves the game thread.
    Profile-driven; gates stay green throughout.
15. **Cleanup**: remove debug tools/files not needed for the gates, dead code,
    naming/layout pass. Parity suite must still pass at the end.

## Scope judgments (recorded as `n/a` rows when marked)
- `com/mojang/realmsclient/**`, telemetry, Mojang auth/online services: the port
  targets offline play; mark n/a with reason (revisit if multiplayer auth needed).
- `util/datafix/**` (392 files): old-save migration. Out of scope while the port
  generates its own worlds; revisit if loading vanilla worlds becomes a goal
  (the .mca READER used by parity tooling is separate and already exists).
- `gametest/framework`: Mojang's internal test harness — n/a unless repurposed.

## Progress log
- 2026-06-09/10: worldgen terrain+carvers byte-exact (2.36M cells); decoration
  certified end-to-end on 15 chunks (oceans/shore/forest, 1.47M cells, all 0);
  tree family ported; PalettedContainer engine bug found+fixed by parity work;
  ledger created, 51 files evidence-marked ported.
- 2026-06-10: bounded-class byte-exact parity waves 1-4 (~150 leaf classes,
  util/core/math/worldgen/phys/entity/item/biome/AI/render/netcode/block-state).
  Engine bugs found+fixed by the gates: Mth.square/invSqrt/atan2, AABB.getCenter,
  Joml jfma, PalettedContainer wire layout, Vec3i saturating cast,
  SegmentedAnglePrecision saturating round, DensityFunctions YClampedGradient
  endpoint (1-ULP). Documented platform ceiling: java.lang.Math.{sin,log} HotSpot
  intrinsics != std::*/StrictMath (handled with tight ULP allowlists/tolerances).
  Wave 5 (+36 gates): pos/coord math (AxisCycle/SectionPos/GlobalPos), value math,
  and NBT/packet-relevant enums (Difficulty/GameType/ChatFormatting/Heightmap.Types/
  block-state enums/Rotation/Mirror/...). Ledger: 263 ported / 6124 unvisited.
