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

Singleplayer smoke:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\run_with_timeout.ps1 -FilePath build\mcpp.exe -Arguments '--quickPlaySingleplayer' -TimeoutSec 35 -LogPath build\logs\smoke.log
```

## GitHub Notes

Do not commit or publish Mojang-owned binaries, decompiled sources, raw assets, or generated asset packs. Keep those local and regenerate derived files on machines that own the required game files.
