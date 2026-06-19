// Ground truth for net.minecraft.world.entity.Crackiness (Minecraft 26.1.2).
//
// Exercises the REAL class:
//   - byFraction(float)               over a battery of physical fractions
//   - byDamage(int damage, int maxDamage)
// on both vanilla preset instances GOLEM and WOLF_ARMOR.
//
// byFraction is public; byDamage(int,int) is public. We resolve them via
// reflection (setAccessible) on the public static GOLEM/WOLF_ARMOR fields so
// no protected method is called directly. The returned Crackiness.Level enum
// is emitted as its ordinal (NONE=0, LOW=1, MEDIUM=2, HIGH=3).
//
// SKIPPED: byDamage(ItemStack) — needs a live ItemStack; not represented here.
//
// Floats are emitted as 8-hex raw int bits (Float.floatToRawIntBits) so the C++
// side feeds byte-identical inputs into the comparison; the Level result is a
// decimal ordinal.
//
// Row tags (tab-separated):
//   FRAC   <preset> <fraction8>            <levelOrdinal>
//   DAMG   <preset> <damage> <maxDamage>   <levelOrdinal>
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.world.entity.Crackiness;

public class CrackinessParity {
    static final java.io.PrintStream O = System.out;

    static String fb(float f) { return String.format("%08x", Float.floatToRawIntBits(f)); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Public static preset instances.
        Field fGolem = Crackiness.class.getField("GOLEM");
        Field fWolf = Crackiness.class.getField("WOLF_ARMOR");
        fGolem.setAccessible(true);
        fWolf.setAccessible(true);
        Object golem = fGolem.get(null);
        Object wolf = fWolf.get(null);

        Method byFraction = Crackiness.class.getMethod("byFraction", float.class);
        byFraction.setAccessible(true);
        Method byDamage = Crackiness.class.getMethod("byDamage", int.class, int.class);
        byDamage.setAccessible(true);

        Object[][] presets = {
            {"GOLEM", golem},
            {"WOLF_ARMOR", wolf},
        };

        // ── byFraction battery ──────────────────────────────────────────────
        // Physical, finite fractions: the [0,1] durability range plus exact
        // threshold values for GOLEM (0.25/0.5/0.75) and WOLF_ARMOR
        // (0.32/0.69/0.95), values just above/below them (nextUp/nextDown), and
        // a few in-between and out-of-[0,1] (but finite, non-negative-where-sane
        // and a couple negatives that are reachable from damage math) points.
        float[] fractions = {
            0.0f, 1.0f,
            0.25f, 0.5f, 0.75f,
            0.32f, 0.69f, 0.95f,
            0.1f, 0.2f, 0.3f, 0.4f, 0.6f, 0.7f, 0.8f, 0.9f,
            0.24f, 0.26f, 0.49f, 0.51f, 0.74f, 0.76f,
            0.31f, 0.33f, 0.68f, 0.70f, 0.94f, 0.96f,
            Math.nextUp(0.25f), Math.nextDown(0.25f),
            Math.nextUp(0.5f), Math.nextDown(0.5f),
            Math.nextUp(0.75f), Math.nextDown(0.75f),
            Math.nextUp(0.32f), Math.nextDown(0.32f),
            Math.nextUp(0.69f), Math.nextDown(0.69f),
            Math.nextUp(0.95f), Math.nextDown(0.95f),
            0.001f, 0.999f, 0.5000001f, 0.4999999f,
            1.5f, 2.0f, -0.5f, -1.0f,
            0.123456f, 0.654321f, 0.333333f, 0.666666f,
        };

        for (Object[] p : presets) {
            String name = (String) p[0];
            Object inst = p[1];
            for (float fr : fractions) {
                Object lvl = byFraction.invoke(inst, fr);
                int ord = ((Enum<?>) lvl).ordinal();
                O.println("FRAC\t" + name + "\t" + fb(fr) + "\t" + ord);
            }
        }

        // ── byDamage(damage, maxDamage) battery ─────────────────────────────
        // Real item maxDamage values; damage swept across [0, maxDamage] plus a
        // couple of boundary points. All physical (maxDamage > 0, damage >= 0).
        int[] maxDamages = {1, 2, 7, 32, 59, 100, 200, 250, 384, 465, 1561, 2031};
        for (Object[] p : presets) {
            String name = (String) p[0];
            Object inst = p[1];
            for (int max : maxDamages) {
                // Sweep damage densely for small max, sampled for large.
                int step = Math.max(1, max / 23);
                for (int dmg = 0; dmg <= max; dmg += step) {
                    Object lvl = byDamage.invoke(inst, dmg, max);
                    int ord = ((Enum<?>) lvl).ordinal();
                    O.println("DAMG\t" + name + "\t" + dmg + "\t" + max + "\t" + ord);
                }
                // Ensure the exact endpoints are present.
                for (int dmg : new int[]{0, max, max - 1 < 0 ? 0 : max - 1}) {
                    Object lvl = byDamage.invoke(inst, dmg, max);
                    int ord = ((Enum<?>) lvl).ordinal();
                    O.println("DAMG\t" + name + "\t" + dmg + "\t" + max + "\t" + ord);
                }
            }
        }
    }
}
