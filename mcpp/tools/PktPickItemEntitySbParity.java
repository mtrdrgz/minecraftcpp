// Ground truth for net.minecraft.network.protocol.game.ServerboundPickItemFromEntityPacket.
//
// The packet is a record(int id, boolean includeData). Its STREAM_CODEC is
// StreamCodec.composite of:
//   ByteBufCodecs.VAR_INT  -> id           (output.writeVarInt(id) / input.readVarInt())
//   ByteBufCodecs.BOOL     -> includeData  (output.writeBoolean(b) / input.readBoolean())
// (net.minecraft.network.protocol.game.ServerboundPickItemFromEntityPacket lines 9-16;
//  ByteBufCodecs.VAR_INT/BOOL definitions in net.minecraft.network.codec.ByteBufCodecs.)
// composite body codec: NO packet-id prefix, just the body, so the whole wire payload is
// one VarInt (LEB128, signed, no zig-zag) then one boolean byte (0x00 / 0x01).
//
// Row format (tab separated):
//   ENC <id-dec> <includeData-dec(0/1)> <readableBytes-dec> <hex>
//        encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert both fields equal as a
// sanity check before emitting. The C++ pkt_pick_item_entity_sb_parity rebuilds the packet
// from <id,includeData>, re-encodes via PacketBuffer.writeVarInt + writeBool in that order,
// and must match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt
// then readBool and checks the recovered fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPickItemFromEntityPacket;
import net.minecraft.server.Bootstrap;

public class PktPickItemEntitySbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet. The packet declares
        //   STREAM_CODEC : StreamCodec<ByteBuf, ServerboundPickItemFromEntityPacket>
        // and FriendlyByteBuf extends ByteBuf, so it is usable as a FriendlyByteBuf codec.
        StreamCodec<FriendlyByteBuf, ServerboundPickItemFromEntityPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPickItemFromEntityPacket>)
                (StreamCodec<?, ?>) ServerboundPickItemFromEntityPacket.STREAM_CODEC;

        // id is an entity id (arbitrary int). Pin every LEB128 byte boundary
        // (1->2->3->4->5 bytes) and the int extremes (negatives encode as 5 bytes in
        // LEB128 since writeVarInt does not zig-zag).
        int[] ids = {
            0, 1, 2, 7, 36, 100,
            127, 128, 129,                   // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,             // 2->3 byte boundary
            2097151, 2097152, 2097153,       // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,               // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE                // 0x80000000 -> 5 bytes
        };

        // includeData is a single boolean: both states, paired against every id width.
        for (int i = 0; i < ids.length; i++) {
            boolean inc = (i % 2) == 0;          // alternate true/false across id widths
            emit(CODEC, ids[i], inc);
            emit(CODEC, ids[i], !inc);           // and the complement, so every id sees both
        }
        // Explicit corner combos.
        emit(CODEC, 0, false);
        emit(CODEC, 0, true);
        emit(CODEC, Integer.MAX_VALUE, true);
        emit(CODEC, Integer.MAX_VALUE, false);
        emit(CODEC, Integer.MIN_VALUE, true);
        emit(CODEC, Integer.MIN_VALUE, false);
        emit(CODEC, -1, true);
        emit(CODEC, -1, false);
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundPickItemFromEntityPacket> CODEC,
                     int id, boolean includeData) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundPickItemFromEntityPacket pkt =
            new ServerboundPickItemFromEntityPacket(id, includeData);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundPickItemFromEntityPacket dec = CODEC.decode(rbuf);
        if (dec.id() != id || dec.includeData() != includeData) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + id + "," + includeData + ") out=("
                    + dec.id() + "," + dec.includeData() + ")");
        }

        O.print("ENC\t");
        O.print(id);
        O.print('\t');
        O.print(includeData ? 1 : 0);
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
