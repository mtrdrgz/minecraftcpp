#!/usr/bin/env bash
# port_coverage.sh — maintain the master 1:1 port coverage ledger.
#
# The goal requires visiting EVERY decompiled Java file at least once and recording
# what happened to it. The ledger is mcpp/docs/PORT_COVERAGE.tsv with one row per
# Java file under 26.1.2/src:
#
#   path<TAB>status<TAB>evidence
#
#   status   one of:
#     unvisited  nobody has read this file against the port yet (default)
#     ported     behaviour fully reimplemented in C++; evidence = C++ file(s) + the
#                parity gate / test that PROVES equivalence (never "looks right")
#     partial    some behaviour ported with proof, the rest enumerated in evidence
#     no-op      intentionally not executed yet; the port FAILS CLOSED where this
#                file's behaviour would be needed (logged hard no-op), never silent
#     n/a        genuinely outside the port's scope (e.g. realms client, telemetry,
#                datafix schemas for old save formats) — evidence says why
#   evidence  free text: C++ files, parity targets, commit hashes, reasons.
#
# Commands:
#   port_coverage.sh init        create the ledger for any Java files not yet listed
#                                (idempotent merge; existing rows are preserved)
#   port_coverage.sh summary     status counts, total and per top-level package
#   port_coverage.sh mark <status> <evidence> <path-regex>
#                                set status+evidence for all rows whose path matches
#                                the (extended) regex AND are currently unvisited;
#                                prints every row it changes. Use mark-force to also
#                                overwrite non-unvisited rows.
#   port_coverage.sh mark-force <status> <evidence> <path-regex>
#   port_coverage.sh show <path-regex>   print matching rows
set -euo pipefail
cd "$(dirname "$0")/../.."   # repo root
LEDGER="mcpp/docs/PORT_COVERAGE.tsv"
SRC="26.1.2/src"

cmd="${1:-summary}"

init() {
    mkdir -p "$(dirname "$LEDGER")"
    touch "$LEDGER"
    local tmp added
    tmp=$(mktemp)
    # existing paths -> keep; new paths -> unvisited
    cut -f1 "$LEDGER" | sort > "$tmp.existing"
    find "$SRC" -name "*.java" | sed "s|^$SRC/||" | sort > "$tmp.all"
    added=0
    while IFS= read -r p; do
        printf '%s\tunvisited\t\n' "$p" >> "$LEDGER"
        added=$((added+1))
    done < <(comm -13 "$tmp.existing" "$tmp.all")
    sort -o "$LEDGER" "$LEDGER"
    rm -f "$tmp" "$tmp.existing" "$tmp.all"
    echo "ledger: $(wc -l < "$LEDGER") files ($added added)"
}

summary() {
    echo "== total =="
    awk -F'\t' '{c[$2]++} END{for(s in c) printf "  %-10s %d\n", s, c[s]; }' "$LEDGER" | sort
    echo "== unvisited by top package (top 25) =="
    awk -F'\t' '$2=="unvisited"{n=split($1,a,"/"); pkg=a[1]; for(i=2;i<n && i<=4;i++) pkg=pkg"/"a[i]; c[pkg]++}
                END{for(p in c) printf "  %6d  %s\n", c[p], p}' "$LEDGER" | sort -rn | head -25
}

mark() {  # $1 force? $2 status $3 evidence $4 regex
    local force="$1" status="$2" evidence="$3" regex="$4" tmp
    tmp=$(mktemp)
    awk -F'\t' -v OFS='\t' -v re="$regex" -v st="$status" -v ev="$evidence" -v force="$force" '
        $1 ~ re && (force=="1" || $2=="unvisited") { print "  " $1 ": " $2 " -> " st > "/dev/stderr"; $2=st; $3=ev }
        { print }
    ' "$LEDGER" > "$tmp"
    mv "$tmp" "$LEDGER"
}

case "$cmd" in
    init) init ;;
    summary) summary ;;
    mark)       mark 0 "$2" "$3" "$4"; summary | head -8 ;;
    mark-force) mark 1 "$2" "$3" "$4"; summary | head -8 ;;
    show) grep -E "$2" "$LEDGER" | head -50 ;;
    *) echo "usage: port_coverage.sh init|summary|mark|mark-force|show"; exit 2 ;;
esac
