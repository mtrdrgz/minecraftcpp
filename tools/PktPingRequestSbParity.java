// Ground truth for net.minecraft.network.protocol.ping.ServerboundPingRequestPacket.
//
// The packet's STREAM_CODEC body is exactly `output.writeLong(this.time)` (8 bytes,
// big-endian — see ServerboundPingRequestPacket.write(ByteBuf)). We encode each time
// value through the REAL STREAM_CODEC into a fresh FriendlyByteBuf and dump
// readableBytes() + the hex of every byte. We also decode the bytes back through the
// SAME STREAM_CODEC and confirm getTime() round-trips (sanity).
//
// Row format (tab-separated, emitted to STDOUT):
//   ENC  <time-decimal>  <readableBytes>  <hex-of-all-bytes>
//
// Notes:
//   * STREAM_CODEC is declared StreamCodec<ByteBuf, ServerboundPingRequestPacket> and is
//     public static final; we drive it directly. FriendlyByteBuf IS a ByteBuf, so it is a
//     valid encode/decode target.
//   * The public ctor ServerboundPingRequestPacket(long) and getTime() are public — no
//     reflection on private members is needed.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.ping.ServerboundPingRequestPacket;

public class PktPingRequestSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The public StreamCodec<ByteBuf, ServerboundPingRequestPacket>.
        StreamCodec<ByteBuf, ServerboundPingRequestPacket> codec =
            ServerboundPingRequestPacket.STREAM_CODEC;

        // FINITE / PHYSICAL longs: zero, small positives, byte/short/int boundaries,
        // negatives, bit-pattern edges, Long extremes, and a couple of realistic
        // millisecond timestamps (the ping request carries a client-side millis stamp).
        long[] times = {
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
            1717977600000L,         // a plausible client millis value
            42L
        };

        for (long time : times) {
            ServerboundPingRequestPacket pkt = new ServerboundPingRequestPacket(time);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);

            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            // Read all bytes WITHOUT consuming the decode path: use getByte by index.
            int base = buf.readerIndex();
            for (int i = 0; i < n; i++) {
                hex.append(String.format("%02x", buf.getByte(base + i) & 0xFF));
            }

            // Round-trip decode through the same codec and confirm time survives.
            ServerboundPingRequestPacket back = codec.decode(buf);
            if (back.getTime() != time) {
                throw new IllegalStateException("round-trip mismatch: " + time + " -> " + back.getTime());
            }

            O.print("ENC\t");
            O.print(time);
            O.print('\t');
            O.print(n);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
