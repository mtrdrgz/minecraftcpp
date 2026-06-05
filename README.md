# Minecraft CPP

Native C++ porting workspace for Minecraft Java Edition 26.1.2 behavior.

This repository is for the port code, build tooling, and parity helpers. It does not include Mojang's original `client.jar`, decompiled Java source, raw asset objects, or generated embedded asset packs.

## Layout

- `mcpp/` - C++ project.
- `mcpp/src/` - runtime/client/world/render/audio code.
- `mcpp/tools/run_with_timeout.ps1` - required command harness for local build/run/test commands.
- `mcpp/tools/asset_packer/` - local asset packer for generating embedded runtime assets.
- `AGENTS.md` - current project state and AI-agent working notes.

Local-only inputs expected beside this repo:

- `26.1.2/client.jar`
- `26.1.2/src/` decompiled Java source
- `assets/indexes/`
- `assets/objects/`

Those folders are intentionally ignored by git.

## Build

From `mcpp/`, use the harness with explicit timeouts:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_timeout.ps1 -FilePath cmake.exe -Arguments '--build build --config Release' -TimeoutSec 120 -LogPath build\logs\build.log
```

The current local workspace already has generated embedded assets. A clean clone needs the local Minecraft inputs above and then asset generation before the full client can run.

## Tests / Smoke

BiomeManager parity test:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_timeout.ps1 -FilePath cmake.exe -Arguments '--build build --config Release --target biome_manager_parity' -TimeoutSec 120 -LogPath build\logs\build-biome-manager-parity.log
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_timeout.ps1 -FilePath build\biome_manager_parity.exe -TimeoutSec 10 -LogPath build\logs\run-biome-manager-parity.log
```

Overworld biome climate parity test (`OverworldBiomeBuilder` + `Climate::RTree`).
Pure standard C++, so it also builds/runs without the Windows toolchain:

```bash
g++ -std=c++23 -O2 -I src/world/level/levelgen \
  src/world/level/levelgen/OverworldBiomeParityTest.cpp \
  src/world/level/levelgen/OverworldBiomeBuilder.cpp -o overworld_biome_parity
./overworld_biome_parity                 # self-contained invariant checks
```

For the strongest, jar-backed check, regenerate ground truth from the real
decompiled code and replay it (needs the local `26.1.2/client.jar` + libraries):

```bash
javac -cp "26.1.2/client.jar:26.1.2/libs/*" -d 26.1.2/parity/out tools/OverworldBiomeParity.java
java  -cp "26.1.2/parity/out:26.1.2/client.jar:26.1.2/libs/*" OverworldBiomeParity 26.1.2/parity
./overworld_biome_parity --cases 26.1.2/parity/climate_cases.tsv   # expects 0 mismatches
```

Biome registry loader test (loads all 65 biomes from the worldgen data; needs
the extracted `26.1.2/data/...`). Pure standard C++ + nlohmann/json:

```bash
g++ -std=c++23 -O2 -I vendor -I src/world/level/biome \
  src/world/level/biome/BiomeRegistryParityTest.cpp \
  src/world/level/biome/BiomeRegistry.cpp -o biome_registry_parity
./biome_registry_parity 26.1.2/data/minecraft/worldgen/biome        # spot-checks
# full field-for-field cross-check against an independent parser:
diff <(./biome_registry_parity --dump 26.1.2/data/minecraft/worldgen/biome) \
     <(python3 tools/biome_registry_reference.py 26.1.2/data/minecraft/worldgen/biome)
```

WorldgenRandom test (the decoration/feature population RNG). Pure standard C++:

```bash
g++ -std=c++23 -O2 -I src/world/level/levelgen \
  src/world/level/levelgen/WorldgenRandomParityTest.cpp \
  src/world/level/levelgen/RandomSource.cpp -o worldgen_random_parity
./worldgen_random_parity                                    # self-checks
# full check vs the real decompiled WorldgenRandom (needs the local jar):
javac -cp 26.1.2/client.jar -d 26.1.2/parity/out tools/WorldgenRandomParity.java
java  -cp "26.1.2/parity/out:26.1.2/client.jar:26.1.2/libs/*" WorldgenRandomParity \
      > 26.1.2/parity/worldgen_random_cases.tsv
./worldgen_random_parity --cases 26.1.2/parity/worldgen_random_cases.tsv
```

Placement test (IntProvider value samplers + pure placement modifiers).
Needs `glm` headers (BlockPos) — link the project's `glm` or pass the defines:

```bash
g++ -std=c++23 -O2 -DGLM_ENABLE_EXPERIMENTAL -I src -I vendor \
  src/world/level/levelgen/placement/PlacementParityTest.cpp \
  src/world/level/levelgen/RandomSource.cpp -o placement_parity
./placement_parity                                          # self-checks
# full check vs the real decompiled classes (needs the local jar + libs):
javac -cp "26.1.2/client.jar:26.1.2/libs/*" -d 26.1.2/parity/out tools/PlacementParity.java
java  -cp "26.1.2/parity/out:26.1.2/client.jar:26.1.2/libs/*" PlacementParity 26.1.2/parity/placement_cases.tsv
./placement_parity --cases 26.1.2/parity/placement_cases.tsv
```

Height/Float provider test (VerticalAnchor + HeightProvider + FloatProvider).
Pure standard C++:

```bash
g++ -std=c++23 -O2 -I src/world/level/levelgen \
  src/world/level/levelgen/heightproviders/HeightFloatProviderParityTest.cpp \
  src/world/level/levelgen/RandomSource.cpp -o height_float_provider_parity
./height_float_provider_parity                              # self-checks
# full check vs the real decompiled classes (needs the local jar + libs):
javac -cp "26.1.2/client.jar:26.1.2/libs/*" -d 26.1.2/parity/out tools/HeightFloatProviderParity.java
java  -cp "26.1.2/parity/out:26.1.2/client.jar:26.1.2/libs/*" HeightFloatProviderParity 26.1.2/parity/height_float_cases.tsv
./height_float_provider_parity --cases 26.1.2/parity/height_float_cases.tsv
```

World placement test (HeightmapPlacement + HeightRangePlacement vs a stub
WorldGenLevel). Needs `glm` headers (BlockPos):

```bash
g++ -std=c++23 -O2 -DGLM_ENABLE_EXPERIMENTAL -I src -I vendor \
  src/world/level/levelgen/placement/WorldPlacementParityTest.cpp \
  src/world/level/levelgen/RandomSource.cpp -o world_placement_parity
./world_placement_parity                                    # self-checks
# full check vs the real decompiled modifiers (needs the local jar + libs):
javac -cp "26.1.2/client.jar:26.1.2/libs/*" -d 26.1.2/parity/out tools/WorldPlacementParity.java
java  -cp "26.1.2/parity/out:26.1.2/client.jar:26.1.2/libs/*" WorldPlacementParity 26.1.2/parity/world_placement_cases.tsv
./world_placement_parity --cases 26.1.2/parity/world_placement_cases.tsv
```

See `docs/WORLDGEN_PLAN.md` for the full 1:1 worldgen port roadmap.

Singleplayer smoke:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_timeout.ps1 -FilePath build\mcpp.exe -Arguments '--quickPlaySingleplayer' -TimeoutSec 35 -LogPath build\logs\smoke.log
```

## GitHub Notes

~~-Do not commit or publish Mojang-owned binaries, decompiled sources, raw assets, or generated asset packs. Keep those local and regenerate derived files on machines that own the required game files.~~
We got official permission from mojang to use source 🎉
