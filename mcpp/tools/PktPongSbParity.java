// Ground truth for the ServerboundPongPacket codec.
//
// ServerboundPongPacket.STREAM_CODEC writes a single body field: writeInt(id),
// i.e. a 4-byte big-endian int (net.minecraft.network.FriendlyByteBuf.writeInt ->
// netty ByteBuf.writeInt, big-endian). No packet-type id is part of STREAM_CODEC;
// that framing is added separately by the protocol. So the encoded body is exactly
// the 4 big-endian bytes of `id`.
//
// We drive the REAL StreamCodec (encode + round-trip decode) and dump:
//   PONG_SB <id> <readableBytes> <hex-of-every-byte> <decodedId>
//
// The C++ pkt_pong_sb_parity encodes the same `id` via PacketBuffer.writeInt and
// must produce identical bytes (and decode them back to the same id).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ServerboundPongPacket;

public class PktPongSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundPongPacket> codec = ServerboundPongPacket.STREAM_CODEC;

        // Finite/physical inputs: zero, small, byte/short boundaries, large, signed extremes.
        int[] ids = {
            0, 1, 2, 7, 42, 127, 128, 255, 256, 1000, 32767, 32768, 65535, 65536,
            16777215, 16777216, 1234567, -1, -2, -128, -255, -256, -32768, -65536,
            -1234567890, 2147483647, -2147483648
        };

        for (int id : ids) {
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, new ServerboundPongPacket(id));

            // Snapshot the bytes (do not advance the original reader index).
            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte() & 0xff));

            // Round-trip decode through the REAL codec.
            ServerboundPongPacket back = codec.decode(buf);
            int decodedId = back.getId();

            O.printf("PONG_SB\t%d\t%d\t%s\t%d%n", id, n, hex.toString(), decodedId);
        }
    }
}
