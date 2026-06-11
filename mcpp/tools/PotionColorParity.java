// Ground-truth generator for the pure color math of
// net.minecraft.world.item.alchemy.PotionContents (26.1.2):
//   * getColorOptional(Iterable<MobEffectInstance>)  — weighted (amplifier+1) sRGB mix
//   * getColorOr(int defaultColor)                   — customColor short-circuit / fallback
//   * getColor()                                     — getColorOr(BASE_POTION_COLOR)
//   * BASE_POTION_COLOR constant
//
// Drives the REAL methods. Effects are built as REAL MobEffectInstance objects from
// REAL MobEffects holders (so isVisible()/getAmplifier()/getEffect().value().getColor()
// all come from the live class). Each effect's color is read from the registry and
// emitted into the TSV so the C++ side stays world-free.
//
// Emits tab-separated rows. All values decimal ints (colors/argb are signed 32-bit).
//   OPT   <n> [<color> <amp> <vis>]*n            -> <present(0|1)> <argb>
//   OR    <hasCustom> <custom> <default> <n> [<color> <amp> <vis>]*n -> <result>
//   COLOR <hasCustom> <custom> <n> [<color> <amp> <vis>]*n           -> <result>
//   CONST BASE_POTION_COLOR <value>
//
//   tools/run_groundtruth.ps1 -Tool PotionColorParity -Out mcpp/build/potion_color.tsv

import java.util.ArrayList;
import java.util.List;
import java.util.Optional;
import java.util.OptionalInt;

import net.minecraft.core.Holder;
import net.minecraft.world.effect.MobEffect;
import net.minecraft.world.effect.MobEffectInstance;
import net.minecraft.world.effect.MobEffects;
import net.minecraft.world.item.alchemy.PotionContents;

public class PotionColorParity {
    static final java.io.PrintStream O = System.out;
    static final StringBuilder SB = new StringBuilder();

    // A small, diverse palette of real effects with distinct colors. Populated in
    // main() AFTER Bootstrap.bootStrap() — referencing MobEffects in a static field
    // initializer would force its <clinit> (and the registries) before bootstrap.
    static Holder<MobEffect>[] PALETTE;

    @SuppressWarnings("unchecked")
    static void initPalette() {
        PALETTE = new Holder[] {
            MobEffects.SPEED, MobEffects.SLOWNESS, MobEffects.HASTE, MobEffects.STRENGTH,
            MobEffects.INSTANT_HEALTH, MobEffects.JUMP_BOOST, MobEffects.NAUSEA, MobEffects.REGENERATION,
            MobEffects.RESISTANCE, MobEffects.FIRE_RESISTANCE, MobEffects.WATER_BREATHING, MobEffects.INVISIBILITY
        };
    }

    // Build a MobEffectInstance with the given amplifier and visibility.
    // Constructor: (effect, duration, amplifier, ambient, visible). amplifier is
    // clamped to [0,255] internally — we feed the same values the C++ side uses.
    static MobEffectInstance inst(Holder<MobEffect> effect, int amplifier, boolean visible) {
        return new MobEffectInstance(effect, 200, amplifier, false, visible);
    }

    // Emit the inline effect list: count, then (color amp vis) per effect, returning
    // the same List for driving the real method.
    static List<MobEffectInstance> appendEffects(int[][] spec) {
        // spec[i] = {paletteIndex, amplifier, visible(0|1)}
        List<MobEffectInstance> list = new ArrayList<>();
        SB.append('\t').append(spec.length);
        for (int[] s : spec) {
            Holder<MobEffect> eff = PALETTE[s[0]];
            MobEffectInstance mi = inst(eff, s[1], s[2] != 0);
            list.add(mi);
            // Read back exactly what getColorOptional reads: registry color, stored
            // (clamped) amplifier, isVisible().
            int color = mi.getEffect().value().getColor();
            int amp = mi.getAmplifier();
            int vis = mi.isVisible() ? 1 : 0;
            SB.append('\t').append(color).append('\t').append(amp).append('\t').append(vis);
        }
        return list;
    }

    static void emitOpt(int[][] spec) {
        SB.setLength(0);
        SB.append("OPT");
        List<MobEffectInstance> list = appendEffects(spec);
        OptionalInt res = PotionContents.getColorOptional(list);
        SB.append('\t').append(res.isPresent() ? 1 : 0)
          .append('\t').append(res.isPresent() ? res.getAsInt() : 0);
        O.println(SB.toString());
    }

    static void emitColorOr(boolean hasCustom, int custom, int dflt, int[][] spec) {
        SB.setLength(0);
        SB.append("OR").append('\t').append(hasCustom ? 1 : 0)
          .append('\t').append(custom).append('\t').append(dflt);
        List<MobEffectInstance> list = appendEffects(spec);
        // potion == empty, customName == empty: getAllEffects() == customEffects list.
        PotionContents pc = new PotionContents(
            Optional.empty(),
            hasCustom ? Optional.of(custom) : Optional.empty(),
            list,
            Optional.empty());
        int res = pc.getColorOr(dflt);
        SB.append('\t').append(res);
        O.println(SB.toString());
    }

