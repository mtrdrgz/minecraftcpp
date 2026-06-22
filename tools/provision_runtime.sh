#!/usr/bin/env bash
# provision_runtime.sh — Linux/bash counterpart of provision_parity_runtime.ps1 +
# PrepareRuntimeAssets.ps1, for headless (Claude Code web / CI) environments that
# have curl/jq/unzip but no PowerShell.
#
# Fetches the sha1-verified 26.1.2 client.jar from Mojang and extracts the two
# git-ignored trees the C++ runtime + parity harnesses expect:
#   26.1.2/data/minecraft/...            (worldgen JSON, tags, structures)
#   assets/client-extract/assets/...     (blockstates, block models, textures, colormaps)
#
# Nothing proprietary is committed — both targets are git-ignored. Idempotent:
# re-extraction overwrites in place. Run from the repo root:
#   tools/provision_runtime.sh
set -euo pipefail

VERSION="${MCPP_VERSION:-26.1.2}"
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

for tool in curl jq unzip sha1sum; do
    command -v "$tool" >/dev/null 2>&1 || { echo "ERROR: missing required tool: $tool" >&2; exit 1; }
done

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "[1/4] version manifest -> $VERSION metadata"
curl -sS -L --retry 3 --max-time 60 -o "$TMP/manifest.json" \
    https://piston-meta.mojang.com/mc/game/version_manifest_v2.json
META_URL="$(jq -r --arg v "$VERSION" '.versions[] | select(.id==$v) | .url' "$TMP/manifest.json")"
[ -n "$META_URL" ] && [ "$META_URL" != "null" ] || { echo "ERROR: $VERSION not in manifest" >&2; exit 1; }
curl -sS -L --retry 3 --max-time 60 -o "$TMP/version.json" "$META_URL"

CLIENT_URL="$(jq -r '.downloads.client.url'  "$TMP/version.json")"
CLIENT_SHA="$(jq -r '.downloads.client.sha1' "$TMP/version.json")"

echo "[2/4] download client.jar ($(jq -r '.downloads.client.size' "$TMP/version.json") bytes)"
curl -sS -L --retry 3 --max-time 600 -o "$TMP/client.jar" "$CLIENT_URL"
GOT_SHA="$(sha1sum "$TMP/client.jar" | cut -d' ' -f1)"
[ "$GOT_SHA" = "$CLIENT_SHA" ] || { echo "ERROR: sha1 mismatch ($GOT_SHA != $CLIENT_SHA)" >&2; exit 1; }
echo "      sha1 ok: $GOT_SHA"

echo "[3/4] extract data/ -> 26.1.2/data"
mkdir -p "$VERSION"
unzip -qo "$TMP/client.jar" 'data/*' -d "$VERSION/"

echo "[4/4] extract assets/ -> assets/client-extract/assets"
mkdir -p assets/client-extract
unzip -qo "$TMP/client.jar" 'assets/*' -d assets/client-extract/

echo "done:"
echo "  worldgen biomes : $(ls "$VERSION/data/minecraft/worldgen/biome" 2>/dev/null | wc -l)"
echo "  blockstates     : $(ls assets/client-extract/assets/minecraft/blockstates 2>/dev/null | wc -l)"
echo "  block models    : $(ls assets/client-extract/assets/minecraft/models/block 2>/dev/null | wc -l)"
