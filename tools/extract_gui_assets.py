"""extract_gui_assets.py — pull the GUI/font/text assets the build embeds,
renamed 1:1 from their client.jar source paths into mcpp/src/assets/."""
import zipfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
JAR = ROOT / "26.1.2" / "client.jar"
OUT = ROOT / "mcpp" / "src" / "assets"

MAP = {
    "assets/minecraft/textures/font/ascii.png": "ascii.png",
    "assets/minecraft/textures/gui/title/minecraft.png": "gui_logo.png",
    "assets/minecraft/textures/gui/title/edition.png": "gui_edition.png",
    "assets/minecraft/textures/gui/sprites/widget/button.png": "gui_button.png",
    "assets/minecraft/textures/gui/sprites/widget/button_highlighted.png": "gui_button_hl.png",
    "assets/minecraft/textures/gui/menu_background.png": "gui_dirt.png",
    "assets/minecraft/textures/gui/sprites/icon/language.png": "gui_lang.png",
    "assets/minecraft/textures/gui/sprites/icon/accessibility.png": "gui_access.png",
    "assets/minecraft/texts/splashes.txt": "splashes.txt",
}

with zipfile.ZipFile(JAR) as z:
    names = set(z.namelist())
    for src, dst in MAP.items():
        if src not in names:
            print("MISSING", src)
            continue
        data = z.read(src)
        (OUT / dst).write_bytes(data)
        print(f"  {dst:18s} <- {src}  ({len(data)} B)")
print("done")
