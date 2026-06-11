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
import net.minecraft.world.entity.EquipmentSlotGroup;
import net.minecraft.world.entity.ai.attributes.AttributeModifier;
import net.minecraft.world.entity.ai.attributes.Attributes;
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

    static void emitCustomData(RegistryAccess access, Item base, net.minecraft.nbt.CompoundTag tag,
                               String type, String name, String valuePart) {
        ItemStack s = new ItemStack(base, 1);
        s.set(DataComponents.CUSTOM_DATA, net.minecraft.world.item.component.CustomData.of(tag));
        String nameHex = hex(name.getBytes(java.nio.charset.StandardCharsets.UTF_8));
        emit(access, s, "minecraft:custom_data", "customdata", type + ":" + nameHex + ":" + valuePart);
    }

    @SuppressWarnings("unchecked")
    static void emitAttrMods(RegistryAccess access, Object[][] specs) {
        net.minecraft.world.item.component.ItemAttributeModifiers mods =
                net.minecraft.world.item.component.ItemAttributeModifiers.EMPTY;
        StringBuilder vd = new StringBuilder(Integer.toString(specs.length));
        for (Object[] sp : specs) {
            net.minecraft.core.Holder<net.minecraft.world.entity.ai.attributes.Attribute> attr =
                    (net.minecraft.core.Holder<net.minecraft.world.entity.ai.attributes.Attribute>) sp[0];
            String modId = (String) sp[1];
            double amount = (Double) sp[2];
            AttributeModifier.Operation op = (AttributeModifier.Operation) sp[3];
            EquipmentSlotGroup slot = (EquipmentSlotGroup) sp[4];
            mods = mods.withModifierAdded(attr,
                    new AttributeModifier(net.minecraft.resources.Identifier.parse(modId), amount, op), slot);
            String attrName = BuiltInRegistries.ATTRIBUTE.getKey(attr.value()).toString();
            vd.append("|").append(attrName)
              .append(",").append(hex(net.minecraft.resources.Identifier.parse(modId).toString().getBytes(java.nio.charset.StandardCharsets.UTF_8)))
              .append(",").append(String.format("%016x", Double.doubleToRawLongBits(amount)))
              .append(",").append(op.ordinal()).append(",").append(slot.ordinal());
        }
        ItemStack s = new ItemStack(Items.STICK, 1);
        s.set(DataComponents.ATTRIBUTE_MODIFIERS, mods);
        emit(access, s, "minecraft:attribute_modifiers", "attrmods", vd.toString());
    }

    static void emitCMD(RegistryAccess access, Item base, java.util.List<Float> floats,
                        java.util.List<Boolean> flags, java.util.List<String> strings, java.util.List<Integer> colors) {
        ItemStack s = new ItemStack(base, 1);
        s.set(DataComponents.CUSTOM_MODEL_DATA,
                new net.minecraft.world.item.component.CustomModelData(floats, flags, strings, colors));
        StringBuilder vd = new StringBuilder().append(floats.size());
        for (float fv : floats) vd.append(":").append(String.format("%08x", Float.floatToRawIntBits(fv)));
        vd.append(";").append(flags.size());
        for (boolean b : flags) vd.append(":").append(b ? "1" : "0");
        vd.append(";").append(strings.size());
        for (String t : strings) vd.append(":").append(hex(t.getBytes(java.nio.charset.StandardCharsets.UTF_8)));
        vd.append(";").append(colors.size());
        for (int c : colors) vd.append(":").append(c);
        emit(access, s, "minecraft:custom_model_data", "custommodeldata", vd.toString());
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

        // List<Component>-valued: lore (ItemLore.STREAM_CODEC = list(Component plain-text)).
        // Non-empty so it doesn't collapse to the empty-lore default. valueData = count:hex:hex...
        String[][] lores = { {"Legendary"}, {"Line one", "Line two"}, {"中文", "😀 emoji line"} };
        for (String[] lore : lores) {
            java.util.List<Component> lines = new java.util.ArrayList<>();
            for (String tt : lore) lines.add(Component.literal(tt));
            ItemStack s = new ItemStack(base, 1);
            s.set(DataComponents.LORE, new net.minecraft.world.item.component.ItemLore(lines));
            StringBuilder vd = new StringBuilder(Integer.toString(lore.length));
            for (String tt : lore) vd.append(":").append(hex(tt.getBytes(java.nio.charset.StandardCharsets.UTF_8)));
            emit(access, s, "minecraft:lore", "lore", vd.toString());
        }

        // NBT-compound-valued: custom_data (CustomData.STREAM_CODEC = ByteBufCodecs.COMPOUND_TAG ->
        // writeNbt(compound)). SINGLE-entry compounds only (multi-entry wire order depends on Java
        // CompoundTag's HashMap iteration order). valueData = "<type>:<nameHex>:<valuePart>".
        {
            net.minecraft.nbt.CompoundTag cs = new net.minecraft.nbt.CompoundTag();
            cs.putString("id", "minecraft:foo");
            emitCustomData(access, base, cs, "s", "id",
                    hex("minecraft:foo".getBytes(java.nio.charset.StandardCharsets.UTF_8)));
            net.minecraft.nbt.CompoundTag ci = new net.minecraft.nbt.CompoundTag();
            ci.putInt("count", 42);
            emitCustomData(access, base, ci, "i", "count", "42");
            net.minecraft.nbt.CompoundTag cb = new net.minecraft.nbt.CompoundTag();
            cb.putByte("flag", (byte) 1);
            emitCustomData(access, base, cb, "b", "flag", "1");
            net.minecraft.nbt.CompoundTag cl = new net.minecraft.nbt.CompoundTag();
            cl.putLong("seed", 123456789012345L);
            emitCustomData(access, base, cl, "l", "seed", "123456789012345");
            net.minecraft.nbt.CompoundTag cf = new net.minecraft.nbt.CompoundTag();
            cf.putFloat("scale", 1.5f);
            emitCustomData(access, base, cf, "f", "scale", String.format("%08x", Float.floatToRawIntBits(1.5f)));
        }

        // custom_model_data: composite of 4 lists (FLOAT.list, BOOL.list, STRING_UTF8.list, INT.list).
        emitCMD(access, base, java.util.List.of(1.5f, 2.25f), java.util.List.of(true, false),
                java.util.List.of("a", "bb"), java.util.List.of(255, 65280));
        emitCMD(access, base, java.util.List.of(), java.util.List.of(true),
                java.util.List.of(), java.util.List.of());
        emitCMD(access, base, java.util.List.of(0.5f), java.util.List.of(),
                java.util.List.of("model"), java.util.List.of(16711680, -1));

        // attribute_modifiers: list of {Holder<Attribute>, AttributeModifier(id,amount,Operation), slot,
        // Default display}. On a STICK (no default modifiers) so any value is a non-default patch.
        emitAttrMods(access, new Object[][]{
            { Attributes.ATTACK_DAMAGE, "minecraft:base_attack", 7.0,
              AttributeModifier.Operation.ADD_VALUE, EquipmentSlotGroup.MAINHAND } });
        emitAttrMods(access, new Object[][]{
            { Attributes.MAX_HEALTH, "minecraft:hp_boost", 4.0,
              AttributeModifier.Operation.ADD_MULTIPLIED_BASE, EquipmentSlotGroup.ARMOR },
            { Attributes.MOVEMENT_SPEED, "minecraft:speed_pen", -0.05,
              AttributeModifier.Operation.ADD_MULTIPLIED_TOTAL, EquipmentSlotGroup.LEGS } });

        O.flush();
    }
}
