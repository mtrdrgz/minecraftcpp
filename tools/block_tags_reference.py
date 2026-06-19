#!/usr/bin/env python3
"""Independent reference for the C++ BlockTags resolver.

Resolves data/minecraft/tags/block/*.json (recursively expanding "#tag" refs)
with Python and emits, for every tag, its flat sorted member set — the same
normalised dump BlockTagsParityTest's `--dump` produces. Diffing the two
validates the C++ tag resolver against the canonical data.

Usage: python3 block_tags_reference.py <block_tag_dir>
"""
import json
import os
import sys


def normalize(i):
    return i if ":" in i else "minecraft:" + i


def main(directory):
    raw = {}
    for fn in os.listdir(directory):
        if not fn.endswith(".json"):
            continue
        with open(os.path.join(directory, fn)) as fh:
            j = json.load(fh)
        name = "minecraft:" + fn[:-5]
        entries = []
        for v in j["values"]:
            vid = v if isinstance(v, str) else v["id"]
            if vid.startswith("#"):
                entries.append("#" + normalize(vid[1:]))
            else:
                entries.append(normalize(vid))
        raw[name] = entries

    memo = {}

    def resolve(tag, visiting):
        if tag in memo:
            return memo[tag]
        out = set()
        if tag not in visiting:
            visiting = visiting | {tag}
            for e in raw.get(tag, []):
                if e.startswith("#"):
                    out |= resolve(e[1:], visiting)
                else:
                    out.add(e)
        memo[tag] = out
        return out

    lines = []
    for tag in sorted(raw):
        members = sorted(resolve(tag, set()))
        lines.append(tag + "\t" + ",".join(members))
    sys.stdout.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "26.1.2/data/minecraft/tags/block")
