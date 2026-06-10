// Ground truth for net.minecraft.network.protocol.game.ClientboundSetCursorItemPacket's
// STREAM_CODEC (the inventory packet that sets the held-on-cursor stack).
//
// ClientboundSetCursorItemPacket (ClientboundSetCursorItemPacket.java:9-12):
//   public record ClientboundSetCursorItemPacket(ItemStack contents) ...
//   STREAM_CODEC = StreamCodec.composite(ItemStack.OPTIONAL_STREAM_CODEC, ::contents, ::new)
// One field, encoded through ItemStack.OPTIONAL_STREAM_CODEC (createOptionalStreamCodec,
// ItemStack.java:170-194):
//   encode: if isEmpty -> writeVarInt(0)
//           else      -> writeVarInt(count); Item.STREAM_CODEC.encode(holder);
//                        DataComponentPatch.STREAM_CODEC.encode(patch)
//   Item.STREAM_CODEC = ByteBufCodecs.holderRegistry(Registries.ITEM) -> plain VarInt(getId).
//   DataComponentPatch.encode, EMPTY patch (DataComponentPatch.java:106-109): writeVarInt(0)
//     [added count] + writeVarInt(0) [removed count].
// So a NO-COMPONENT stack of item X count N = VarInt(N) VarInt(itemId) VarInt(0) VarInt(0);
// EMPTY = VarInt(0). This gate covers exactly that domain (no DataComponents). Stacks WITH
// components need the per-component patch codecs (a separate later wave) and are out of scope.
//
// We build the REAL packet (no-component ItemStacks only), encode it through the packet's own
// STREAM_CODEC over a RegistryFriendlyByteBuf, dump readableBytes + hex, and round-trip decode
// for sanity.
//
//   tools/run_groundtruth.ps1 -Tool PktSetCursorItemParity -Out mcpp/build/pkt_set_cursor_item.tsv
//
// Row: ENC \t <itemName ns:path or "-" for empty> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetCursorItemPacket;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class PktSetCursorItemParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static void emit(RegistryAccess access, String name, ItemStack stack) {
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ClientboundSetCursorItemPacket pkt = new ClientboundSetCursorItemPacket(stack);
        ClientboundSetCursorItemPacket.STREAM_CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        // sanity: round-trip decode through the SAME codec
        ClientboundSetCursorItemPacket back = ClientboundSetCursorItemPacket.STREAM_CODEC.decode(buf);
        ItemStack bc = back.contents();
        if (bc.isEmpty() != stack.isEmpty() || (!stack.isEmpty() && bc.getCount() != stack.getCount()))
            throw new IllegalStateException("round-trip mismatch for " + name);
        O.println("ENC\t" + name + "\t" + (stack.isEmpty() ? 0 : stack.getCount())
                + "\t" + (stack.isEmpty() ? 1 : 0) + "\t" + readable + "\t" + hex(bytes));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind item data components (registry holders start unbound; same as RegistryDataCollector
        // at runtime) — required before ItemStack.components / the OPTIONAL_STREAM_CODEC encode.
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // EMPTY stack (OPTIONAL_STREAM_CODEC allows empty -> VarInt(0)).
        emit(access, "-", ItemStack.EMPTY);

        // No-component stacks: a spread of item ids x counts crossing the VarInt 1->2-byte
        // count/id boundaries. Items pulled in registry id order (byId); a freshly-created
        // ItemStack == the item prototype, so its component PATCH is empty by construction ->
        // the no-component wire form (VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)).
        int[] ids = { 0, 1, 2, 50, 126, 127, 128, 255, 256, 1505 };  // last ~ max item id
        int[] counts = { 1, 16, 64, 99, 127, 128 };
        int n = BuiltInRegistries.ITEM.size();
        for (int id : ids) {
            if (id >= n) continue;
            Item item = BuiltInRegistries.ITEM.byId(id);
            String name = BuiltInRegistries.ITEM.getKey(item).toString();
            for (int c : counts) {
                emit(access, name, new ItemStack(item, c));
            }
        }
        O.flush();
    }
}
