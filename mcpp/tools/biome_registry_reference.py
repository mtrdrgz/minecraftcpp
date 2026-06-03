#!/usr/bin/env python3
"""Independent reference for the C++ biome registry loader.

Reads the canonical 26.1.2 worldgen biome JSON with a *different* parser
(Python's json) and emits the exact same normalised text view that
BiomeRegistryParityTest's `--dump` produces. Diffing the two outputs validates
the C++ field mapping (defaults, colour parsing, list/category ordering,
float widths) for every biome end to end.

Usage:
    python3 biome_registry_reference.py <biome_dir>   # prints normalised dump

Float-typed fields (temperature/downfall/probabilities) are rounded to 32-bit
to match the C++ `float` model (Minecraft uses float for these); spawn-cost
energy/charge stay double. Numbers are printed with %.9g exactly like the C++.
"""
import json
import os
import struct
import sys


def f32(x):
    return struct.unpack("<f", struct.pack("<f", float(x)))[0]


def g9(x):
    return "%.9g" % x


def color(s):
    if isinstance(s, int):
        v = s
    else:
        v = int(s[1:] if s.startswith("#") else s, 16)
    return "#%06x" % (v & 0xFFFFFF)


def opt_color(obj, key):
    return color(obj[key]) if key in obj else "-"


def main(directory):
    out = []
    files = sorted(f for f in os.listdir(directory) if f.endswith(".json"))
    biomes = []
    for fn in files:
        with open(os.path.join(directory, fn)) as fh:
            biomes.append(("minecraft:" + fn[:-5], json.load(fh)))
    biomes.sort(key=lambda kv: kv[0])  # match std::map ordering by id

    for bid, j in biomes:
        eff = j["effects"]
        attrs = j.get("attributes", {})
        temp_mod = j.get("temperature_modifier", "none")
        out.append("%s\tCLIMATE\t%d\t%s\t%s\t%s" % (
            bid, 1 if j["has_precipitation"] else 0,
            g9(f32(j["temperature"])), g9(f32(j["downfall"])), temp_mod))
        out.append("%s\tEFFECTS\t%s\t%s\t%s\t%s\t%s" % (
            bid, color(eff["water_color"]), opt_color(eff, "foliage_color"),
            opt_color(eff, "grass_color"), opt_color(eff, "dry_foliage_color"),
            eff.get("grass_color_modifier", "none")))

        def av(k):
            return attrs.get("minecraft:" + k)
        sky = color(av("visual/sky_color")) if av("visual/sky_color") is not None else "-"
        fog = color(av("visual/fog_color")) if av("visual/fog_color") is not None else "-"
        wfog = color(av("visual/water_fog_color")) if av("visual/water_fog_color") is not None else "-"
        wfe = av("visual/water_fog_end_distance")
        wfe_s = json.dumps(wfe, separators=(",", ":"), sort_keys=True) if wfe is not None else "-"
        parts = av("visual/ambient_particles")
        if parts:
            ps = ",".join("%s:%s" % (p["particle"]["type"], g9(f32(p["probability"]))) for p in parts)
        else:
            ps = "-"
        out.append("%s\tATTR\tsky=%s\tfog=%s\twfog=%s\twfogEnd=%s\tparticles=%s" % (
            bid, sky, fog, wfog, wfe_s, ps))

        carvers = j["carvers"]
        carvers = carvers if isinstance(carvers, list) else [carvers]
        out.append("%s\tCARVERS\t%s" % (bid, ",".join(carvers)))

        for step, feats in enumerate(j["features"]):
            feats = feats if isinstance(feats, list) else [feats]
            out.append("%s\tFEAT\t%d\t%s" % (bid, step, ",".join(feats)))

        for cat in sorted(j["spawners"].keys()):
            entries = j["spawners"][cat]
            s = ",".join("%s:%d:%d:%d" % (e["type"], e["weight"], e["minCount"], e["maxCount"]) for e in entries)
            out.append("%s\tSPAWN\t%s\t%s" % (bid, cat, s if s else "-"))

        out.append("%s\tSPAWNPROB\t%s" % (bid, g9(f32(j.get("creature_spawn_probability", 0.1)))))

        for entity in sorted(j["spawn_costs"].keys()):
            c = j["spawn_costs"][entity]
            out.append("%s\tCOST\t%s\t%s\t%s" % (bid, entity, g9(c["energy_budget"]), g9(c["charge"])))

    sys.stdout.write("\n".join(out) + "\n")


if __name__ == "__main__":
    main(sys.argv[1] if len(sys.argv) > 1 else "26.1.2/data/minecraft/worldgen/biome")
