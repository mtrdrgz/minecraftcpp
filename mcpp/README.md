# mcpp

C++23 Windows client/runtime project for the Minecraft CPP port.

Always run long-lived commands through `tools/run_with_timeout.ps1` with an explicit timeout. The build currently expects generated local assets in `src/assets/` for a runnable client; these files are intentionally ignored at repository root because they are derived from Mojang assets.
