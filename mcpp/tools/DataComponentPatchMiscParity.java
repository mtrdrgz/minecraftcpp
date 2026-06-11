// Ground truth for DataComponentPatch over three more component-value families:
//   * Component-valued: custom_name, item_name (ComponentSerialization.STREAM_CODEC -> the
//     plain-text Component NBT root StringTag, already certified by component_nbt_parity).
//   * enum-valued: rarity (Rarity.STREAM_CODEC = ByteBufCodecs.idMapper -> VarInt(rarity.id),
//     id == ordinal: COMMON 0..EPIC 3).
//   * Unit-valued: unbreakable (Unit.STREAM_CODEC = StreamCodec.unit -> ZERO value bytes).
// Each case is a single-added-component (non-default) patch on a base item.
//
//   tools/run_groundtruth.ps1 -Tool DataComponentPatchMiscParity -Out mcpp/build/dcp_misc.tsv
//
// Row: ENC \t <itemName> \t <componentName> \t <valueKind: component|enumId|unit> \t
//      <valueData: textUtf8Hex | enumId | -> \t <readableBytes> \t <wireHex>

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.component.DataComponents;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;
import net.minecraft.world.item.Items;
import net.minecraft.world.item.Rarity;

public class DataComponentPatchMiscParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static void emit(RegistryAccess access, ItemStack stack, String compName,
                     String valueKind, String valueData) {
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ItemStack.OPTIONAL_STREAM_CODEC.encode(buf, stack);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        ItemStack.OPTIONAL_STREAM_CODEC.decode(buf);  // round-trip sanity
        String itemName = BuiltInRegistries.ITEM.getKey(stack.getItem()).toString();
        O.println("ENC\t" + itemName + "\t" + compName + "\t" + valueKind + "\t" + valueData
                + "\t" + readable + "\t" + hex(bytes));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);
        Item base = Items.DIAMOND_SWORD;

        // Component-valued: custom_name + item_name with plain-text literals.
        String[] texts = { "", "Excalibur", "niño", "中文", "😀" };  // incl emoji
        for (String t : texts) {
            ItemStack s1 = new ItemStack(base, 1);
            s1.set(DataComponents.CUSTOM_NAME, Component.literal(t));
            emit(access, s1, "minecraft:custom_name", "component",
                    hex(t.getBytes(java.nio.charset.StandardCharsets.UTF_8)));
            ItemStack s2 = new ItemStack(base, 1);
            s2.set(DataComponents.ITEM_NAME, Component.literal(t));
            emit(access, s2, "minecraft:item_name", "component",
                    hex(t.getBytes(java.nio.charset.StandardCharsets.UTF_8)));
        }

        // enum-valued: rarity (diamond_sword default is COMMON=0, so UNCOMMON/RARE/EPIC differ).
        for (Rarity r : new Rarity[]{ Rarity.UNCOMMON, Rarity.RARE, Rarity.EPIC }) {
            ItemStack s = new ItemStack(base, 1);
            s.set(DataComponents.RARITY, r);
            emit(access, s, "minecraft:rarity", "enumId", Integer.toString(r.ordinal()));
        }

        // Unit-valued: unbreakable (no value bytes).
        ItemStack u = new ItemStack(base, 1);
        u.set(DataComponents.UNBREAKABLE, net.minecraft.util.Unit.INSTANCE);
        emit(access, u, "minecraft:unbreakable", "unit", "-");

        // Identifier-valued: item_model (Identifier.STREAM_CODEC = STRING_UTF8 -> writeUtf(toString)).
        // NOTE: an item's DEFAULT item_model is its own id, so a same-id value collapses to an EMPTY
        // patch (DataComponentPatch encodes only differences) -> use ids != base's default.
        String[] ids = { "minecraft:stone", "mymod:custom/model_x", "minecraft:trident", "minecraft:bedrock" };
        for (String id : ids) {
            net.minecraft.resources.Identifier rl = net.minecraft.resources.Identifier.parse(id);
            ItemStack s = new ItemStack(base, 1);
            s.set(DataComponents.ITEM_MODEL, rl);
            emit(access, s, "minecraft:item_model", "identifier",
                    hex(rl.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8)));
        }

        // Boolean-valued: enchantment_glint_override (ByteBufCodecs.BOOL -> writeBool).
        for (boolean g : new boolean[]{ true, false }) {
            ItemStack s = new ItemStack(base, 1);
            s.set(DataComponents.ENCHANTMENT_GLINT_OVERRIDE, g);
            emit(access, s, "minecraft:enchantment_glint_override", "bool", g ? "1" : "0");
        }

        // Unit-valued (network-synchronized): glider (Unit.STREAM_CODEC -> zero value bytes).
        ItemStack gl = new ItemStack(base, 1);
        gl.set(DataComponents.GLIDER, net.minecraft.util.Unit.INSTANCE);
        emit(access, gl, "minecraft:glider", "unit", "-");

        O.flush();
    }
}
