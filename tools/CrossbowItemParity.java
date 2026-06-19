// Reference value generator for the C++ CrossbowItem shot-helper port
// (mcpp/src/world/item/CrossbowItem.h). Runs the REAL decompiled
// net.minecraft.world.item.CrossbowItem from client.jar so the emitted pitches
// and shooting powers are exact ground truth. The body is NEVER replicated
// here: we invoke the real (private static) methods reflectively. The RNG-driven
// getShotPitch is fed a seeded LegacyRandomSource; the C++ port replays the same
// seed through its own LegacyRandomSource and must produce identical float bits.
//
//   javac -cp 26.1.2/client.jar;26.1.2/libs/* -d <out> mcpp/tools/CrossbowItemParity.java
//   java  -cp <out>;26.1.2/client.jar;26.1.2/libs/* CrossbowItemParity > crossbow_item.tsv
//
// Rows are tab-separated, leading TAG. Floats are emitted as
// Float.floatToRawIntBits (decimal int32) for byte-exact comparison.
//
//   SP     <containsFirework 0|1>          <powerBits>
//          CrossbowItem.getShootingPower(ChargedProjectiles): 1.6F if the
//          charged set contains a firework rocket, else 3.15F. Driven with a
//          real ChargedProjectiles built from a firework / an arrow stack.
//   PITCH  <seed> <index>                  <pitchBits>
//          CrossbowItem.getShotPitch(LegacyRandomSource(seed), index). index 0
//          is a pure 1.0F constant (no RNG draw); index != 0 routes through
//          getRandomShotPitch((index & 1) == 1, random), consuming one nextFloat.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.
@SuppressWarnings({"deprecation", "unchecked"})
public class CrossbowItemParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Build data component initializers so ItemStack components resolve
        // (needed to construct ChargedProjectiles from real stacks).
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());

        Class<?> cb = Class.forName("net.minecraft.world.item.CrossbowItem");

        // ---- getShootingPower(ChargedProjectiles) ----
        java.lang.reflect.Method mPower = null;
        for (java.lang.reflect.Method m : cb.getDeclaredMethods()) {
            if (m.getName().equals("getShootingPower") && m.getParameterCount() == 1) {
                m.setAccessible(true);
                mPower = m;
                break;
            }
        }

        net.minecraft.world.item.ItemStack fireworkStack =
            new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.FIREWORK_ROCKET);
        net.minecraft.world.item.ItemStack arrowStack =
            new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.ARROW);

        net.minecraft.world.item.component.ChargedProjectiles withFirework =
            net.minecraft.world.item.component.ChargedProjectiles.ofNonEmpty(
                java.util.List.of(fireworkStack));
        net.minecraft.world.item.component.ChargedProjectiles withArrow =
            net.minecraft.world.item.component.ChargedProjectiles.ofNonEmpty(
                java.util.List.of(arrowStack));

        float powF = (Float) mPower.invoke(null, withFirework);
        float powA = (Float) mPower.invoke(null, withArrow);
        O.println("SP\t1\t" + Float.floatToRawIntBits(powF));
        O.println("SP\t0\t" + Float.floatToRawIntBits(powA));

        // ---- getShotPitch(RandomSource, int) ----
        java.lang.reflect.Method mPitch = null;
        for (java.lang.reflect.Method m : cb.getDeclaredMethods()) {
            if (m.getName().equals("getShotPitch") && m.getParameterCount() == 2) {
                m.setAccessible(true);
                mPitch = m;
                break;
            }
        }

        // A spread of seeds (including negatives and edge longs) and indices that
        // exercise index 0 (constant), even indices -> low pitch (0.43F), and odd
        // indices -> high pitch (0.63F) via (index & 1).
        long[] seeds = {
            0L, 1L, 2L, 3L, 42L, 100L, 123456789L, -1L, -2L, -987654321L,
            2147483647L, -2147483648L, 1234567890123456789L,
            -1234567890123456789L, 8675309L, 555L, 99999999L, -42L
        };
        int[] indices = {0, 1, 2, 3, 4, 5, 6, 7, 8, 15, 16, 31, 100, 101};

        for (long seed : seeds) {
            for (int index : indices) {
                // Fresh seeded RNG per call so each row is reproducible from
                // (seed) alone, matching how the C++ test reconstructs it.
                net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                    new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                float pitch = (Float) mPitch.invoke(null, rng, index);
                O.println("PITCH\t" + seed + "\t" + index + "\t" + Float.floatToRawIntBits(pitch));
            }
        }
    }
}
