// Ground truth for net.minecraft.network.protocol.game.ClientboundSetEquipmentPacket.
//
// STREAM_CODEC = Packet.codec(::write, ::new) over RegistryFriendlyByteBuf
// (ClientboundSetEquipmentPacket.java:14-16). The wire body is the hand-written
// write() (lines 39-51):
//   output.writeVarInt(this.entity)
//   for i in [0, slots.size()):
//     slotId        = slots.get(i).getFirst().ordinal()     // EquipmentSlot.ordinal()
//     shouldContinue= (i != size - 1)
//     output.writeByte(shouldContinue ? slotId | -128 : slotId)   // CONTINUE_MASK = -128 (0x80)
//     ItemStack.OPTIONAL_STREAM_CODEC.encode(output, slots.get(i).getSecond())
// i.e. every slot byte = ordinal | (continue ? 0x80 : 0); the LAST slot has the
// high bit CLEAR (that terminates the do-while decode loop, lines 31-36). writeByte(int)
// emits the low 8 bits, so for ordinal 0..7 the byte is 0x00..0x07, or 0x80..0x87 when
// the continuation bit is set.
//
// EquipmentSlot enum ordinals (EquipmentSlot.java:13-20):
//   MAINHAND=0 OFFHAND=1 FEET=2 LEGS=3 CHEST=4 HEAD=5 BODY=6 SADDLE=7
// NOTE: the wire byte uses ordinal(), NOT EquipmentSlot.id (which differs: OFFHAND id=5,
// FEET id=1, ...). decode() reads EquipmentSlot.VALUES.get(slotId & 127) — also ordinal.
//
// ItemStack.OPTIONAL_STREAM_CODEC (ItemStack.java:170-194), NO-COMPONENT domain only:
//   empty stack        -> writeVarInt(0)
//   no-component stack  -> writeVarInt(count) VarInt(itemId) VarInt(0) VarInt(0)
//   itemId = Item.STREAM_CODEC = holderRegistry(ITEM) -> plain VarInt(getId).
// A freshly-built new ItemStack(item,count) has an empty DataComponent patch by
// construction, so it lands in exactly that domain. Stacks WITH components need the
// per-component patch codecs (a separate wave) and are intentionally NOT exercised.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <entityId-dec> <slotCount-dec> [ <slotOrdinal-dec> <itemName|"-"> <count-dec> <isEmpty 0/1> ]* <readableBytes-dec> <hexBytes>
// The per-slot triple-plus-ordinal repeats <slotCount> times. <hexBytes> is the full
// packet body (lowercase hex). The C++ side rebuilds the exact bytes through PacketBuffer
// and must match <hexBytes> byte-for-byte AND <readableBytes>.
import com.mojang.datafixers.util.Pair;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetEquipmentPacket;
import net.minecraft.world.entity.EquipmentSlot;
import net.minecraft.world.item.Item;
import net.minecraft.world.item.ItemStack;

