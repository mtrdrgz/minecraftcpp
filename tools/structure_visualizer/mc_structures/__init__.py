"""Minecraft structure generator with block-by-block certification.

Each structure is a pure function that returns a list of Block instances.
Each structure also has a reference layout (hardcoded from Minecraft Wiki
canonical data) that the certification system uses to verify the generator
output block-by-block.

Architecture:
    mc_structures/
        __init__.py
        blocks.py            # Block dataclass + color palette
        renderer.py          # voxel renderer with no "missing block" bug
        structures/          # one file per structure (generator + reference)
            __init__.py
            fossils.py
            mineshaft.py
            mansion.py
            ...
        certification/
            diff.py          # block-by-block diff tool
            certify_all.py   # runs diff for every structure, aborts on mismatch
"""
from .blocks import Block, BLOCK_COLORS, get_color

__all__ = ["Block", "BLOCK_COLORS", "get_color"]
