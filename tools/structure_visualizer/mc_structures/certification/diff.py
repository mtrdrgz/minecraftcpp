"""Block-by-block certification tool.

This is the heart of the new methodology. It compares a generator's output
against a reference layout and reports EVERY discrepancy:
  - missing:   reference has block at (x,y,z) but generator didn't emit it
  - extra:     generator emitted block at (x,y,z) but reference doesn't have it
  - misplaced: both have a block at (x,y,z) but the block_type differs

The certification PASSES only if all three lists are empty. Any non-empty
list means the structure is NOT 1:1 with the reference and must be fixed.

Usage:
    from mc_structures.certification.diff import certify
    result = certify(generator_output, reference_layout, "fossils")
    if not result.passed:
        result.print_report()
        sys.exit(1)
"""

from __future__ import annotations

from dataclasses import dataclass, field
from typing import Iterable

from ..blocks import Block


@dataclass
class DiffEntry:
    """A single block discrepancy."""
    kind: str       # "missing" | "extra" | "misplaced"
    x: int
    y: int
    z: int
    expected: str | None   # block_type in reference (None for "extra")
    actual: str | None     # block_type in generator output (None for "missing")


@dataclass
class CertResult:
    """Result of certifying one structure."""
    name: str
    passed: bool
    missing: list[DiffEntry] = field(default_factory=list)
    extra: list[DiffEntry] = field(default_factory=list)
    misplaced: list[DiffEntry] = field(default_factory=list)
    ref_count: int = 0
    gen_count: int = 0

    def print_report(self) -> None:
        if self.passed:
            print(f"[PASS] {self.name}: {self.ref_count} blocks match exactly.")
            return
        print(f"[FAIL] {self.name}")
        print(f"  reference blocks: {self.ref_count}")
        print(f"  generator blocks: {self.gen_count}")
        print(f"  missing:   {len(self.missing)}")
        print(f"  extra:     {len(self.extra)}")
        print(f"  misplaced: {len(self.misplaced)}")

        def _show(label: str, entries: list[DiffEntry], limit: int = 15):
            if not entries:
                return
            print(f"\n  --- {label} (showing up to {limit} of {len(entries)}) ---")
            for e in entries[:limit]:
                if e.kind == "missing":
                    print(f"    ({e.x:4d}, {e.y:3d}, {e.z:4d})  expected={e.expected!r:20s}  actual=<none>")
                elif e.kind == "extra":
                    print(f"    ({e.x:4d}, {e.y:3d}, {e.z:4d})  expected=<none>            actual={e.actual!r}")
                else:
                    print(f"    ({e.x:4d}, {e.y:3d}, {e.z:4d})  expected={e.expected!r:20s}  actual={e.actual!r}")

        _show("MISSING (in reference, not generated)", self.missing)
        _show("EXTRA (in generator, not in reference)", self.extra)
        _show("MISPLACED (position occupied by wrong block_type)", self.misplaced)


def certify(
    generator_output: Iterable[Block],
    reference_layout: Iterable[Block],
    name: str,
) -> CertResult:
    """Compare generator output against reference layout, block by block.

    Returns a CertResult. `result.passed` is True iff the two block sets are
    identical (same positions, same block_type at each position).
    """
    # Build dicts: position -> block_type
    gen: dict[tuple[int, int, int], str] = {}
    for b in generator_output:
        key = (b.x, b.y, b.z)
        # Note: if the generator emits two blocks at the same position,
        # the second one wins. We log this as a generator bug too.
        if key in gen:
            raise ValueError(
                f"Generator emitted two blocks at {key}: "
                f"{gen[key]!r} and {b.block_type!r}"
            )
        gen[key] = b.block_type

    ref: dict[tuple[int, int, int], str] = {}
    for b in reference_layout:
        key = (b.x, b.y, b.z)
        if key in ref:
            raise ValueError(
                f"Reference has two blocks at {key}: "
                f"{ref[key]!r} and {b.block_type!r}"
            )
        ref[key] = b.block_type

    gen_keys = set(gen.keys())
    ref_keys = set(ref.keys())

    missing: list[DiffEntry] = []
    extra: list[DiffEntry] = []
    misplaced: list[DiffEntry] = []

    # Missing: in ref but not in gen
    for key in ref_keys - gen_keys:
        x, y, z = key
        missing.append(DiffEntry("missing", x, y, z, ref[key], None))

    # Extra: in gen but not in ref
    for key in gen_keys - ref_keys:
        x, y, z = key
        extra.append(DiffEntry("extra", x, y, z, None, gen[key]))

    # Misplaced: in both, but block_type differs
    for key in gen_keys & ref_keys:
        if gen[key] != ref[key]:
            x, y, z = key
            misplaced.append(DiffEntry("misplaced", x, y, z, ref[key], gen[key]))

    # Sort by (y, x, z) for stable reporting
    missing.sort(key=lambda e: (e.y, e.x, e.z))
    extra.sort(key=lambda e: (e.y, e.x, e.z))
    misplaced.sort(key=lambda e: (e.y, e.x, e.z))

    passed = not (missing or extra or misplaced)

    return CertResult(
        name=name,
        passed=passed,
        missing=missing,
        extra=extra,
        misplaced=misplaced,
        ref_count=len(ref),
        gen_count=len(gen),
    )
