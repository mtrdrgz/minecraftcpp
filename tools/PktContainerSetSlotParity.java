// Ground truth for net.minecraft.network.protocol.game.ClientboundContainerSetSlotPacket.STREAM_CODEC.
// write (ClientboundContainerSetSlotPacket.java:32-37):
//   output.writeContainerId(containerId)   -- FriendlyByteBuf.writeContainerId = VarInt.write (FriendlyByteBuf.java:679-681)
//   output.writeVarInt(stateId)
//   output.writeShort(slot)                -- 2-byte big-endian short (netty ByteBuf.writeShort)
//   ItemStack.OPTIONAL_STREAM_CODEC.encode(output, itemStack)
//       empty stack       -> VarInt(0)
//       no-component stack -> VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)
//       (Item.STREAM_CODEC = holderRegistry(ITEM) = plain VarInt(getId); empty DataComponentPatch
//        = VarInt(0)added + VarInt(0)removed -- ItemStack.java:170-194, DataComponentPatch.java:106-109)
// This gate covers EMPTY + NO-COMPONENT ItemStacks only (newly created stacks have an empty patch);
// stacks carrying DataComponents need the per-component patch codecs (a separate later wave).
//
//   tools/run_groundtruth.ps1 -Tool PktContainerSetSlotParity -Out mcpp/build/pkt_container_set_slot.tsv
//
// Row: ENC \t <containerId> \t <stateId> \t <slot> \t <itemName ns:path or "-"> \t <count> \t <isEmpty 0/1> \t <readableBytes> \t <wireHex>

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundContainerSetSlotPacket;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class PktContainerSetSlotParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    static void emit(RegistryAccess access, int containerId, int stateId, int slot, String name, ItemStack stack) {
        ClientboundContainerSetSlotPacket pkt = new ClientboundContainerSetSlotPacket(containerId, stateId, slot, stack);
        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ClientboundContainerSetSlotPacket.STREAM_CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        byte[] bytes = new byte[readable];
        buf.duplicate().readBytes(bytes);
        // sanity: round-trip decode through the SAME codec
        ClientboundContainerSetSlotPacket back = ClientboundContainerSetSlotPacket.STREAM_CODEC.decode(buf);
        if (back.getContainerId() != containerId || back.getStateId() != stateId || back.getSlot() != slot
                || back.getItem().isEmpty() != stack.isEmpty()
                || (!stack.isEmpty() && back.getItem().getCount() != stack.getCount()))
            throw new IllegalStateException("round-trip mismatch for " + name);
        O.println("ENC\t" + containerId + "\t" + stateId + "\t" + slot + "\t" + name
                + "\t" + (stack.isEmpty() ? 0 : stack.getCount())
                + "\t" + (stack.isEmpty() ? 1 : 0) + "\t" + readable + "\t" + hex(bytes));
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind item data components (registry holders start unbound; same as RegistryDataCollector
        // at runtime) -- required before ItemStack.components / the OPTIONAL_STREAM_CODEC encode.
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // Header field spreads crossing the VarInt 1->2->3-byte boundaries for containerId/stateId,
        // and the signed short range (incl. -1 = the "carried"/full-update slot vanilla uses) for slot.
        int[] containerIds = { 0, 1, 127, 128, 255, 16383, 16384 };
        int[] stateIds = { 0, 1, 127, 128, 16384, 2097151, 2097152 };
        int[] slots = { -1, 0, 1, 36, 127, 128, 255, 256, 32767, -32768 };

        // EMPTY stack across a representative header set.
        for (int cid : containerIds)
            for (int sid : stateIds)
                for (int sl : slots)
                    emit(access, cid, sid, sl, "-", ItemStack.EMPTY);

        // No-component stacks: item ids x counts crossing VarInt count/id byte boundaries, paired
        // with header values. Items pulled in registry id order (byId); a freshly-constructed stack
        // == the item prototype, so its component PATCH is empty -> the no-component wire form.
        int[] ids = { 0, 1, 2, 50, 126, 127, 128, 255, 256, 1505 };  // last = near-max item id
        int[] counts = { 1, 16, 64, 99, 127, 128 };
        int n = BuiltInRegistries.ITEM.size();
        int hi = 0;
        for (int id : ids) {
            if (id >= n) continue;
            Item item = BuiltInRegistries.ITEM.byId(id);
            String name = BuiltInRegistries.ITEM.getKey(item).toString();
            for (int c : counts) {
                int cid = containerIds[hi % containerIds.length];
                int sid = stateIds[hi % stateIds.length];
                int sl = slots[hi % slots.length];
                ++hi;
                emit(access, cid, sid, sl, name, new ItemStack(item, c));
            }
        }
        O.flush();
    }
}
