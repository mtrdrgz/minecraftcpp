#!/usr/bin/env bash
# run_server_gen_structures.sh — Linux/bash counterpart of run_server_gen_structures.ps1.
#
# Generates ground-truth chunks WITH structures from the REAL Minecraft 26.1.2
# dedicated server, into a SEPARATE world dir (world_structures) so it never
# clobbers the terrain-only `world/` that full_chunk_parity uses. A wide region
# is force-loaded so the sparse structures (mineshafts, villages, etc.) actually
# land in the saved .mca; ServerChunkDump then decodes the per-block ground truth
# (via Mojang's own PalettedContainer codec) for the structure block-placement
# *_parity tests.
#
# This is the gold-standard oracle for the large hand-built structures
# (mineshaft / stronghold / ocean_monument / woodland_mansion / fortress) whose
# block placement reads back the generated terrain and therefore can only be
# verified against a fully-generated world — not in isolation.
#
# Requires the git-ignored real-Java runtime under 26.1.2/ (fetch first):
#   tools/provision_runtime.sh --parity     # JDK 25 + client.jar + libs
#   26.1.2/server.jar                        # the dedicated server (sha1-verified;
#                                            #   provision_parity_runtime.ps1 fetches it,
#                                            #   or copy it in alongside client.jar)
#
# Usage (from repo root):
#   tools/run_server_gen_structures.sh [fromChunkX fromChunkZ toChunkX toChunkZ] [seed]
# Defaults: -20 -20 20 20  (a 41x41-chunk square around origin), seed 1.
#
# After it finishes, dump + scan the world, e.g.:
#   CP="26.1.2/parity_classes:26.1.2/client.jar:26.1.2/libs/*"
#   REGION=26.1.2/server_run/world_structures/dimensions/minecraft/overworld/region
#   26.1.2/jdk25/bin/java -cp "$CP" ServerChunkDump --region "$REGION" --status   # chunk statuses
#   26.1.2/jdk25/bin/java -cp "$CP" ServerChunkDump --region "$REGION" 1 0,0 1,1   # block TSV
set -euo pipefail

FROM_CX="${1:--20}"; FROM_CZ="${2:--20}"; TO_CX="${3:-20}"; TO_CZ="${4:-20}"; SEED="${5:-1}"

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$REPO_ROOT"

JAVA="26.1.2/jdk25/bin/java"
SERVER_JAR="26.1.2/server.jar"
RUN="26.1.2/server_run"
[ -x "$JAVA" ]       || { echo "ERROR: $JAVA missing — run tools/provision_runtime.sh --parity" >&2; exit 1; }
[ -f "$SERVER_JAR" ] || { echo "ERROR: $SERVER_JAR missing — provision the dedicated server jar" >&2; exit 1; }

mkdir -p "$RUN"
# eula + a structures-enabled, headless-friendly server.properties. spawn-protection=0
# and a fixed seed keep the oracle reproducible; level-name isolates the world.
echo "eula=true" > "$RUN/eula.txt"
cat > "$RUN/server.properties" <<EOF
level-name=world_structures
level-seed=$SEED
level-type=minecraft:normal
generate-structures=true
online-mode=false
spawn-protection=0
max-players=1
view-distance=10
EOF
rm -rf "$RUN/world_structures"

CMD="$RUN/cmd.txt"
LOG="$RUN/server.log"
: > "$CMD"

# tail -f keeps the server's stdin open and streams console commands we append
# (the robust headless pattern — no TTY, no fifo EOF races).
( cd "$RUN" && exec tail -n +1 -f cmd.txt | "$REPO_ROOT/$JAVA" -Xmx3G -jar "$REPO_ROOT/$SERVER_JAR" --nogui ) > "$LOG" 2>&1 &
LAUNCHER=$!
trap 'kill "$LAUNCHER" 2>/dev/null || true' EXIT

echo "[gen] server starting (pid $LAUNCHER); waiting for world load..."
for _ in $(seq 1 600); do grep -q 'Done (' "$LOG" 2>/dev/null && break; sleep 0.5; done
grep -q 'Done (' "$LOG" || { echo "ERROR: server never reached 'Done'"; tail -20 "$LOG"; exit 1; }
echo "[gen] server ready."

send() { echo "$1" >> "$CMD"; echo "  > $1"; }

# Freeze gameplay ticks (mobs/fluids) but keep chunk generation working.
send 'tick freeze'

# Force-load the requested rectangle in <=16x16-chunk (256-chunk) tiles: vanilla's
# forceload caps at 256 chunks per command, so a wide region must be tiled.
gen_total=0
cx="$FROM_CX"
while [ "$cx" -le "$TO_CX" ]; do
    cx_end=$(( cx + 15 )); [ "$cx_end" -gt "$TO_CX" ] && cx_end="$TO_CX"
    cz="$FROM_CZ"
    while [ "$cz" -le "$TO_CZ" ]; do
        cz_end=$(( cz + 15 )); [ "$cz_end" -gt "$TO_CZ" ] && cz_end="$TO_CZ"
        send "forceload add $(( cx*16 )) $(( cz*16 )) $(( cx_end*16 + 15 )) $(( cz_end*16 + 15 ))"
        gen_total=$(( gen_total + (cx_end-cx+1)*(cz_end-cz+1) ))
        # give the chunk system time to generate this tile before the next ticket
        sleep 6
        cz=$(( cz_end + 1 ))
    done
    cx=$(( cx_end + 1 ))
done
echo "[gen] requested $gen_total chunks; letting generation settle..."
sleep 20

send 'save-all flush'
for _ in $(seq 1 120); do grep -q 'All dimensions are saved' "$LOG" 2>/dev/null && break; sleep 0.5; done
send 'stop'
for _ in $(seq 1 180); do kill -0 "$LAUNCHER" 2>/dev/null || break; sleep 0.5; done
kill "$LAUNCHER" 2>/dev/null || true
trap - EXIT

REGION="$RUN/world_structures/dimensions/minecraft/overworld/region"
echo "==== world_structures overworld region files ===="
ls -la "$REGION" 2>/dev/null || echo "  (none — generation may have failed; see $LOG)"
