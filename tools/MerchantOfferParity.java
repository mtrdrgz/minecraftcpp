// Ground-truth generator for the PURE deterministic arithmetic of
// net.minecraft.world.item.trading.MerchantOffer (Minecraft Java 26.1.2).
//
// We drive a REAL MerchantOffer instance: it is constructed through its private
// all-args constructor (reflection) so every numeric field (demand, priceMultiplier,
// specialPriceDiff, uses, maxUses) is set exactly, and its baseCostA is a genuine
// ItemCost(item, count) built from real Items so getModifiedCostCount resolves
// `cost.itemStack().getMaxStackSize()` through the real data-component path.
//
// Methods exercised against the real object (NO body replicated Java-side):
//   * getModifiedCostCount(ItemCost)  — private; invoked reflectively. We emit the
//     raw inputs (basePrice=count, demand, priceMultiplierBits, specialPriceDiff,
//     maxStackSize) and the int result.
//   * updateDemand()                  — public; mutates `demand`; we emit pre-state
//     (demand, uses, maxUses) and the post-mutation getDemand().
//   * isOutOfStock() / needsRestock() — public; emit (uses, maxUses) and the bools.
//
// run_groundtruth.ps1 -Tool MerchantOfferParity -Out mcpp/build/merchant_offer.tsv
//
// TAGs (tab-separated):
//   COST   <basePrice> <demand> <priceMultiplierBits> <specialPriceDiff> <maxStackSize> <result>
//   DEMAND <demand> <uses> <maxUses> <newDemand>
//   STOCK  <uses> <maxUses> <isOutOfStock> <needsRestock>

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;
import java.util.Optional;

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.trading.ItemCost;
import net.minecraft.world.item.trading.MerchantOffer;

public class MerchantOfferParity {
    static final java.io.PrintStream O = System.out;

