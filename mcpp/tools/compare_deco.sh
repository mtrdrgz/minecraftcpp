#!/usr/bin/env bash
# Compare a FullChunkDecorateParity dump (chunk C, FullChunkParity TSV) against the
# server .mca ground truth (server_chunk_cases.tsv, same format). Counts mismatches,
# categorized by distance-from-chunk-edge (0 = border) and block transition.
#   compare_deco.sh <harness.tsv> <server_cases.tsv> <cx> <cz>
H="$1"; S="$2"; CX="$3"; CZ="$4"
awk -F'\t' -v cx="$CX" -v cz="$CZ" '
  function chunkc(b){ return (b>=0?int(b/16):int((b-15)/16)) }
  function edgedist(bx,bz,  lx,lz,dx,dz){ lx=bx-cx*16; lz=bz-cz*16; dx=(lx<15-lx?lx:15-lx); dz=(lz<15-lz?lz:15-lz); return (dx<dz?dx:dz) }
  NR==FNR { if(chunkc($2)==cx && chunkc($3)==cz) srv[$2","$3","$4]=$5; next }
  { k=$2","$3","$4; if(!(k in srv)) next; seen++;
    if(srv[k]!=$5){ mm++; dist[edgedist($2,$3)]++; trans[$5" => "srv[k]]++ } }
  END {
    printf "compared=%d mismatches=%d\n", seen, mm;
    print "--- mismatches by edge-distance (0=border) ---";
    for(d=0;d<=7;d++) if(dist[d]>0) printf "  dist %d: %d\n", d, dist[d];
    print "--- block transitions (harness => server), top by count ---";
    for(t in trans) printf "%d\t%s\n", trans[t], t | "sort -rn | head -25";
  }
' "$S" "$H"
