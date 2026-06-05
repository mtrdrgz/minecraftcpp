"""
gen_block_states.py — Generate block state ID mapping from client.jar + Blocks.java
Outputs: mcpp/src/assets/block_states.json
"""
import os
import zipfile, json, re
from pathlib import Path
from collections import defaultdict

ROOT        = Path(__file__).resolve().parents[1]
JAR         = Path(os.environ.get("MCPP_CLIENT_JAR", ROOT / "26.1.2" / "client.jar"))
SRC         = Path(os.environ.get("MCPP_BLOCK_SRC", ROOT / "26.1.2" / "src" / "net" / "minecraft" / "world" / "level" / "block"))
ATLAS_JSON  = Path(os.environ.get("MCPP_ATLAS_JSON", ROOT / "mcpp" / "src" / "assets" / "block_atlas.json"))
BLOCKS_JAVA = SRC / "Blocks.java"
OUT         = Path(os.environ.get("MCPP_BLOCK_STATES_OUT", ROOT / "mcpp" / "src" / "assets" / "block_states.json"))

# Known atlas textures
with open(ATLAS_JSON) as f:
    KNOWN_TEXTURES = set(json.load(f)["textures"].keys())

# ── Load BlockIds.java → constant name → block name map ──────────────────────
blockid_map = {}  # "DIRT" -> "dirt"
refs_path = SRC.parent.parent.parent / "references" / "BlockIds.java"
if refs_path.exists():
    for m in re.finditer(r'(\w+)\s*=\s*createKey\("([a-z_]+)"\)', refs_path.read_text()):
        blockid_map[m.group(1)] = m.group(2)
print(f"BlockIds: {len(blockid_map)} constants")

# ── Parse blockstates from JAR ─────────────────────────────────────────────────
bs_variant_count = {}  # name -> explicit variant count from JSON
bs_prop_vals     = {}  # name -> {prop: sorted(values)} from variant keys

with zipfile.ZipFile(JAR) as zf:
    bs_files = [n for n in zf.namelist()
                if n.startswith("assets/minecraft/blockstates/") and n.endswith(".json")]
    for path in bs_files:
        name = Path(path).stem
        data = json.loads(zf.read(path))
        props = defaultdict(set)
        variants = 0

        if "variants" in data:
            for key in data["variants"]:
                variants += 1
                if key:
                    for kv in key.split(","):
                        if "=" in kv:
                            k, v = kv.split("=", 1)
                            props[k].add(v)
        elif "multipart" in data:
            # For multipart, count unique property combinations from conditions
            for part in data["multipart"]:
                when = part.get("when", {})
                items = list(when.get("OR", [{}])[0].items()) if "OR" in when else list(when.items())
                for k, v in items:
                    if k != "OR":
                        vs = v if isinstance(v, str) else str(v)
                        for val in vs.split("|"):
                            props[k].add(val)

        # Boolean props should have both values
        for k in list(props.keys()):
            if "true" in props[k] or "false" in props[k]:
                props[k] = {"true", "false"}

        bs_variant_count[name] = variants
        bs_prop_vals[name] = {k: sorted(v) for k, v in props.items()}

print(f"Blockstates: {len(bs_files)} files loaded")

# ── Parse Blocks.java registration order ─────────────────────────────────────
# Single pass: match both register("name",...) and register(BlockIds.CONST,...) in source order
blocks_text = BLOCKS_JAVA.read_text(encoding='utf-8', errors='replace')

reg_order = []
pattern = re.compile(r'register\(\s*(?:"([a-z_]+)"|BlockIds\.(\w+))')
for m in pattern.finditer(blocks_text):
    if m.group(1):
        name = m.group(1)
    else:
        # Resolve BlockIds constant
        name = blockid_map.get(m.group(2), m.group(2).lower())
    if name and name not in reg_order:
        reg_order.append(name)

# Append any blockstate blocks not in the parsed order (custom registered elsewhere)
for name in sorted(bs_prop_vals.keys()):
    if name not in reg_order:
        reg_order.append(name)

print(f"Registration order: {len(reg_order)} blocks")

# ── Block class → waterlogged detection ──────────────────────────────────────
# Blocks that have WATERLOGGED property not visible in blockstates variants
waterlogged_classes = set()
for java_file in SRC.glob("*.java"):
    text = java_file.read_text(encoding='utf-8', errors='replace')
    if "WATERLOGGED" in text and "createBlockStateDefinition" in text:
        waterlogged_classes.add(java_file.stem)

# Map block name → class name (from Blocks.java)
class_map = {}
for m in re.finditer(r'register\(\s*"([a-z_]+)"\s*,\s*([A-Z]\w*)::new', blocks_text):
    class_map[m.group(1)] = m.group(2)
