// Ground truth for net.minecraft.network.protocol.game.ServerboundSelectBundleItemPacket.
//
// The packet is a record(int slotId, int selectedItemIndex). Its STREAM_CODEC is
// Packet.codec(write, new), where write/read are exactly:
//   write : output.writeVarInt(this.slotId); output.writeVarInt(this.selectedItemIndex);
//   read  : this(input.readVarInt(), input.readVarInt());  // + validation below
// (net.minecraft.network.protocol.game.ServerboundSelectBundleItemPacket lines 8-23).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is two VarInts (LEB128, signed, no zig-zag): slotId then selectedItemIndex.
//
// NOTE: the decode ctor throws IllegalArgumentException when
//   selectedItemIndex < 0 && selectedItemIndex != -1.
// So every emitted case keeps selectedItemIndex in {-1} U {>= 0}; slotId is unconstrained.
//
// Row format (tab separated):
//   ENC <slotId-dec> <selectedItemIndex-dec> <readableBytes-dec> <hex>
//        encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert both ints equal as a
// sanity check before emitting. The C++ pkt_select_bundle_item_sb_parity rebuilds the
// packet from <slotId,selectedItemIndex>, re-encodes via PacketBuffer.writeVarInt twice,
// and must match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via two
// readVarInt calls and checks the recovered ints.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSelectBundleItemPacket;
import net.minecraft.server.Bootstrap;

public class PktSelectBundleItemSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundSelectBundleItemPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSelectBundleItemPacket>)
                ServerboundSelectBundleItemPacket.STREAM_CODEC;

        // slotId is an arbitrary container slot index; we still pin every LEB128 byte
        // boundary (1->2->3->4->5 bytes) and the int extremes (negatives encode as 5
        // bytes in LEB128 since writeVarInt does not zig-zag).
        int[] slotIds = {
            0, 1, 2, 7, 36, 100,
            127, 128, 129,                 // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,           // 2->3 byte boundary
            2097151, 2097152, 2097153,     // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,             // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE              // 0x80000000 -> 5 bytes
        };

        // selectedItemIndex: -1 is the sentinel "nothing selected"; the decode ctor rejects
        // any other negative, so legal values are {-1} U {>= 0}. Pin LEB128 boundaries too.
        int[] selectedIndexes = {
            -1, 0, 1, 2, 7, 63, 100,
            127, 128, 129,                 // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,           // 2->3 byte boundary
            2097151, 2097152, 2097153,     // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE              // 0x7fffffff -> 5 bytes
        };

        // Cross product would be large but harmless; keep it focused: pair each slotId with
        // a rotating selectedItemIndex AND each selectedItemIndex with a rotating slotId, so
        // both VarInt-length boundaries are exercised on both fields, including mixed widths.
        for (int i = 0; i < slotIds.length; i++) {
            int slot = slotIds[i];
            int sel = selectedIndexes[i % selectedIndexes.length];
            emit(CODEC, slot, sel);
        }
        for (int j = 0; j < selectedIndexes.length; j++) {
            int sel = selectedIndexes[j];
            int slot = slotIds[j % slotIds.length];
            emit(CODEC, slot, sel);
        }
        // A few explicit corner combos (both sentinel / both max / mixed extremes).
        emit(CODEC, 0, 0);
        emit(CODEC, 0, -1);
        emit(CODEC, -1, -1);
        emit(CODEC, Integer.MIN_VALUE, -1);
        emit(CODEC, Integer.MAX_VALUE, Integer.MAX_VALUE);
        emit(CODEC, Integer.MIN_VALUE, Integer.MAX_VALUE);
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundSelectBundleItemPacket> CODEC,
                     int slotId, int selectedItemIndex) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundSelectBundleItemPacket pkt =
            new ServerboundSelectBundleItemPacket(slotId, selectedItemIndex);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundSelectBundleItemPacket dec = CODEC.decode(rbuf);
        if (dec.slotId() != slotId || dec.selectedItemIndex() != selectedItemIndex) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + slotId + "," + selectedItemIndex + ") out=("
                    + dec.slotId() + "," + dec.selectedItemIndex() + ")");
        }

        O.print("ENC\t");
        O.print(slotId);
        O.print('\t');
        O.print(selectedItemIndex);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
