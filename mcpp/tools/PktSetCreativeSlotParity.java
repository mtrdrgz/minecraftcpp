// Ground truth for net.minecraft.network.protocol.game.ServerboundSetCreativeModeSlotPacket
// vs its REAL STREAM_CODEC (ServerboundSetCreativeModeSlotPacket.java:11-17):
//   StreamCodec.composite(
//     ByteBufCodecs.SHORT,                                              slotNum
//     ItemStack.validatedStreamCodec(ItemStack.OPTIONAL_UNTRUSTED_STREAM_CODEC), itemStack
//     ServerboundSetCreativeModeSlotPacket::new)
//
// Wire form (byte-for-byte):
//   ByteBufCodecs.SHORT.encode (ByteBufCodecs.java:80-82) = output.writeShort(value) -> 2 bytes big-endian.
//   ItemStack.validatedStreamCodec.encode (ItemStack.java:209-211) just delegates to the inner
//     codec's encode (validation runs only on DECODE), adding NO bytes.
//   OPTIONAL_UNTRUSTED_STREAM_CODEC = createOptionalStreamCodec(DataComponentPatch.DELIMITED_STREAM_CODEC)
//     (ItemStack.java:126-128). encode (ItemStack.java:185-193):
//       empty -> writeVarInt(0)
//       else  -> writeVarInt(count); Item.STREAM_CODEC.encode(holder); patchCodec.encode(patch)
//     Item.STREAM_CODEC = holderRegistry(ITEM) -> plain VarInt(getId).
//     DELIMITED_STREAM_CODEC == STREAM_CODEC (both createStreamCodec, DataComponentPatch.java:62/68);
//     for an EMPTY patch encode is identical: writeVarInt(0) writeVarInt(0)
//     (DataComponentPatch.java:106-109). The delimited length prefix only wraps PRESENT components,
//     so for the no-component domain the bytes are exactly VarInt(count) VarInt(itemId) VarInt(0) VarInt(0).
//
// So for slotNum S and a no-component stack of item X count N:
//   short(S) | VarInt(N) VarInt(itemId) VarInt(0) VarInt(0)
// and for slotNum S with ItemStack.EMPTY:
//   short(S) | VarInt(0)
//
// Restricted to EMPTY + NO-COMPONENT ItemStacks (freshly-built new ItemStack(item,count) — its
// DataComponent PATCH is empty by construction). Stacks WITH components need the per-component
// patch codecs (a separate later wave) and are NOT in this gate.
//
//   tools/run_groundtruth.ps1 -Tool PktSetCreativeSlotParity -Out mcpp/build/pkt_set_creative_slot.tsv
//
// Row: ENC \t <slotNum> \t <itemName ns:path or "-" for empty> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ServerboundSetCreativeModeSlotPacket;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class PktSetCreativeSlotParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static void emit(RegistryAccess access, short slotNum, String name, ItemStack stack) {
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ServerboundSetCreativeModeSlotPacket pkt = new ServerboundSetCreativeModeSlotPacket(slotNum, stack);
        ServerboundSetCreativeModeSlotPacket.STREAM_CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        // sanity: round-trip decode through the SAME packet codec
        ServerboundSetCreativeModeSlotPacket back = ServerboundSetCreativeModeSlotPacket.STREAM_CODEC.decode(buf);
        if (back.slotNum() != slotNum
                || back.itemStack().isEmpty() != stack.isEmpty()
                || (!stack.isEmpty() && back.itemStack().getCount() != stack.getCount()))
            throw new IllegalStateException("round-trip mismatch for slot=" + slotNum + " " + name);
        O.println("ENC\t" + slotNum + "\t" + name
                + "\t" + (stack.isEmpty() ? 0 : stack.getCount())
                + "\t" + (stack.isEmpty() ? 1 : 0)
                + "\t" + readable + "\t" + hex(bytes));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind item data components (registry holders start unbound; same as RegistryDataCollector
        // at runtime) — required before ItemStack.components / the OPTIONAL_UNTRUSTED encode.
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // slotNum spread crossing the signed-short range / VarInt boundaries are irrelevant here
        // (slot is a fixed 2-byte short), but exercise representative + boundary values.
        short[] slots = { 0, 1, 9, 36, 45, 127, 128, 255, 256, (short) -1, (short) -999, Short.MAX_VALUE, Short.MIN_VALUE };

        // EMPTY stack across a few slots.
        for (short s : slots) emit(access, s, "-", ItemStack.EMPTY);

        // No-component stacks: a spread of item ids x counts crossing the VarInt 1->2-byte
        // count/id boundaries. Items pulled in registry id order (byId).
        int[] ids = { 0, 1, 2, 50, 126, 127, 128, 255, 256, 1505 };  // last ~ max item id
        // ServerboundSetCreativeModeSlotPacket uses the VALIDATED ItemStack codec, which
        // round-trip-validates count to [1;99] (max stack count on the untrusted path);
        // counts > 99 are rejected by decode and are non-physical for this packet.
        int[] counts = { 1, 16, 64, 99 };
        int n = BuiltInRegistries.ITEM.size();
        for (short s : slots) {
            for (int id : ids) {
                if (id >= n) continue;
                Item item = BuiltInRegistries.ITEM.byId(id);
                String name = BuiltInRegistries.ITEM.getKey(item).toString();
                for (int c : counts) {
                    // A freshly-constructed ItemStack == the item prototype, so its component
                    // PATCH (diff from prototype) is empty by construction -> the no-component
                    // wire form.
                    emit(access, s, name, new ItemStack(item, c));
                }
            }
        }
        O.flush();
    }
}