    static void emitColor(boolean hasCustom, int custom, int[][] spec) {
        SB.setLength(0);
        SB.append("COLOR").append('\t').append(hasCustom ? 1 : 0).append('\t').append(custom);
        List<MobEffectInstance> list = appendEffects(spec);
        PotionContents pc = new PotionContents(
            Optional.empty(),
            hasCustom ? Optional.of(custom) : Optional.empty(),
            list,
            Optional.empty());
        int res = pc.getColor();
        SB.append('\t').append(res);
        O.println(SB.toString());
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        initPalette();

        // Constant.
        O.println("CONST\tBASE_POTION_COLOR\t" + PotionContents.BASE_POTION_COLOR);

        int n = PALETTE.length;

        // --- getColorOptional: empty -> absent. ---
        emitOpt(new int[0][]);

        // --- Single effect, every palette entry, several amplifiers, visible. ---
        int[] amps = { 0, 1, 2, 3, 4, 9, 31, 63, 127, 200, 255 };
        for (int i = 0; i < n; i++) {
            for (int a : amps) {
                emitOpt(new int[][] { { i, a, 1 } });
            }
        }

        // --- Single effect but invisible -> absent (totalWeight stays 0). ---
        for (int i = 0; i < n; i++) {
            emitOpt(new int[][] { { i, 5, 0 } });
        }

        // --- Pairs of effects across the palette, mixed amplifiers/visibility. ---
        for (int i = 0; i < n; i++) {
            for (int j = 0; j < n; j++) {
                emitOpt(new int[][] { { i, i % 5, 1 }, { j, (j % 3) + 1, 1 } });
                // one visible, one invisible (invisible must not contribute)
                emitOpt(new int[][] { { i, 7, 1 }, { j, 7, 0 } });
            }
        }

        // --- Triples & larger combos to exercise accumulation / integer division. ---
        for (int i = 0; i < n; i++) {
            emitOpt(new int[][] {
                { i, 255, 1 }, { (i + 1) % n, 255, 1 }, { (i + 2) % n, 255, 1 }
            });
            emitOpt(new int[][] {
                { i, 1, 1 }, { (i + 3) % n, 2, 1 }, { (i + 5) % n, 4, 1 }, { (i + 7) % n, 8, 0 }
            });
        }

        // A big stack of all palette entries at high amplifier (large accumulators).
        {
            int[][] big = new int[n][];
            for (int i = 0; i < n; i++) big[i] = new int[] { i, 255, 1 };
            emitOpt(big);
        }
        // Same but several copies, to push the accumulators well past a byte range.
        {
            int reps = 8;
            int[][] big = new int[n * reps][];
            int idx = 0;
            for (int r = 0; r < reps; r++)
                for (int i = 0; i < n; i++) big[idx++] = new int[] { i, 255, 1 };
            emitOpt(big);
        }

        // --- getColorOr / getColor: custom-color short-circuit + fallback. ---
        int[] customs = { 0, 1, -1, 16777215, -13083194, 0x7FFFFFFF, 0x80000000, 12345678, -999 };
        int[] defaults = { -13083194, 0, -1, 7, 0x00ABCDEF };

        // No effects + no custom -> default.
        for (int d : defaults) emitColorOr(false, 0, d, new int[0][]);
        // No effects + custom present -> custom (verbatim).
        for (int c : customs) emitColorOr(true, c, -13083194, new int[0][]);
        // Effects present + no custom -> mix.
        for (int i = 0; i < n; i++) {
            for (int d : defaults) {
                emitColorOr(false, 0, d, new int[][] { { i, 2, 1 }, { (i + 4) % n, 3, 1 } });
            }
        }
        // Effects present + custom present -> custom wins.
        for (int c : customs) {
            emitColorOr(true, c, 0, new int[][] { { 0, 1, 1 }, { 3, 2, 1 } });
        }
        // Invisible-only effects + no custom -> default.
        for (int d : defaults) {
            emitColorOr(false, 0, d, new int[][] { { 1, 5, 0 }, { 6, 5, 0 } });
        }

        // getColor() = getColorOr(BASE_POTION_COLOR).
        for (int c : customs) emitColor(true, c, new int[][] { { 2, 1, 1 } });
        emitColor(false, 0, new int[0][]);                                  // -> BASE
        for (int i = 0; i < n; i++) {
            emitColor(false, 0, new int[][] { { i, i % 4, 1 } });
        }
        emitColor(false, 0, new int[][] { { 1, 3, 0 }, { 4, 2, 0 } });      // invisible -> BASE
    }
}
