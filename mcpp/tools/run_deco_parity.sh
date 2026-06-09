#!/usr/bin/env bash
# Compile FullChunkDecorateParity, run it for all server ground-truth chunks (seed 1),
# compare each against the server .mca dump, and report per-chunk + total mismatches
# and the aggregated block-transition breakdown.
#   run_deco_parity.sh [order]      order in {xz, zx}; default xz
set -uo pipefail
cd "$(dirname "$0")/../.."   # repo root
ORDER="${1:-xz}"
SERVER="mcpp/build/server_chunk_cases.tsv"
JAVA=$(find 26.1.2/jdk25 -name java.exe | head -1)
JAVAC=$(find 26.1.2/jdk25 -name javac.exe | head -1)
CP="26.1.2/parity_classes;26.1.2/client.jar;26.1.2/libs/*"

echo "compiling..."
"$JAVAC" -cp "26.1.2/client.jar;26.1.2/libs/*" -d 26.1.2/parity_classes mcpp/tools/FullChunkDecorateParity.java 2>&1 | grep -v "Note:" | head
[ -f 26.1.2/parity_classes/FullChunkDecorateParity.class ] || { echo "compile failed"; exit 1; }

CHUNKS="0 0;0 1;1 1;-1 -1;2 3;3 -2"
TOTAL=0
TMPALL=$(mktemp)
IFS=';'
for cc in $CHUNKS; do
  IFS=' ' read -r cx cz <<< "$cc"
  OUT="mcpp/build/deco_gt_${cx}_${cz}.tsv"
  "$JAVA" -cp "$CP" FullChunkDecorateParity 1 "$cx" "$cz" "$ORDER" >"$OUT" 2>/dev/null
  REP=$(bash mcpp/tools/compare_deco.sh "$OUT" "$SERVER" "$cx" "$cz")
  M=$(echo "$REP" | head -1 | grep -o 'mismatches=[0-9]*' | cut -d= -f2)
  TOTAL=$((TOTAL + ${M:-0}))
  printf "chunk (%2s,%2s): mismatches=%s\n" "$cx" "$cz" "$M"
  echo "$REP" | awk '/^[0-9]+\t/{print}' >> "$TMPALL"
  IFS=';'
done
unset IFS
echo "================ TOTAL mismatches (order=$ORDER): $TOTAL / 589824 ================"
echo "--- aggregated transitions (harness => server) ---"
awk -F'\t' '{c[$2]+=$1} END{for(t in c) printf "%6d  %s\n", c[t], t}' "$TMPALL" | sort -rn | head -30
rm -f "$TMPALL"
