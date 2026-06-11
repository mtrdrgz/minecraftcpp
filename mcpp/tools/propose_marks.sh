#!/usr/bin/env bash
# propose_marks.sh — from GREEN gate results, derive the target Java class per gate
# (first Capitalized class token in the test-file header), resolve to a ledger path,
# and emit "path<TAB>status<TAB>base<TAB>summary" ONLY for paths that are currently
# `unvisited` rows in PORT_COVERAGE.tsv. Does NOT modify the ledger (review first).
set -uo pipefail
cd "$(dirname "$0")/../.."
LEDGER=mcpp/docs/PORT_COVERAGE.tsv
MAP=mcpp/build/target_source.map
RES=mcpp/build/gate_results.txt

# unvisited path set (for O(1) membership)
declare -A UNV
while IFS=$'\t' read -r p s e; do [ "$s" = "unvisited" ] && UNV["$p"]=1; done < "$LEDGER"

# token -> ledger path: split on '.', append segments until the FIRST Uppercase-initial
# segment (the class file); drop trailing inner-class/method segments.
tok2path() {
  local tok="$1" IFS='.' seg out=""
  for seg in $tok; do
    case "$seg" in
      [A-Z]*) out+="$seg"; echo "${out}.java"; return;;
      *) out+="$seg/";;
    esac
  done
  echo ""   # no class segment found
}

while IFS=$'\t' read -r tag base summary; do
  [ "$tag" = "GREEN" ] || continue
  src=$(awk -F'\t' -v t="${base}_parity" '$1==t{print $2; exit}' "$MAP")
  [ -z "$src" ] && { echo "NOSRC	$base"; continue; }
  hdr=$(head -18 "mcpp/src/$src" 2>/dev/null)
  # candidate class tokens from the header
  mapfile -t toks < <(echo "$hdr" | grep -ohE '(net\.minecraft|com\.mojang)\.[A-Za-z0-9_.]+' | sed -E 's/\.+$//' | sort -u)
  picked=""
  for tk in "${toks[@]}"; do
    p=$(tok2path "$tk")
    [ -z "$p" ] && continue
    if [ -n "${UNV[$p]:-}" ]; then picked="$p"; break; fi
  done
  if [ -n "$picked" ]; then
    echo "MARK	$picked	$base	$summary"
  else
    # resolved a class but it's already non-unvisited, or no net/mojang token
    first=""; [ "${#toks[@]}" -gt 0 ] && first=$(tok2path "${toks[0]}")
    echo "SKIP	${first:-<no-class-token>}	$base"
  fi
done < "$RES"