# Also handle new ClassName(...)
for m in re.finditer(r'register\(\s*"([a-z_]+)"\s*,[^)]*?\bnew\s+([A-Z]\w+)\b', blocks_text):
    if m.group(1) not in class_map:
        class_map[m.group(1)] = m.group(2)

print(f"Blocks with WATERLOGGED: ~{len(waterlogged_classes)} classes")

def block_has_waterlogged(name: str) -> bool:
    cls = class_map.get(name)
    if cls and cls in waterlogged_classes:
        return True
    # Heuristic: common blocks with waterlogged but no visible variants
    waterlogged_keywords = ["stairs", "slab", "fence", "wall", "sign", "door",
                             "trapdoor", "button", "pressure", "anvil", "chest",
                             "barrel", "chain", "lantern", "coral", "sea",
                             "scaffolding", "lily", "iron_bars"]
    return any(kw in name for kw in waterlogged_keywords)

# ── Compute state count per block ─────────────────────────────────────────────
def get_state_count(block_name: str) -> int:
    explicit = bs_variant_count.get(block_name, 0)
    props = bs_prop_vals.get(block_name, {})

    if not props and explicit <= 1:
        # Simple block or multipart with no visible properties
        # Still might have WATERLOGGED
        count = 1
    else:
        count = 1
        for vals in props.values():
            count *= max(len(vals), 1)
        if count < explicit:
            count = explicit

    # Add WATERLOGGED dimension if not already in the counted properties
    if block_has_waterlogged(block_name) and "waterlogged" not in props:
        count *= 2

    # For multipart blocks with only partial property enumeration,
    # add common missing properties
    return max(count, 1)

