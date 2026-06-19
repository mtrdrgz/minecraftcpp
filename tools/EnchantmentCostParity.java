// Reference value generator for the C++ EnchantmentCost port
// (mcpp/src/world/item/enchantment/EnchantmentCost.h).
//
// Drives the REAL decompiled
//   net.minecraft.world.item.enchantment.EnchantmentHelper.getEnchantmentCost(
//       RandomSource random, int slot, int bookcases, ItemStack itemStack)
// from client.jar so the emitted level requirements are exact ground truth.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/EnchantmentCostParity.java
//   java  -cp <out>;26.1.2/client.jar EnchantmentCostParity > enchantment_cost.tsv
//
// Each row is tab-separated:
//   CST  <seed>  <slot>  <bookcases>  <hasEnchantable:0|1>  <result>
//
// `hasEnchantable` is recorded straight from the constructed ItemStack so the
// C++ side never has to model the DataComponents.ENCHANTABLE guard itself; it
// just receives the boolean. A fresh LegacyRandomSource(seed) is built per row
// so the RNG stream order (nextInt(8) then nextInt(bookcases+1)) is reproduced
// deterministically. When hasEnchantable==0 the method returns before touching
// the RNG, which the C++ port mirrors.
//
// O is captured at class load so any bootstrap chatter on stdout stays out of
// the TSV.
public class EnchantmentCostParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind each item's DEFAULT data components onto its built-in registry
        // Holder so `new ItemStack(item)` -> item.components() resolves (the
        // ENCHANTABLE component the guard reads lives here). Without this,
        // Holder$Reference.components() throws "Components not bound yet".
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());

        // Reflect the real static helper. Signature:
        //   (RandomSource, int, int, ItemStack) -> int
        Class<?> helper =
            Class.forName("net.minecraft.world.item.enchantment.EnchantmentHelper");
        java.lang.reflect.Method mCost = null;
        for (java.lang.reflect.Method m : helper.getDeclaredMethods()) {
            if (!m.getName().equals("getEnchantmentCost")) continue;
            Class<?>[] p = m.getParameterTypes();
            if (p.length == 4 && p[1] == int.class && p[2] == int.class) {
                m.setAccessible(true);
                mCost = m;
                break;
            }
        }
        if (mCost == null) throw new IllegalStateException("getEnchantmentCost not found");

        // An enchantable stack (DIAMOND_SWORD carries the ENCHANTABLE component
        // by default) and a non-enchantable one (STONE has no ENCHANTABLE).
        // Verify the guard's actual state via the component getter so the TSV's
        // hasEnchantable flag is ground truth, not an assumption.
        net.minecraft.world.item.ItemStack enchantable =
            new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.DIAMOND_SWORD);
        net.minecraft.world.item.ItemStack plain =
            new net.minecraft.world.item.ItemStack(net.minecraft.world.item.Items.STONE);

        int hasEnch = (enchantable.get(net.minecraft.core.component.DataComponents.ENCHANTABLE) != null) ? 1 : 0;
        int hasPlain = (plain.get(net.minecraft.core.component.DataComponents.ENCHANTABLE) != null) ? 1 : 0;
        if (hasEnch != 1) throw new IllegalStateException("DIAMOND_SWORD expected enchantable");
        if (hasPlain != 0) throw new IllegalStateException("STONE expected non-enchantable");

        // Seeds: zero, small, negatives, extremes, and assorted magic numbers so
        // both nextInt(8) and nextInt(bookcases+1) walk a representative range of
        // LegacyRandomSource states (incl. the power-of-two vs rejection paths of
        // nextInt(bound)).
        long[] seeds = {
            0L, 1L, 2L, 3L, 7L, 42L, 100L, 12345L, 123456789L,
            -1L, -2L, -42L, -987654321L,
            2147483647L, -2147483648L,
            8675309L, 1234567890123456789L, -1234567890123456789L,
            0x5DEECE66DL,            // the LCG multiplier, a classic edge seed
            0xCAFEBABEL, 0xDEADBEEFL
        };

        // Slots are 0,1,2 in production (three table rows). Probe a couple of
        // out-of-range slots too: slot>=2 falls into the same `else` branch as 2
        // (Math.max(selected, bookcases*2)), so e.g. slot 3 must equal slot 2.
        int[] slots = { 0, 1, 2, 3, -1 };

        // Bookcases: 0..17 covers the unclamped range, the clamp boundary (15),
        // and values above it (16,17) that must clamp to 15. Note bookcases also
        // bounds the second draw via nextInt(bookcases+1), so the clamp changes
        // BOTH `selected` and the RNG consumption for >15.
        int[] bookcaseValues = { 0, 1, 2, 3, 5, 7, 8, 10, 12, 14, 15, 16, 17 };

        // Enchantable battery: the full cross product.
        for (long seed : seeds) {
            for (int slot : slots) {
                for (int bookcases : bookcaseValues) {
                    net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                        new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                    int result = (Integer) mCost.invoke(null, rng, slot, bookcases, enchantable);
                    O.println("CST\t" + seed + "\t" + slot + "\t" + bookcases + "\t1\t" + result);
                }
            }
        }

        // Non-enchantable battery: must be a hard 0 with the RNG untouched. Use a
        // small representative subset of seeds/slots/bookcases — the guard makes
        // them all return 0 regardless, so coverage of the early-return is what
        // matters, not breadth.
        long[] plainSeeds = { 0L, 42L, -1L, 123456789L };
        for (long seed : plainSeeds) {
            for (int slot : slots) {
                for (int bookcases : new int[] { 0, 5, 15, 16 }) {
                    net.minecraft.world.level.levelgen.LegacyRandomSource rng =
                        new net.minecraft.world.level.levelgen.LegacyRandomSource(seed);
                    int result = (Integer) mCost.invoke(null, rng, slot, bookcases, plain);
                    O.println("CST\t" + seed + "\t" + slot + "\t" + bookcases + "\t0\t" + result);
                }
            }
        }
    }
}
