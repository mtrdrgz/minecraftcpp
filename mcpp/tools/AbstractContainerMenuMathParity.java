// Ground-truth generator for the PURE static slot/redstone math of
// net.minecraft.world.inventory.AbstractContainerMenu (26.1.2).
//
// We drive the REAL public statics on AbstractContainerMenu directly (no reflection
// needed — they are public), and for getRedstoneSignalFromContainer / getQuickCraftPlaceCount
// we build genuine net.minecraft.world.SimpleContainer + net.minecraft.world.item.ItemStack
// instances so the IEEE-754 float math runs through the real code path.
//
// The mc::world::inventory::AbstractContainerMenuMath C++ port must reproduce every
// emitted value bit-for-bit.
//
//   tools/run_groundtruth.ps1 -Tool AbstractContainerMenuMathParity -Out mcpp/build/abstract_container_math.tsv
//
// TAGs emitted (all result fields are decimal int32 unless noted):
//   QTYPE   <mask>                              <result>        getQuickcraftType(mask)
//   QHEAD   <mask>                              <result>        getQuickcraftHeader(mask)
//   QMASK   <header> <type>                     <result>        getQuickcraftMask(header,type)
//   QPLACE  <size> <type> <count> <maxStack>    <result>        getQuickCraftPlaceCount(size,type,stack)
//   REDST   <containerSize> <nSlots> [slot:count:effMax]...  <result>
//                                                              getRedstoneSignalFromContainer(container)
//
// For REDST the trailing tokens describe the populated slots used to BUILD the real
// SimpleContainer; the C++ test rebuilds the same (count, effectiveMax) per-slot vector.
// effMax = min(container.getMaxStackSize()==99, itemStack.getMaxStackSize()).

import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.world.SimpleContainer;
import net.minecraft.world.inventory.AbstractContainerMenu;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;

public class AbstractContainerMenuMathParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind each item's DEFAULT data components onto its built-in registry Holder so
        // `new ItemStack(item, count)` -> item.components() resolves (MAX_STACK_SIZE etc.).
        // Without this, Holder$Reference.components() throws NPE during ItemStack.<init>.
        BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());

        // ── 1) getQuickcraftType(mask) = mask >> 2 & 3  (arithmetic shift) ──────
        // Probe small masks, the full 0..63 nibble range, and negatives so the
        // sign-propagating >> is exercised.
        int[] MASKS = {
            0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
            16, 20, 24, 28, 31, 32, 47, 60, 63, 64, 100, 255, 256,
            -1, -2, -3, -4, -5, -8, -16, -32, -64,
            2147483647, -2147483648, 1431655765, -1431655766, -559038737
        };
        for (int m : MASKS) {
            O.println("QTYPE\t" + m + "\t" + AbstractContainerMenu.getQuickcraftType(m));
            O.println("QHEAD\t" + m + "\t" + AbstractContainerMenu.getQuickcraftHeader(m));
        }

        // ── 2) getQuickcraftMask(header, type) = (header & 3) | ((type & 3) << 2) ─
        // Sweep all header/type in -2..5 to cover the &3 masking on both signs, plus
        // the canonical 0..3 round-trips that the menu actually uses.
        for (int h = -2; h <= 5; h++) {
            for (int t = -2; t <= 5; t++) {
                O.println("QMASK\t" + h + "\t" + t + "\t" + AbstractContainerMenu.getQuickcraftMask(h, t));
            }
        }
        // round-trip sanity (header/type recovered from packed mask) — included as data rows
        for (int h = 0; h <= 3; h++) {
            for (int t = 0; t <= 3; t++) {
                int packed = AbstractContainerMenu.getQuickcraftMask(h, t);
                O.println("QMASK\t" + h + "\t" + t + "\t" + packed);
            }
        }

        // ── 3) getQuickCraftPlaceCount(size, type, stack) ──────────────────────
        // case 0 = Mth.floor((float)count / size) is the truncation trap; cases 1/2/3
        // return 1 / maxStackSize / count. Use real ItemStacks of varying max sizes so
        // getMaxStackSize() / getCount() come from the genuine component path.
        // (item, count) pairs spanning maxStack 1 / 16 / 64.
        Object[][] STACKS = {
            { Items.STONE, 64 }, { Items.STONE, 7 }, { Items.STONE, 1 }, { Items.STONE, 3 },
            { Items.STONE, 5 }, { Items.STONE, 10 }, { Items.STONE, 63 }, { Items.STONE, 50 },
            { Items.ENDER_PEARL, 16 }, { Items.ENDER_PEARL, 5 }, { Items.ENDER_PEARL, 1 },
            { Items.EGG, 9 }, { Items.SNOWBALL, 13 },
            { Items.DIAMOND_SWORD, 1 },
        };
        int[] SIZES = { 1, 2, 3, 4, 5, 6, 7, 8, 9 };
        int[] TYPES = { 0, 1, 2, 3, 4 };  // 4 hits the `default` arm
        for (Object[] sp : STACKS) {
            Item item = (Item) sp[0];
            int count = (Integer) sp[1];
            ItemStack stack = new ItemStack(item, count);
            int maxStack = stack.getMaxStackSize();
            int realCount = stack.getCount();
            for (int size : SIZES) {
                for (int type : TYPES) {
                    int r = AbstractContainerMenu.getQuickCraftPlaceCount(size, type, stack);
                    O.println("QPLACE\t" + size + "\t" + type + "\t" + realCount + "\t" + maxStack + "\t" + r);
                }
            }
        }

        // ── 4) getRedstoneSignalFromContainer(container) ───────────────────────
        // Build real SimpleContainers of various sizes, populated with real stacks.
        // Each scenario is described by (containerSize, [(slot, item, count)...]).
        // We emit the resolved per-slot (count, effectiveMax) so the C++ side rebuilds
        // the identical float accumulation. effectiveMax = min(99, item.maxStackSize).
        emitRedstone(1, new int[][]{});                                    // empty size-1
        emitRedstone(27, new int[][]{});                                   // empty chest
        emitRedstone(1, new int[][]{ {0, S, 64} });                        // full single 64-stack
        emitRedstone(1, new int[][]{ {0, S, 32} });                        // half single 64-stack
        emitRedstone(1, new int[][]{ {0, S, 1} });                         // one item
        emitRedstone(1, new int[][]{ {0, E, 16} });                        // full 16-stack
        emitRedstone(1, new int[][]{ {0, D, 1} });                         // full 1-stack (sword)
        emitRedstone(5, new int[][]{ {0, S, 64}, {1, S, 64}, {2, S, 64}, {3, S, 64}, {4, S, 64} });
        emitRedstone(5, new int[][]{ {0, S, 64} });                        // 1 of 5 full
        emitRedstone(9, new int[][]{ {0, S, 1}, {4, S, 1}, {8, S, 1} });   // sparse singles
        emitRedstone(27, new int[][]{ {0, S, 64}, {13, E, 16}, {26, D, 1} });
        emitRedstone(3, new int[][]{ {0, S, 21}, {1, S, 22}, {2, S, 23} }); // mixed -> float fractions
        emitRedstone(7, new int[][]{ {0, E, 7}, {3, S, 33}, {6, D, 1} });
        emitRedstone(64, new int[][]{ {0, S, 1} });                        // 1 in a big container
        emitRedstone(2, new int[][]{ {0, S, 64}, {1, S, 64} });
        emitRedstone(2, new int[][]{ {0, S, 1}, {1, S, 1} });
        // every fill level 0..64 of a single-slot 64-stack: walks lerpDiscrete buckets 0..15
        for (int c = 1; c <= 64; c++) {
            emitRedstone(1, new int[][]{ {0, S, c} });
        }
        // every fill level of a single-slot 16-stack
        for (int c = 1; c <= 16; c++) {
            emitRedstone(1, new int[][]{ {0, E, c} });
        }
    }

    // item selectors for the compact REDST scenario tables
    static final int S = 0;  // STONE       maxStack 64
    static final int E = 1;  // ENDER_PEARL maxStack 16
    static final int D = 2;  // DIAMOND_SWORD maxStack 1

    static Item pick(int sel) {
        switch (sel) {
            case S: return Items.STONE;
            case E: return Items.ENDER_PEARL;
            case D: return Items.DIAMOND_SWORD;
            default: throw new IllegalArgumentException("sel " + sel);
        }
    }

    static void emitRedstone(int size, int[][] fills) {
        SimpleContainer container = new SimpleContainer(size);
        // resolved per-slot (count, effectiveMax) for ALL slots (0 count = empty)
        int[] counts = new int[size];
        int[] effMax = new int[size];
        for (int[] f : fills) {
            int slot = f[0];
            Item item = pick(f[1]);
            int count = f[2];
            ItemStack stack = new ItemStack(item, count);
            container.setItem(slot, stack);
        }
        // read back through the REAL container so effectiveMax = container.getMaxStackSize(stack)
        for (int i = 0; i < size; i++) {
            ItemStack st = container.getItem(i);
            if (st.isEmpty()) {
                counts[i] = 0;
                effMax[i] = 0;
            } else {
                counts[i] = st.getCount();
                effMax[i] = container.getMaxStackSize(st);
            }
        }
        int result = AbstractContainerMenu.getRedstoneSignalFromContainer(container);

        StringBuilder sb = new StringBuilder();
        sb.append("REDST\t").append(size).append('\t').append(size);
        for (int i = 0; i < size; i++) {
            sb.append('\t').append(i).append(':').append(counts[i]).append(':').append(effMax[i]);
        }
        sb.append('\t').append(result);
        O.println(sb.toString());
    }
}
