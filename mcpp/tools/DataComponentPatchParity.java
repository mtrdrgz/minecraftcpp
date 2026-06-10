// Ground truth for DataComponentPatch on the wire (the components an ItemStack carries),
// for the simplest high-frequency component family: the VAR_INT integer components
// (damage, max_damage, max_stack_size, repair_cost). This certifies (1) the patch framing
// (DataComponentPatch.java:106-141): VarInt(positiveCount) VarInt(negativeCount), then each
// ADDED = DataComponentType.STREAM_CODEC.encode(type) [registry(DATA_COMPONENT_TYPE) = plain
// VarInt(getId)] + the component value codec; and (2) the VAR_INT value codec for these ints.
//
// Each case builds a real ItemStack with exactly ONE int component set, encodes it through
// ItemStack.OPTIONAL_STREAM_CODEC, and dumps the bytes. The C++ gate resolves both the item
// id and the component-type id via mc::net::NetworkRegistries and reproduces the bytes.
//
//   tools/run_groundtruth.ps1 -Tool DataComponentPatchParity -Out mcpp/build/dcp_int.tsv
//
// Row: ENC \t <itemName> \t <count> \t <componentName ns:path> \t <intValue> \t <readableBytes> \t <wireHex>

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.component.DataComponentType;
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class DataComponentPatchParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static void emit(RegistryAccess access, String compName, DataComponentType<Integer> type, int value) {
        // diamond_sword (id stable, a real tool) carries no int components by default, so a
        // single set() yields a patch with exactly that one added component.
        Item item = net.minecraft.world.item.Items.DIAMOND_SWORD;
        // Skip cases where the value equals the item's prototype default — those produce an
        // EMPTY patch (no diff), which the C++ gate can't predict without the prototype. By
        // emitting only non-default values, every row is a single-added-component patch
        // (positiveCount=1, negativeCount=0).
        Integer def = new ItemStack(item, 1).get(type);
        if (java.util.Objects.equals(def, value)) return;
        ItemStack stack = new ItemStack(item, 1);
        stack.set(type, value);
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ItemStack.OPTIONAL_STREAM_CODEC.encode(buf, stack);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        ItemStack back = ItemStack.OPTIONAL_STREAM_CODEC.decode(buf);
        Integer rv = back.get(type);
        if (rv == null || rv != value) throw new IllegalStateException("round-trip mismatch " + compName + " " + value);
        String itemName = BuiltInRegistries.ITEM.getKey(item).toString();
        O.println("ENC\t" + itemName + "\t1\t" + compName + "\t" + value + "\t" + readable + "\t" + hex(bytes));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // The VAR_INT integer components. Values cross the VarInt 1->2->3 byte boundaries.
        int[] vals = { 0, 1, 5, 16, 127, 128, 255, 256, 16383, 16384, 100000 };
        Object[][] comps = {
            { "minecraft:damage", DataComponents.DAMAGE },
            { "minecraft:max_damage", DataComponents.MAX_DAMAGE },
            { "minecraft:max_stack_size", DataComponents.MAX_STACK_SIZE },
            { "minecraft:repair_cost", DataComponents.REPAIR_COST },
        };
        for (Object[] c : comps) {
            String name = (String) c[0];
            @SuppressWarnings("unchecked")
            DataComponentType<Integer> type = (DataComponentType<Integer>) c[1];
            for (int v : vals) {
                // max_stack_size must be in [1,99]; skip out-of-range for that one.
                if (name.equals("minecraft:max_stack_size") && (v < 1 || v > 99)) continue;
                emit(access, name, type, v);
            }
        }
        O.flush();
    }
}
