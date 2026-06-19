// Ground truth for net.minecraft.network.protocol.common.ServerboundKeepAlivePacket.
//
// The packet's STREAM_CODEC body is exactly `output.writeLong(this.id)` (8 bytes,
// big-endian). We encode each id through the REAL STREAM_CODEC into a fresh
// FriendlyByteBuf and dump readableBytes() + the hex of every byte. We also decode
// the bytes back through the SAME STREAM_CODEC and confirm getId() round-trips.
//
// Row format (tab-separated, emitted to STDOUT):
//   ENC  <id-decimal>  <readableBytes>  <hex-of-all-bytes>
//
// The constructor ServerboundKeepAlivePacket(FriendlyByteBuf) and write(FriendlyByteBuf)
// are private, so the STREAM_CODEC (built via Packet.codec(::write, ::new)) is the only
// public encode/decode path — we drive it directly, no reflection on private members
// needed for the codec itself (the codec field STREAM_CODEC is public static final).
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ServerboundKeepAlivePacket;

public class PktKeepAliveSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The public StreamCodec<FriendlyByteBuf, ServerboundKeepAlivePacket>.
        StreamCodec<FriendlyByteBuf, ServerboundKeepAlivePacket> codec =
            ServerboundKeepAlivePacket.STREAM_CODEC;

        // FINITE / PHYSICAL longs: zero, small positives, boundaries, negatives,
        // bit-pattern edges, and a couple of realistic millisecond timestamps
        // (the server seeds keep-alive ids with Util.getMillis()).
        long[] ids = {
            0L,
            1L,
            2L,
            127L,
            128L,
            255L,
            256L,
            32767L,
            32768L,
            65535L,
            65536L,
            2147483647L,            // Integer.MAX_VALUE
            2147483648L,
            4294967295L,            // 0xFFFFFFFF
            4294967296L,
            9223372036854775807L,   // Long.MAX_VALUE
            -1L,
            -2L,
            -128L,
            -129L,
            -2147483648L,           // Integer.MIN_VALUE
            -9223372036854775808L,  // Long.MIN_VALUE
            0x0123456789ABCDEFL,
            (long) 0xDEADBEEFCAFEBABEL,
            1234567890123456789L,
            -1234567890123456789L,
            1717977600000L,         // a plausible Util.getMillis() value
            42L
        };

        for (long id : ids) {
            ServerboundKeepAlivePacket pkt = new ServerboundKeepAlivePacket(id);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);

            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            // Read all bytes WITHOUT consuming the decode path: use getByte by index.
            int base = buf.readerIndex();
            for (int i = 0; i < n; i++) {
                hex.append(String.format("%02x", buf.getByte(base + i) & 0xFF));
            }

            // Round-trip decode through the same codec and confirm id survives.
            ServerboundKeepAlivePacket back = codec.decode(buf);
            if (back.getId() != id) {
                throw new IllegalStateException("round-trip mismatch: " + id + " -> " + back.getId());
            }

            O.print("ENC\t");
            O.print(id);
            O.print('\t');
            O.print(n);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
