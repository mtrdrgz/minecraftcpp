// Ground truth for the durability-bar display math on net.minecraft.world.item.Item:
//
//   Item.isBarVisible(stack) = stack.isDamaged()
//                            = isDamageableItem() && getDamageValue() > 0           (Item.java:218-220)
//   Item.getBarWidth(stack)  = Mth.clamp(Math.round(13.0F - dmg*13.0F/maxDamage), 0, 13)  (Item.java:222-224)
//   Item.getBarColor(stack)  = Mth.hsvToRgb(Math.max(0.0F,((float)maxDamage-dmg)/maxDamage)/3.0F,1,1) (Item.java:226-230)
//   where dmg = ItemStack.getDamageValue() = Mth.clamp(stored DAMAGE, 0, getMaxDamage())  (ItemStack.java:425-427)
//
// We drive the REAL classes: build a real ItemStack for every damageable vanilla item
// (each item has its own maxDamage), set the raw DAMAGE component to a sweep of stored
// values (including > maxDamage and negative to exercise the clamp), then read the REAL
// stack.getBarWidth()/getBarColor()/isBarVisible(). BundleItem overrides these three
// methods (its own formula), so it is excluded — the C++ gate ports the base Item math.
//
//   tools/run_groundtruth.ps1 -Tool ItemBarDisplayParity -Out mcpp/build/item_bar_display.tsv
//
// Row: BAR \t <storedDamage int> \t <maxDamage int> \t <barWidth int> \t <barColor int> \t <barVisible 0/1>

import net.minecraft.core.component.DataComponents;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.item.BundleItem;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class ItemBarDisplayParity {
    static final java.io.PrintStream O = System.out;

    static void emit(int storedDamage, int maxDamage, int barWidth, int barColor, boolean visible) {
        O.println("BAR\t" + storedDamage + "\t" + maxDamage + "\t" + barWidth + "\t" + barColor
                + "\t" + (visible ? 1 : 0));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind item data components (registry holders start unbound) — required before
        // ItemStack.getMaxDamage()/isDamageableItem()/the DAMAGE component reads.
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());

        // Sweep of raw stored DAMAGE values to exercise the clamp [0,maxDamage] and the
        // 14 distinct bar widths. Fractions are expressed relative to each item's maxDamage.
        // Includes negative + over-max values so the ItemStack clamp is verified end to end.
        for (Item item : BuiltInRegistries.ITEM) {
            if (item instanceof BundleItem) continue;          // overrides the three bar methods
            ItemStack stack = new ItemStack(item);
            if (!stack.isDamageableItem()) continue;            // needs MAX_DAMAGE + DAMAGE, no UNBREAKABLE
            int maxDamage = stack.getMaxDamage();
            if (maxDamage <= 0) continue;

            java.util.LinkedHashSet<Integer> stored = new java.util.LinkedHashSet<>();
            stored.add(-5);
            stored.add(-1);
            for (int num = 0; num <= 16; num++) {
                // 0/16 .. 16/16 of maxDamage, integer-truncated (matches the in-game spread).
                stored.add((int) ((long) maxDamage * num / 16));
            }
            stored.add(maxDamage - 1);
            stored.add(maxDamage);
            stored.add(maxDamage + 1);
            stored.add(maxDamage + 7);

            for (int sd : stored) {
                // Set the raw DAMAGE component directly (NOT setDamageValue, which would
                // pre-clamp) so the ItemStack.getDamageValue() clamp is part of what we test.
                stack.set(DataComponents.DAMAGE, sd);
                int barWidth = stack.getBarWidth();
                int barColor = stack.getBarColor();
                boolean visible = stack.isBarVisible();
                emit(sd, maxDamage, barWidth, barColor, visible);
            }
        }
        O.flush();
    }
}