public class PktSetEquipmentParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x & 0xff));
        return s.toString();
    }

    // One slot spec: a chosen EquipmentSlot + an ItemStack (EMPTY or no-component).
    static final class Slot {
        final EquipmentSlot slot;
        final ItemStack stack;
        final String name; // item ns:path, or "-" for EMPTY
        Slot(EquipmentSlot s, ItemStack st, String n) { slot = s; stack = st; name = n; }
    }

    static Slot empty(EquipmentSlot s) { return new Slot(s, ItemStack.EMPTY, "-"); }

    static Slot of(EquipmentSlot s, int id, int count) {
        Item item = BuiltInRegistries.ITEM.byId(id);
        String name = BuiltInRegistries.ITEM.getKey(item).toString();
        // Freshly-constructed -> empty component patch -> no-component wire form.
        return new Slot(s, new ItemStack(item, count), name);
    }

    static void emit(RegistryAccess access, int entityId, List<Slot> slots) {
        List<Pair<EquipmentSlot, ItemStack>> pairs = new ArrayList<>();
        for (Slot s : slots) pairs.add(Pair.of(s.slot, s.stack));
        ClientboundSetEquipmentPacket pkt = new ClientboundSetEquipmentPacket(entityId, pairs);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        ClientboundSetEquipmentPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        byte[] bytes = new byte[n];
        buf.duplicate().readBytes(bytes);

        // Round-trip decode through the SAME codec (sanity): entity + slot ordinals + stack
        // empties/counts must all survive.
        RegistryFriendlyByteBuf rb = new RegistryFriendlyByteBuf(Unpooled.copiedBuffer(buf), access);
        ClientboundSetEquipmentPacket back = ClientboundSetEquipmentPacket.STREAM_CODEC.decode(rb);
        if (back.getEntity() != entityId)
            throw new IllegalStateException("round-trip entity mismatch: " + back.getEntity() + " != " + entityId);
        List<Pair<EquipmentSlot, ItemStack>> bs = back.getSlots();
        if (bs.size() != slots.size())
            throw new IllegalStateException("round-trip slot count mismatch: " + bs.size() + " != " + slots.size());
        for (int i = 0; i < slots.size(); i++) {
            if (bs.get(i).getFirst() != slots.get(i).slot)
                throw new IllegalStateException("round-trip slot mismatch at " + i);
            ItemStack a = slots.get(i).stack, d = bs.get(i).getSecond();
            if (a.isEmpty() != d.isEmpty() || (!a.isEmpty() && a.getCount() != d.getCount()))
                throw new IllegalStateException("round-trip stack mismatch at " + i);
        }
        if (rb.readableBytes() != 0)
            throw new IllegalStateException("round-trip left trailing bytes");

        StringBuilder row = new StringBuilder();
        row.append("ENC\t").append(entityId).append('\t').append(slots.size());
        for (Slot s : slots) {
            row.append('\t').append(s.slot.ordinal())
               .append('\t').append(s.name)
               .append('\t').append(s.stack.isEmpty() ? 0 : s.stack.getCount())
               .append('\t').append(s.stack.isEmpty() ? 1 : 0);
        }
        row.append('\t').append(n).append('\t').append(hex(bytes));
        O.println(row.toString());
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // Bind item data components (registry holders start unbound) — required before
        // ItemStack.components / OPTIONAL_STREAM_CODEC.encode.
        net.minecraft.core.registries.BuiltInRegistries.DATA_COMPONENT_INITIALIZERS
            .build(net.minecraft.data.registries.VanillaRegistries.createLookup())
            .forEach(p -> p.apply());
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        EquipmentSlot[] V = EquipmentSlot.values(); // index == ordinal
        int nItems = BuiltInRegistries.ITEM.size();
        int maxId = nItems - 1;

        // Entity ids exercise the VarInt 1->5 byte boundaries + extrema.
        int[] entityIds = { 0, 1, 127, 128, 16383, 16384, 2097151, 2097152, -1, Integer.MAX_VALUE };

        // --- Single-slot lists (last slot -> high bit CLEAR): one per EquipmentSlot, empty + a stack.
        for (EquipmentSlot s : V) {
            List<Slot> one = new ArrayList<>();
            one.add(empty(s));
            emit(access, 1, one);

            List<Slot> two = new ArrayList<>();
            two.add(of(s, 1, 64)); // diamond_sword-ish id won't be exact, just a real low id
            emit(access, 1, two);
        }

        // --- Two-slot lists (first slot -> high bit SET, second CLEAR): every pair semantics.
        emit(access, 42, List.of(
            of(V[0], 1, 1),    // MAINHAND, item id 1 count 1
            empty(V[5])));     // HEAD empty
        emit(access, 42, List.of(
            empty(V[0]),       // MAINHAND empty
            of(V[1], 50, 16))); // OFFHAND item id 50 count 16

        // --- A full 8-slot list in ordinal order: every continuation byte set except the last.
        {
            List<Slot> full = new ArrayList<>();
            full.add(of(V[0], 1, 1));      // MAINHAND
            full.add(of(V[1], 2, 64));     // OFFHAND
            full.add(empty(V[2]));         // FEET empty
            full.add(of(V[3], 126, 99));   // LEGS
            full.add(of(V[4], 127, 127));  // CHEST  (count VarInt 1-byte edge)
            full.add(of(V[5], 128, 128));  // HEAD   (count VarInt 2-byte: 128 -> 0x80 0x01)
            full.add(empty(V[6]));         // BODY empty
            full.add(of(V[7], 255, 16));   // SADDLE (item id VarInt boundary)
        emit(access, 1234567, full);
        }

        // --- Item id / count VarInt boundary sweep across all entity ids, on a single CHEST slot.
        int[] ids = { 0, 1, 2, 50, 126, 127, 128, 255, 256, maxId };
        int[] counts = { 1, 16, 64, 99, 127, 128 };
        for (int eid : entityIds) {
            for (int id : ids) {
                if (id < 0 || id >= nItems) continue;
                for (int c : counts) {
                    emit(access, eid, List.of(of(V[4], id, c))); // CHEST
                }
            }
            // also an empty single-slot per entity id (exercise VarInt(0) stack with each entity)
            emit(access, eid, List.of(empty(V[4])));
        }

        O.flush();
    }
}
