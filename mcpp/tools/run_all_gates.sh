#!/usr/bin/env bash
# run_all_gates.sh — run every built *_parity.exe against its oracle TSV and classify
# GREEN (rc==0, no mismatches>0) / RED / NEEDS-ORACLE. Used to reconcile PORT_COVERAGE.tsv
# with reality (many byte-exact gates were green but their classes still marked unvisited).
# Output: one line per gate to stdout; GREEN lines also carry the summary line.
set -uo pipefail
cd "$(dirname "$0")/../.."        # repo root
BUILD=mcpp/build
SRC=mcpp/src
TO=""; command -v timeout >/dev/null 2>&1 && TO="timeout 60"
cd "$BUILD"
g=0; r=0; n=0
for exe in *_parity.exe; do
  [ -f "$exe" ] || continue
  base="${exe%_parity.exe}"
  out=$($TO ./"$exe" </dev/null 2>&1); rc=$?
  if echo "$out" | grep -qi 'usage'; then
    oracle=$(grep -rhoE "${base}_parity --cases [A-Za-z0-9_/.]+\.tsv" ../src 2>/dev/null | head -1 | grep -oE '[A-Za-z0-9_]+\.tsv' | tail -1)
    if [ -z "$oracle" ]; then echo "NEEDS-ORACLE-UNKNOWN	$base"; n=$((n+1)); continue; fi
    if [ ! -f "$oracle" ]; then echo "NEEDS-ORACLE-MISSING	$base	$oracle"; n=$((n+1)); continue; fi
    out=$($TO ./"$exe" --cases "$oracle" </dev/null 2>&1); rc=$?
  fi
  if [ $rc -eq 0 ] && ! echo "$out" | grep -qiE 'mismatch(es)?=[1-9]'; then
    line=$(echo "$out" | grep -iE 'mismatch|cases|checks|certified' | head -1)
    echo "GREEN	$base	$line"; g=$((g+1))
  else
    echo "RED($rc)	$base	$(echo "$out" | head -1)"; r=$((r+1))
  fi
done
echo "==== SUMMARY: GREEN=$g RED=$r NEEDS-ORACLE=$n ====" >&2
