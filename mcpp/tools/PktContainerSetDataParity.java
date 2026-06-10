// Ground truth for net.minecraft.network.protocol.game.ClientboundContainerSetDataPacket.
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ClientboundContainerSetDataPacket.java):
//   ctor        : ClientboundContainerSetDataPacket(int containerId, int id, int value)
//   STREAM_CODEC: Packet.codec(ClientboundContainerSetDataPacket::write,
//                              ClientboundContainerSetDataPacket::new)
//   write(buf)  : buf.writeContainerId(this.containerId);  // VarInt (FriendlyByteBuf line 679: VarInt.write)
//                 buf.writeShort(this.id);                 // 2B big-endian, low 16 bits of int
//                 buf.writeShort(this.value);              // 2B big-endian, low 16 bits of int
//   read(buf)   : containerId = buf.readContainerId();     // VarInt (FriendlyByteBuf line 671: VarInt.read)
//                 id    = buf.readShort();                 // signed 16-bit, sign-extended
//                 value = buf.readShort();                 // signed 16-bit, sign-extended
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. So the whole
// wire payload is exactly: VARINT(containerId) ++ SHORT(id, 2B BE) ++ SHORT(value, 2B BE).
//
// containerId is a plain VarInt (LEB128, no zig-zag). id and value are written by
// FriendlyByteBuf.writeShort, which casts the int to a short and emits the low 16 bits
// big-endian; readShort sign-extends back to int. Therefore the codec only round-trips
// id/value in [-32768, 32767]; this GT keeps id/value within that physical short range so
// the on-wire field and the recovered field agree, and emits the SIGNED short value (the
// value after writeShort+readShort) in the <id>/<value> columns.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <containerId-dec> <id-dec(signed short)> <value-dec(signed short)> <readableBytes-dec> <hex>
//
// Every case is round-trip-decoded through the SAME codec and asserted equal (sanity)
// before emitting. The C++ pkt_container_set_data_parity rebuilds the packet from these
// fields, re-encodes via PacketBuffer (writeVarInt + writeShort + writeShort in the same
// order), and must match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via
// readVarInt + readShort + readShort and checks the recovered fields exactly.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundContainerSetDataPacket;
import net.minecraft.server.Bootstrap;

public class PktContainerSetDataParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundContainerSetDataPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundContainerSetDataPacket>)
                ClientboundContainerSetDataPacket.STREAM_CODEC;

        // containerId: a menu/window id. In vanilla 0 == player inventory, then 1..100 are
        // assigned by AbstractContainerMenu (server cycles 1..100). It is a plain VarInt, so
        // pin every 1->2->3->4->5 byte boundary and the int extremes; negatives encode as
        // 5 bytes (the protocol never sends them, but the codec must round-trip).
        int[] containerIds = {
            0, 1, 2, 5, 30, 50, 99, 100,
            127, 128, 129,                    // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,              // 2->3 byte boundary
            2097151, 2097152, 2097153,        // 3->4 byte boundary
            268435455, 268435456, 268435457,  // 4->5 byte boundary
            Integer.MAX_VALUE,                // 0x7fffffff -> 5 bytes
            -1, -128, Integer.MIN_VALUE       // 5 bytes (no zig-zag)
        };

        // id: which data slot (DataSlot index). value: the data payload. Both go through
        // writeShort (low 16 bits, BE) so they live in the signed-short band [-32768,32767].
        // Pin zero, +/-1, the byte boundary (255/256), and the signed-short extremes.
        short[] ids = {
            0, 1, 2, 3, 7, 15, 100,
            127, 128, 255, 256,
            (short) 0x7fff, (short) 0x8000, (short) 0xffff,  // 32767, -32768, -1
            -1, -128, -255, -256
        };
        short[] values = {
            0, 1, 2, 10, 1000, 9000,
            127, 128, 255, 256, 4096,
            (short) 0x7fff, (short) 0x8000, (short) 0xffff,  // 32767, -32768, -1
            -1, -100, -1000, -32768
        };

        // (A) sweep containerId (VarInt boundaries) with fixed nominal id/value.
        for (int cid : containerIds) {
            emit(CODEC, "cid", cid, (short) 2, (short) 1000);
        }
        // (B) sweep id (short edges) with fixed containerId/value.
        for (short sid : ids) {
            emit(CODEC, "id", 1, sid, (short) 1000);
        }
        // (C) sweep value (short edges) with fixed containerId/id.
        for (short v : values) {
            emit(CODEC, "value", 1, (short) 2, v);
        }
        // (D) a few combined extremes to exercise all three fields together.
        emit(CODEC, "all0", 0, (short) 0, (short) 0);
        emit(CODEC, "allmax", Integer.MAX_VALUE, (short) 0x7fff, (short) 0x7fff);
        emit(CODEC, "allmin", Integer.MIN_VALUE, (short) 0x8000, (short) 0x8000);
        emit(CODEC, "mix", 100, (short) 0xffff, (short) 0x8000);
        emit(CODEC, "mix2", 16384, (short) 256, (short) -1);
    }

    // Construct the real packet, encode through the real STREAM_CODEC, sanity round-trip,
    // and emit the ENC row. id/value are passed as short (already in [-32768,32767]); the
    // ctor widens them to int and write() casts them back via writeShort.
    static void emit(StreamCodec<FriendlyByteBuf, ClientboundContainerSetDataPacket> CODEC,
                     String name, int containerId, short id, short value) {
        ClientboundContainerSetDataPacket pkt =
            new ClientboundContainerSetDataPacket(containerId, id, value);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert field equality.
        // id/value are compared as signed shorts (the readShort result); containerId exactly.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundContainerSetDataPacket dec = CODEC.decode(rbuf);
        if (dec.getContainerId() != containerId
                || (short) dec.getId() != id
                || (short) dec.getValue() != value) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + containerId + "," + id + "," + value + ")"
                + " out=(" + dec.getContainerId() + "," + dec.getId() + ","
                + dec.getValue() + ")");
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(containerId);
        O.print('\t');
        O.print((int) id);     // signed short value, decimal
        O.print('\t');
        O.print((int) value);  // signed short value, decimal
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
