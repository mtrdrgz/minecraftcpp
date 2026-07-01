#!/usr/bin/env python3
"""compare_starts.py — order-insensitive comparison of structure-starts TSVs.

Both inputs use the StructureStartsDump format:
  S\t<structureId>\t<chunkX>\t<chunkZ>\t<references>\t<childCount>
  C\t<pieceId>\t<minX>\t<minY>\t<minZ>\t<maxX>\t<maxY>\t<maxZ>\t<O>\t<GD>

Starts are keyed by (chunkX, chunkZ, structureId); the C-row list of a start is
compared verbatim (order matters — it is the builder/piece order). Chunk sets may
differ between files (the server .mca includes a padding ring beyond the
forceloaded rect), so comparison is restricted to chunks present in --chunks
(a status TSV: cx\tcz\tstatus) or, without it, to the intersection of chunks
that have any start in either file plus an explicit --region rect if given.

Usage:
  compare_starts.py A.tsv B.tsv [--region fx fz tx tz] [--chunks status.tsv [--status st1,st2]]
"""
import sys
from collections import OrderedDict


def parse(path):
    starts = OrderedDict()  # (cx, cz, id) -> (refs, [C rows])
    cur = None
    with open(path) as f:
        for line in f:
            line = line.rstrip("\n")
            if line.startswith("S\t"):
                _, sid, cx, cz, refs, cc = line.split("\t")
                cur = (int(cx), int(cz), sid)
                starts[cur] = (int(refs), [])
            elif line.startswith("C\t") and cur is not None:
                starts[cur][1].append(line)
    return starts


def main():
    args = sys.argv[1:]
    a_path, b_path = args[0], args[1]
    region = None
    chunks_file = None
    statuses = None
    i = 2
    while i < len(args):
        if args[i] == "--region":
            region = tuple(int(v) for v in args[i + 1:i + 5]); i += 5
        elif args[i] == "--chunks":
            chunks_file = args[i + 1]; i += 2
        elif args[i] == "--status":
            statuses = set(args[i + 1].split(",")); i += 2
        else:
            raise SystemExit(f"unknown arg {args[i]}")

    a = parse(a_path)
    b = parse(b_path)

    allowed = None
    if chunks_file:
        allowed = set()
        with open(chunks_file) as f:
            for line in f:
                parts = line.rstrip("\n").split("\t")
                if len(parts) < 3:
                    continue
                if statuses is None or parts[2] in statuses:
                    allowed.add((int(parts[0]), int(parts[1])))

    def in_scope(key):
        cx, cz, _ = key
        if region and not (region[0] <= cx <= region[2] and region[1] <= cz <= region[3]):
            return False
        if allowed is not None and (cx, cz) not in allowed:
            return False
        return True

    a_keys = {k for k in a if in_scope(k)}
    b_keys = {k for k in b if in_scope(k)}

    only_a = sorted(a_keys - b_keys)
    only_b = sorted(b_keys - a_keys)
    both = sorted(a_keys & b_keys)

    piece_mismatches = []
    for k in both:
        if a[k][1] != b[k][1]:
            piece_mismatches.append(k)

    print(f"A starts in scope: {len(a_keys)}   B starts in scope: {len(b_keys)}")
    print(f"matched starts: {len(both) - len(piece_mismatches)}")
    if only_a:
        print(f"\nONLY IN A ({len(only_a)}):")
        for k in only_a:
            print(f"  {k[2]} at ({k[0]},{k[1]}) pieces={len(a[k][1])}")
    if only_b:
        print(f"\nONLY IN B ({len(only_b)}):")
        for k in only_b:
            print(f"  {k[2]} at ({k[0]},{k[1]}) pieces={len(b[k][1])}")
    if piece_mismatches:
        print(f"\nPIECE MISMATCHES ({len(piece_mismatches)}):")
        for k in piece_mismatches:
            pa, pb = a[k][1], b[k][1]
            print(f"  {k[2]} at ({k[0]},{k[1]}): A={len(pa)} pieces, B={len(pb)} pieces")
            for i, (ra, rb) in enumerate(zip(pa, pb)):
                if ra != rb:
                    print(f"    [{i}] A: {ra}")
                    print(f"    [{i}] B: {rb}")
                    if i > 20:
                        break
    ok = not only_a and not only_b and not piece_mismatches
    print("\nRESULT: " + ("BYTE-EXACT" if ok else "MISMATCH"))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