# ── Texture lookup ────────────────────────────────────────────────────────────
SPECIALS = {
    "grass_block":       ("grass_block_top", "grass_block_side", "dirt"),
    "podzol":            ("podzol_top",       "podzol_side",       "dirt"),
    "mycelium":          ("mycelium_top",      "mycelium_side",     "dirt"),
    "farmland":          ("farmland_moist",    "dirt",              "dirt"),
    "dirt_path":         ("dirt_path_top",     "dirt_path_side",    "dirt"),
    "tnt":               ("tnt_top",           "tnt_side",          "tnt_bottom"),
    "crafting_table":    ("crafting_table_top","crafting_table_side","oak_planks"),
    "bookshelf":         ("oak_planks",        "bookshelf",          "oak_planks"),
    "pumpkin":           ("pumpkin_top",       "pumpkin_side",       "pumpkin_top"),
    "carved_pumpkin":    ("pumpkin_top",       "carved_pumpkin",     "pumpkin_top"),
    "jack_o_lantern":    ("pumpkin_top",       "jack_o_lantern",     "pumpkin_top"),
    "melon":             ("melon_top",         "melon_side",          "melon_top"),
    "hay_block":         ("hay_block_top",     "hay_block_side",      "hay_block_top"),
    "bone_block":        ("bone_block_top",    "bone_block_side",     "bone_block_top"),
    "purpur_pillar":     ("purpur_pillar_top", "purpur_pillar",       "purpur_pillar_top"),
    "quartz_pillar":     ("quartz_pillar_top", "quartz_pillar",       "quartz_pillar_top"),

    # Utility / interaction blocks
    "lectern":           ("lectern_top",       "lectern_sides",       "lectern_base"),
    "furnace":           ("furnace_top",       "furnace_side",        "furnace_top"),
    "blast_furnace":     ("blast_furnace_top", "blast_furnace_side",  "blast_furnace_top"),
    "smoker":            ("smoker_top",        "smoker_side",         "smoker_bottom"),
    "loom":              ("loom_top",          "loom_side",           "loom_bottom"),
    "barrel":            ("barrel_top",        "barrel_side",         "barrel_bottom"),
    "composter":         ("composter_top",     "composter_side",      "composter_bottom"),
    "stonecutter":       ("stonecutter_top",   "stonecutter_side",    "stonecutter_bottom"),
    "cartography_table": ("cartography_table_top", "cartography_table_side1", "cartography_table_side3"),
    "fletching_table":   ("fletching_table_top", "fletching_table_side", "fletching_table_top"),
    "smithing_table":    ("smithing_table_top", "smithing_table_side", "smithing_table_bottom"),
    "enchanting_table":  ("enchanting_table_top", "enchanting_table_side", "enchanting_table_bottom"),
    "grindstone":        ("grindstone_round",  "grindstone_side",     "grindstone_round"),
    "jukebox":           ("jukebox_top",       "jukebox_side",        "jukebox_side"),
    "note_block":        ("note_block",        "note_block",          "note_block"),
    "beacon":            ("beacon",            "beacon",              "beacon"),
    "conduit":           ("conduit",           "conduit",             "conduit"),
    "spawner":           ("spawner",           "spawner",             "spawner"),
    "end_portal_frame":  ("end_portal_frame_top", "end_portal_frame_side", "end_portal_frame_side"),

    # Sandstone family
    "sandstone":         ("sandstone_top",     "sandstone",           "sandstone_bottom"),
    "chiseled_sandstone": ("sandstone_top",    "chiseled_sandstone",  "sandstone_bottom"),
    "cut_sandstone":     ("sandstone_top",     "cut_sandstone",       "sandstone_bottom"),
    "smooth_sandstone":  ("sandstone_top",     "sandstone_top",       "sandstone_top"),
    "red_sandstone":     ("red_sandstone_top", "red_sandstone",       "red_sandstone_bottom"),
    "chiseled_red_sandstone": ("red_sandstone_top", "chiseled_red_sandstone", "red_sandstone_bottom"),
    "cut_red_sandstone": ("red_sandstone_top", "cut_red_sandstone",   "red_sandstone_bottom"),
    "smooth_red_sandstone": ("red_sandstone_top", "red_sandstone_top", "red_sandstone_top"),

    # Mushroom blocks
    "red_mushroom_block": ("red_mushroom_block", "red_mushroom_block", "red_mushroom_block"),
    "brown_mushroom_block": ("brown_mushroom_block", "brown_mushroom_block", "brown_mushroom_block"),

    # Nether / basalt / soul
    "magma_block":       ("magma",             "magma",               "magma"),
    "polished_basalt":   ("polished_basalt_top", "polished_basalt_side", "polished_basalt_top"),

    # Deepslate (pillar-like)
    "deepslate":         ("deepslate_top",     "deepslate",           "deepslate_top"),

    # Roots
    "muddy_mangrove_roots": ("muddy_mangrove_roots_top", "muddy_mangrove_roots_side", "muddy_mangrove_roots_top"),
    "mangrove_roots":    ("mangrove_roots_top", "mangrove_roots_side", "mangrove_roots_top"),

    # Froglights (pillar-like)
    "ochre_froglight":   ("ochre_froglight_top", "ochre_froglight_side", "ochre_froglight_top"),
    "verdant_froglight": ("verdant_froglight_top", "verdant_froglight_side", "verdant_froglight_top"),
    "pearlescent_froglight": ("pearlescent_froglight_top", "pearlescent_froglight_side", "pearlescent_froglight_top"),

    # Sculk family
    "sculk_catalyst":    ("sculk_catalyst_top", "sculk_catalyst_side", "sculk_catalyst_bottom"),
    "sculk_sensor":      ("sculk_sensor_top",  "sculk_sensor_side",   "sculk_sensor_bottom"),
    "calibrated_sculk_sensor": ("calibrated_sculk_sensor_top", "sculk_sensor_side", "sculk_sensor_bottom"),
    "sculk_shrieker":    ("sculk_shrieker_top", "sculk_shrieker_side", "sculk_shrieker_bottom"),

    # Bee blocks (beehive has no _top — uses _end for both)
    "beehive":           ("beehive_end",       "beehive_side",        "beehive_end"),
    "bee_nest":          ("bee_nest_top",      "bee_nest_side",       "bee_nest_bottom"),

    # New/recent blocks
    "crafter":           ("crafter_top",       "crafter_east",        "crafter_bottom"),
    "trial_spawner":     ("trial_spawner_top_inactive", "trial_spawner_side_inactive", "trial_spawner_bottom"),
    "vault":             ("vault_top",         "vault_side_off",      "vault_bottom"),

    # Pistons and observer
    "piston":            ("piston_top",        "piston_side",         "piston_bottom"),
    "sticky_piston":     ("piston_top_sticky", "piston_side",         "piston_bottom"),
    "observer":          ("observer_top",      "observer_side",       "observer_top"),

    # Command blocks
    "command_block":          ("command_block_side", "command_block_side", "command_block_side"),
    "chain_command_block":    ("chain_command_block_side", "chain_command_block_side", "chain_command_block_side"),
    "repeating_command_block": ("repeating_command_block_side", "repeating_command_block_side", "repeating_command_block_side"),

    # Jigsaw
    "jigsaw":            ("jigsaw_top",        "jigsaw_side",         "jigsaw_bottom"),

    # Other distinct top/side/bottom
    "lodestone":         ("lodestone_top",     "lodestone_side",      "lodestone_side"),
    "respawn_anchor":    ("respawn_anchor_top_off", "respawn_anchor_side0", "respawn_anchor_bottom"),
    "cake":              ("cake_top",          "cake_side",           "cake_bottom"),
    "bell":              ("bell_top",          "bell_side",           "bell_bottom"),

    # Chiseled quartz (block name pattern with _block suffix)
    "chiseled_quartz_block": ("chiseled_quartz_block_top", "chiseled_quartz_block", "chiseled_quartz_block_top"),
}

