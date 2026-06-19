// Ground truth for ClientboundSetPlayerInventoryPacket.STREAM_CODEC
// (ClientboundSetPlayerInventoryPacket.java:10-17):
//   record(int slot, ItemStack contents); STREAM_CODEC = StreamCodec.composite(
//     ByteBufCodecs.VAR_INT,                 -> slot
//     ItemStack.OPTIONAL_STREAM_CODEC,       -> contents
//     ::new)
// Composite encodes fields IN ORDER: VarInt(slot), then the ItemStack OPTIONAL wire form.
//
// ItemStack.OPTIONAL_STREAM_CODEC (ItemStack.java:170-194):
//   empty            -> writeVarInt(0)
//   no-component     -> writeVarInt(count) Item.STREAM_CODEC.encode(holder) DataComponentPatch.encode(patch)
//   Item.STREAM_CODEC = ByteBufCodecs.holderRegistry(Registries.ITEM) -> plain VarInt(getId).
//   EMPTY patch (DataComponentPatch.java:106-109): writeVarInt(0)[added] writeVarInt(0)[removed].
// So a no-component contents stack = VarInt(count) VarInt(itemId) VarInt(0) VarInt(0); EMPTY = VarInt(0).
// This gate covers exactly the EMPTY + NO-COMPONENT ItemStack domain (newly created stacks have an
// empty component patch); stacks WITH DataComponents need the per-component patch codecs (a later wave).
//
//   tools/run_groundtruth.ps1 -Tool PktSetPlayerInventoryParity -Out mcpp/build/pkt_set_player_inventory.tsv
//
// Row: ENC \t <slot> \t <itemName ns:path or "-" for empty> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetPlayerInventoryPacket;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class PktSetPlayerInventoryParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static void emit(RegistryAccess access, int slot, String name, ItemStack stack) {
        ClientboundSetPlayerInventoryPacket pkt = new ClientboundSetPlayerInventoryPacket(slot, stack);
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ClientboundSetPlayerInventoryPacket.STREAM_CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        // sanity: round-trip decode through the SAME packet codec
        ClientboundSetPlayerInventoryPacket back = ClientboundSetPlayerInventoryPacket.STREAM_CODEC.decode(buf);
        ItemStack bc = back.contents();
        if (back.slot() != slot || bc.isEmpty() != stack.isEmpty()
                || (!stack.isEmpty() && bc.getCount() != stack.getCount()))
            throw new IllegalStateException("round-trip mismatch slot=" + slot + " item=" + name);
        O.println("ENC\t" + slot + "\t" + name + "\t" + (stack.isEmpty() ? 0 : stack.getCount())
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

        // slot values crossing the VarInt 1->2->3-byte boundaries (slot is a plain VarInt).
        int[] slots = { 0, 1, 8, 35, 40, 127, 128, 255, 16384 };

        // No-component stacks: a spread of item ids x counts crossing the VarInt 1->2-byte
        // count/id boundaries, plus EMPTY. Items are pulled in registry id order (byId).
        int[] ids = { 0, 1, 2, 50, 126, 127, 128, 255, 256, 1505 };  // last ~ max item id
        int[] counts = { 1, 16, 64, 99, 127, 128 };
        int n = BuiltInRegistries.ITEM.size();

        for (int slot : slots) {
            // EMPTY contents.
            emit(access, slot, "-", ItemStack.EMPTY);
            for (int id : ids) {
                if (id >= n) continue;
                Item item = BuiltInRegistries.ITEM.byId(id);
                String name = BuiltInRegistries.ITEM.getKey(item).toString();
                for (int c : counts) {
                    // A freshly-constructed ItemStack == the item prototype, so its component
                    // PATCH (diff from prototype) is empty by construction -> the no-component
                    // wire form (VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)).
                    emit(access, slot, name, new ItemStack(item, c));
                }
            }
        }
        O.flush();
    }
}
