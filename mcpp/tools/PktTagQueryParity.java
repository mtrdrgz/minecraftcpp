// Ground truth for net.minecraft.network.protocol.game.ClientboundTagQueryPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL source:
//   ClientboundTagQueryPacket.STREAM_CODEC = Packet.codec(::write, ::new) -> body only, NO id prefix.
//   write(FriendlyByteBuf output):
//       output.writeVarInt(this.transactionId);   // VarInt (plain LEB128)
//       output.writeNbt(this.tag);                // @Nullable CompoundTag (network framing)
//   FriendlyByteBuf.writeNbt(@Nullable Tag tag):
//       if (tag == null) tag = EndTag.INSTANCE; NbtIo.writeAnyTag(tag, out);
//   NbtIo.writeAnyTag(tag, out): out.writeByte(tag.getId()); if (id != 0) tag.write(out);
//       => null   -> single 0x00 byte
//          compound -> 0x0a + unnamed entries + 0x00 terminator
//
// Full payload, in order:  VarInt(transactionId) | NbtIo.writeAnyTag(tag-or-EndTag)
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <transactionId-dec> <present 0|1> <nbtHex> <readableBytes-dec> <hexBytes>
// where <nbtHex> is the EXACT bytes the real writeNbt emitted for the tag (the type byte +
// unnamed payload, or "00" for null), and <hexBytes> is the full packet body. The C++ side
// reconstructs the NBT from <nbtHex> via the certified NbtReader and re-encodes the packet
// (writeVarInt + writeNbt framing) through mc::net::PacketBuffer, requiring byte-for-byte
// parity with <hexBytes> + <readableBytes>.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.nbt.ByteArrayTag;
import net.minecraft.nbt.ByteTag;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.IntArrayTag;
import net.minecraft.nbt.IntTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.LongArrayTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundTagQueryPacket;
import net.minecraft.server.Bootstrap;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktTagQueryParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // transaction ids: exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        int caseNo = 0;

        // null tag (the EndTag fast path: writeNbt(null) -> single 0x00 byte).
        for (int id : new int[] { 0, 1, -1, 42, Integer.MAX_VALUE, Integer.MIN_VALUE }) {
            emit("null" + (caseNo++), id, null);
        }

        // empty compound: 0x0a 0x00 (open compound + immediate End terminator).
        for (int id : ids) {
            emit("empty" + (caseNo++), id, new CompoundTag());
        }

        // single-key compounds (unambiguous order) covering every primitive id.
        emit("byte" + (caseNo++),   1, single(t -> t.putByte("b", (byte) -7)));
        emit("short" + (caseNo++),  2, single(t -> t.putShort("s", (short) 12345)));
        emit("int" + (caseNo++),    3, single(t -> t.putInt("i", -2000000000)));
        emit("long" + (caseNo++),   4, single(t -> t.putLong("l", -9223372036854775808L)));
        emit("float" + (caseNo++),  5, single(t -> t.putFloat("f", 3.5f)));
        emit("floatN" + (caseNo++), 6, single(t -> t.putFloat("f", Float.NaN)));
        emit("dbl" + (caseNo++),    7, single(t -> t.putDouble("d", -123.456789)));
        emit("dblInf" + (caseNo++), 8, single(t -> t.putDouble("d", Double.NEGATIVE_INFINITY)));
        emit("bool" + (caseNo++),   9, single(t -> t.putBoolean("ok", true)));
        emit("str" + (caseNo++),   10, single(t -> t.putString("name", "Steve")));
        emit("strU" + (caseNo++),  11, single(t -> t.putString("u", "café§r你好")));
        emit("strE" + (caseNo++),  12, single(t -> t.putString("e", "")));
        emit("barr" + (caseNo++),  13, single(t -> t.putByteArray("ba", new byte[] { 0, 1, -1, 127, -128 })));
        emit("iarr" + (caseNo++),  14, single(t -> t.putIntArray("ia", new int[] { 0, -1, 2000000000, Integer.MIN_VALUE })));
        emit("larr" + (caseNo++),  15, single(t -> t.putLongArray("la", new long[] { 0L, -1L, Long.MAX_VALUE, Long.MIN_VALUE })));

        // nested compound (single key -> single key, unambiguous).
        emit("nested" + (caseNo++), 100, single(t -> {
            CompoundTag inner = new CompoundTag();
            inner.putInt("x", 16);
            t.put("pos", inner);
        }));

        // a list of strings.
        emit("listStr" + (caseNo++), 101, single(t -> {
            ListTag list = new ListTag();
            list.add(StringTag.valueOf("alpha"));
            list.add(StringTag.valueOf("beta"));
            list.add(StringTag.valueOf("gamma"));
            t.put("words", list);
        }));

        // a list of compounds (each single-key, unambiguous).
        emit("listCmp" + (caseNo++), 102, single(t -> {
            ListTag list = new ListTag();
            CompoundTag a = new CompoundTag(); a.putInt("n", 1); list.add(a);
            CompoundTag b = new CompoundTag(); b.putInt("n", 2); list.add(b);
            t.put("items", list);
        }));

        // a list of ints.
        emit("listInt" + (caseNo++), 103, single(t -> {
            ListTag list = new ListTag();
            list.add(IntTag.valueOf(10));
            list.add(IntTag.valueOf(-20));
            list.add(IntTag.valueOf(Integer.MAX_VALUE));
            t.put("nums", list);
        }));

        // empty list (END element type).
        emit("listEmpty" + (caseNo++), 104, single(t -> t.put("empty", new ListTag())));

        // a "typical" block-entity-ish payload, but built so each compound has ONE key per
        // level (keeps Java HashMap iteration order unambiguous w.r.t. the C++ insertion order
        // produced by read->write). The C++ side parses the Java bytes and re-emits them, so any
        // multi-key ordering would still pass; single-key keeps the GT itself human-checkable.
        emit("entity" + (caseNo++), 200, single(t -> t.putString("id", "minecraft:chest")));
    }

    interface TagBuilder { void build(CompoundTag t); }

    static CompoundTag single(TagBuilder b) {
        CompoundTag t = new CompoundTag();
        b.build(t);
        return t;
    }

    static void emit(String name, int transactionId, CompoundTag tag) {
        ClientboundTagQueryPacket pkt = new ClientboundTagQueryPacket(transactionId, tag);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundTagQueryPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // The exact NBT framing bytes the real writeNbt emitted for this tag (the suffix of the
        // packet body). Recompute it standalone via the SAME FriendlyByteBuf.writeNbt path.
        FriendlyByteBuf nbtBuf = new FriendlyByteBuf(Unpooled.buffer());
        nbtBuf.writeNbt(tag);
        int m = nbtBuf.readableBytes();
        StringBuilder nbtHex = new StringBuilder();
        for (int i = 0; i < m; i++) nbtHex.append(String.format("%02x", nbtBuf.getByte(nbtBuf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): the transaction id must survive,
        // and the decoded tag must re-serialize to the same NBT bytes.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundTagQueryPacket back = ClientboundTagQueryPacket.STREAM_CODEC.decode(rb);
        if (back.getTransactionId() != transactionId) {
            throw new IllegalStateException("round-trip id mismatch: " + back.getTransactionId() + " != " + transactionId);
        }
        FriendlyByteBuf rt = new FriendlyByteBuf(Unpooled.buffer());
        rt.writeNbt(back.getTag());
        StringBuilder rtHex = new StringBuilder();
        int rn = rt.readableBytes();
        for (int i = 0; i < rn; i++) rtHex.append(String.format("%02x", rt.getByte(rt.readerIndex() + i)));
        if (!rtHex.toString().equals(nbtHex.toString())) {
            throw new IllegalStateException("round-trip nbt mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        int present = tag != null ? 1 : 0;

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(transactionId);
        O.print('\t');
        O.print(present);
        O.print('\t');
        O.print(nbtHex.toString());
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
