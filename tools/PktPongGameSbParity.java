// Ground truth for net.minecraft.network.protocol.common.ServerboundPongPacket.
//
// (The assignment hint placed this in protocol.game; in 26.1.2 the real class lives in
//  protocol.common — net/minecraft/network/protocol/common/ServerboundPongPacket.java.)
//
// The packet has a single field `int id`. Its STREAM_CODEC is Packet.codec(write, new):
//   write : output.writeInt(this.id)      (ServerboundPongPacket.java:20-22)
//   read  : input.readInt()               (ServerboundPongPacket.java:16-18)
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is exactly a big-endian 4-byte signed int `id` (FriendlyByteBuf.writeInt).
//
// Row format (tab separated):
//   ENC <id-dec> <readableBytes-dec> <hex>     encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert id equality (via the
// public getId()) as a sanity check before emitting. The C++ pkt_pong_game_sb_parity
// rebuilds the packet from <id>, re-encodes via PacketBuffer.writeInt, and must match
// <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via readInt and checks the
// recovered id.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ServerboundPongPacket;
import net.minecraft.server.Bootstrap;

public class PktPongGameSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundPongPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPongPacket>)
                ServerboundPongPacket.STREAM_CODEC;

        // Finite/physical input battery for the BE-4-byte signed int `id`.
        // `id` echoes the keep-alive/ping id the server sent, so the common values are
        // small non-negatives; we also pin the byte-pattern boundaries and the int
        // extremes. writeInt is a plain 4-byte big-endian store (no VarInt, no zig-zag),
        // so every value is exactly 4 bytes on the wire.
        int[] ids = {
            0, 1, 2, 7, 42, 100,
            127, 128, 129,
            255, 256,
            16383, 16384, 16385,
            65535, 65536,
            2097151, 2097152, 2097153,
            16777215, 16777216,            // 0x00ffffff / 0x01000000 byte-3 boundary
            268435455, 268435456, 268435457,
            123456789,
            0x12345678,
            Integer.MAX_VALUE,             // 0x7fffffff
            -1, -2, -128, -256, -16384, -2097152, -16777216,
            Integer.MIN_VALUE              // 0x80000000
        };

        for (int id : ids) {
            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ServerboundPongPacket pkt = new ServerboundPongPacket(id);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundPongPacket dec = CODEC.decode(rbuf);
            if (dec.getId() != id) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + id + " out=" + dec.getId());
            }

            O.print("ENC\t");
            O.print(id);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
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
