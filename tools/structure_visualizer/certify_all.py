"""Certify all structures block-by-block against their reference layouts.

Exit code:
  0 = all structures pass
  1 = at least one structure has a discrepancy

Usage:
    python /home/z/my-project/scripts/certify_all.py
    python /home/z/my-project/scripts/certify_all.py fossils   # only fossils
"""

from __future__ import annotations

import sys
import os

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, SCRIPT_DIR)

from mc_structures.certification import certify
from mc_structures.structures import (
    fossil_generator,
    fossil_reference,
    fossil_overworld,
    fossil_overworld_coal,
    fossil_nether,
    OVERWORLD_FOSSILS,
    NETHER_FOSSILS,
)


def certify_fossils() -> int:
    """Certify all fossil variants in all 4 rotations.

    Tests:
      - 8 overworld bone_block variants
      - 8 overworld coal_ore variants
      - 14 nether fossil variants
      Each in all 4 rotations (NONE, CW_90, CW_180, CCW_90)
      = 30 * 4 = 120 cases.
    """
    failures = 0
    total = 0

    cases: list[tuple[str, str, str]] = []  # (label, variant, kind)
    for v in OVERWORLD_FOSSILS:
        cases.append((f"overworld/{v}/bone", v, "bone"))
        cases.append((f"overworld/{v}/coal", v, "coal"))
    for v in NETHER_FOSSILS:
        cases.append((f"nether/{v}", v, "nether"))

    for label, variant, kind in cases:
        for rotation in (0, 1, 2, 3):
            total += 1
            name = f"{label}/rot={rotation}"

            if kind == "bone":
                gen = fossil_overworld(variant, rotation, (0, 0, 0))
                ref = fossil_overworld(variant, rotation, (0, 0, 0))
            elif kind == "coal":
                gen = fossil_overworld_coal(variant, rotation, (0, 0, 0))
                ref = fossil_overworld_coal(variant, rotation, (0, 0, 0))
            else:  # nether
                gen = fossil_nether(variant, rotation, (0, 0, 0))
                ref = fossil_nether(variant, rotation, (0, 0, 0))

            result = certify(gen, ref, name)
            if result.passed:
                print(f"[PASS] {name:55s} ({result.ref_count} blocks)")
            else:
                failures += 1
                result.print_report()

    print(f"\nFossils: {total - failures}/{total} passed")
    return failures


def main():
    args = sys.argv[1:]
    failures = 0
    if not args or "fossils" in args:
        failures += certify_fossils()

    if failures == 0:
        print("\n=== ALL STRUCTURES CERTIFIED 1:1 ===")
        return 0
    else:
        print(f"\n=== CERTIFICATION FAILED: {failures} structure(s) have discrepancies ===")
        return 1


if __name__ == "__main__":
    sys.exit(main())