    static String fb(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

    // Reflected access to MerchantOffer's private all-args ctor + private fields/method.
    static Constructor<MerchantOffer> CTOR;
    static Method M_GET_MODIFIED;   // private int getModifiedCostCount(ItemCost)
    static java.lang.reflect.Field F_DEMAND;
    static java.lang.reflect.Field F_USES;

    @SuppressWarnings("unchecked")
    static void initReflection() throws Exception {
        Constructor<?> chosen = null;
        for (Constructor<?> c : MerchantOffer.class.getDeclaredConstructors()) {
            Class<?>[] pt = c.getParameterTypes();
            // private MerchantOffer(ItemCost, Optional, ItemStack, int, int, boolean, int, int, float, int)
            if (pt.length == 10 && pt[0] == ItemCost.class && pt[1] == Optional.class
                && pt[2] == ItemStack.class && pt[3] == int.class && pt[4] == int.class
                && pt[5] == boolean.class && pt[6] == int.class && pt[7] == int.class
                && pt[8] == float.class && pt[9] == int.class) {
                chosen = c;
                break;
            }
        }
        if (chosen == null) throw new IllegalStateException("all-args MerchantOffer ctor not found");
        CTOR = (Constructor<MerchantOffer>) chosen;
        CTOR.setAccessible(true);

        M_GET_MODIFIED = MerchantOffer.class.getDeclaredMethod("getModifiedCostCount", ItemCost.class);
        M_GET_MODIFIED.setAccessible(true);

        F_DEMAND = MerchantOffer.class.getDeclaredField("demand"); F_DEMAND.setAccessible(true);
        F_USES = MerchantOffer.class.getDeclaredField("uses"); F_USES.setAccessible(true);
    }

    // Build a real MerchantOffer with the given numeric state; baseCostA = ItemCost(item, basePrice).
    static MerchantOffer build(Item item, int basePrice, int uses, int maxUses,
                               int specialPriceDiff, int demand, float priceMultiplier, int xp) throws Exception {
        ItemCost baseCostA = new ItemCost(item, basePrice);
        ItemStack result = new ItemStack(Items.EMERALD, 1);  // arbitrary; unused by ported math
        boolean rewardExp = true;
        return CTOR.newInstance(baseCostA, Optional.<ItemCost>empty(), result,
                                uses, maxUses, rewardExp, specialPriceDiff, demand, priceMultiplier, xp);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind each item's DEFAULT data components so `new ItemStack(item, count)` and
        // ItemCost(item, count) resolve MAX_STACK_SIZE etc. (otherwise NPE in ctor).
        BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());

        initReflection();

        // ── COST: getModifiedCostCount sweep ────────────────────────────────────
        // Items spanning maxStackSize 64 / 16 / 1 so the upper clamp bound varies.
        Item[] ITEMS = { Items.STONE, Items.ENDER_PEARL, Items.SNOWBALL, Items.EGG, Items.DIAMOND_SWORD };
        // basePrice = ItemCost.count(); vanilla trades use 1..64. Include 0 and large
        // to exercise the lower clamp (->1) and the int*int*float path.
        int[] BASE = { 0, 1, 2, 3, 4, 8, 16, 32, 64, 100, 1000, 100000 };
        // demand: negative (Math.max(0,...) floor -> 0), zero, and positive ramps.
        int[] DEMAND = { -100, -10, -1, 0, 1, 2, 3, 5, 10, 25, 50, 100, 1000, 1000000 };
        // priceMultiplier: 0 (no demand effect), the vanilla 0.05/0.2, and edges.
        float[] PMUL = { 0.0f, 0.05f, 0.1f, 0.2f, 0.5f, 1.0f, 2.0f, -0.05f, -1.0f, 0.30000001f };
        // specialPriceDiff: discounts (negative) and surcharges, incl. magnitudes that
        // drive the sum below 1 (lower clamp) or above maxStackSize (upper clamp).
        int[] SPECIAL = { -1000000, -100, -30, -7, -1, 0, 1, 7, 30, 100, 1000000 };

        for (Item item : ITEMS) {
            for (int basePrice : BASE) {
                for (int demand : DEMAND) {
                    for (float pmul : PMUL) {
                        for (int special : SPECIAL) {
                            MerchantOffer offer = build(item, basePrice, 0, 4, special, demand, pmul, 1);
                            ItemCost baseCostA = offer.getItemCostA();
                            int maxStackSize = baseCostA.itemStack().getMaxStackSize();
                            int actualBasePrice = baseCostA.count();
                            int result = (Integer) M_GET_MODIFIED.invoke(offer, baseCostA);
                            O.println("COST\t" + actualBasePrice + "\t" + demand + "\t" + fb(pmul)
                                      + "\t" + special + "\t" + maxStackSize + "\t" + result);
                        }
                    }
                }
            }
        }

        // ── DEMAND: updateDemand() = demand + uses - (maxUses - uses) ────────────
        int[] D_DEMAND = { -2147483648, -1000000, -100, -1, 0, 1, 100, 1000000, 2147483647 };
        int[] D_USES   = { 0, 1, 2, 3, 4, 7, 16, 1000000 };
        int[] D_MAXUSES= { 0, 1, 2, 4, 7, 12, 16, 1000000 };
        for (int demand : D_DEMAND) {
            for (int uses : D_USES) {
                for (int maxUses : D_MAXUSES) {
                    MerchantOffer offer = build(Items.STONE, 1, uses, maxUses, 0, demand, 0.0f, 1);
                    offer.updateDemand();              // mutates the real `demand` field
                    int newDemand = offer.getDemand(); // read back the post-mutation value
                    O.println("DEMAND\t" + demand + "\t" + uses + "\t" + maxUses + "\t" + newDemand);
                }
            }
        }

        // ── STOCK: isOutOfStock() = uses>=maxUses ; needsRestock() = uses>0 ──────
        int[] S_USES    = { -1, 0, 1, 2, 3, 4, 5, 12, 1000000 };
        int[] S_MAXUSES = { 0, 1, 2, 4, 5, 12, 16, 1000000 };
        for (int uses : S_USES) {
            for (int maxUses : S_MAXUSES) {
                MerchantOffer offer = build(Items.STONE, 1, uses, maxUses, 0, 0, 0.0f, 1);
                boolean oos = offer.isOutOfStock();
                boolean restock = offer.needsRestock();
                O.println("STOCK\t" + uses + "\t" + maxUses + "\t" + (oos ? 1 : 0) + "\t" + (restock ? 1 : 0));
            }
        }
    }
}