def get_textures(name: str):
    if name in SPECIALS:
        return SPECIALS[name]
    # Log/stem/hyphae/wood: _top for up/down faces
    for suffix in ["_log", "_stem", "_hyphae", "_wood"]:
        if name.endswith(suffix):
            top = name + "_top"
            side = name
            return (top if top in KNOWN_TEXTURES else side,
                    side if side in KNOWN_TEXTURES else "",
                    top if top in KNOWN_TEXTURES else side)
    # Direct match
    if name in KNOWN_TEXTURES:
        return (name, name, name)
    # Blocks that have _top and _side variants (basalt, hay, bone, pillar-like)
    top_cand = name + "_top"
    side_cand = name + "_side"
    if top_cand in KNOWN_TEXTURES and side_cand in KNOWN_TEXTURES:
        return (top_cand, side_cand, top_cand)
    if top_cand in KNOWN_TEXTURES:
        return (top_cand, name if name in KNOWN_TEXTURES else top_cand, top_cand)
    if side_cand in KNOWN_TEXTURES:
        return (side_cand, side_cand, side_cand)
    # Remove common suffixes
    for suffix in ["_block"]:
        bare = name.replace(suffix, "")
        if bare in KNOWN_TEXTURES:
            return (bare, bare, bare)
    # Add _block suffix
    with_block = name + "_block"
    if with_block in KNOWN_TEXTURES:
        return (with_block, with_block, with_block)
    # Try prefix removal (e.g., "polished_granite" -> check "granite")
    parts = name.split("_")
    for i in range(1, len(parts)):
        cand = "_".join(parts[i:])
        if cand in KNOWN_TEXTURES:
            return (cand, cand, cand)
    return ("", "", "")

NON_OPAQUE = {
    "air", "cave_air", "void_air", "water", "lava",
    "glass", "glass_pane", "iron_bars", "barrier", "structure_void",
    "oak_leaves", "spruce_leaves", "birch_leaves", "jungle_leaves",
    "acacia_leaves", "dark_oak_leaves", "mangrove_leaves", "azalea_leaves",
    "flowering_azalea_leaves", "cherry_leaves", "pale_oak_leaves",
    "slime_block", "honey_block", "ice", "frosted_ice",
    "tinted_glass",
} | {c + "_stained_glass" for c in ["white","orange","magenta","light_blue","yellow",
     "lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"]} \
  | {c + "_stained_glass_pane" for c in ["white","orange","magenta","light_blue","yellow",
     "lime","pink","gray","light_gray","cyan","purple","blue","brown","green","red","black"]}

FLUID  = {"water", "lava"}
AIR_S  = {"air", "cave_air", "void_air"}

# ── Assign state IDs and write output ─────────────────────────────────────────
states = []
block_summary = {}
current_id = 0

for block_name in reg_order:
    n = get_state_count(block_name)

    is_air    = block_name in AIR_S
    is_fluid  = block_name in FLUID
    is_opaque = not (is_air or is_fluid or block_name in NON_OPAQUE)
    is_solid  = not (is_air or is_fluid)

    top_tex, side_tex, bot_tex = get_textures(block_name)

    block_summary[block_name] = (current_id, n)

    for i in range(n):
        states.append({
            "id":        current_id + i,
            "name":      block_name,
            "is_air":    is_air,
            "is_opaque": is_opaque,
            "is_solid":  is_solid,
            "is_fluid":  is_fluid,
            "tex_top":   top_tex,
            "tex_side":  side_tex,
            "tex_bot":   bot_tex,
        })

    current_id += n

print(f"\nTotal states: {current_id}")

with open(OUT, "w") as f:
    json.dump({"total": current_id, "states": states}, f, separators=(",", ":"))
print(f"Saved {OUT} ({OUT.stat().st_size // 1024} KB)")

print("\nKey block IDs:")
for name in ["air","stone","granite","grass_block","dirt","cobblestone",
             "oak_planks","sand","gravel","water","lava","oak_log",
             "oak_leaves","glass","oak_stairs"]:
    if name in block_summary:
        first, cnt = block_summary[name]
        print(f"  {name}: {first}..{first+cnt-1} ({cnt} states)")
