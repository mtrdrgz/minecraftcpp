// Ground truth for net.minecraft.network.protocol.ping.ClientboundPongResponsePacket.
//
// The packet is `public record ClientboundPongResponsePacket(long time)`. Its
// STREAM_CODEC (Packet.codec(::write, ::new)) body is exactly:
//     private void write(FriendlyByteBuf output) { output.writeLong(this.time); }
//     private ClientboundPongResponsePacket(FriendlyByteBuf input) { this(input.readLong()); }
// i.e. a single 8-byte big-endian long (FriendlyByteBuf.writeLong -> netty
// ByteBuf.writeLong, big-endian). No packet-type id is part of the STREAM_CODEC;
// that framing is added separately. So the encoded body is exactly the 8 big-endian
// bytes of `time`.
//
// We drive the REAL StreamCodec (encode + round-trip decode) and dump every byte:
//   ENC  <time-decimal>  <readableBytes>  <hex-of-all-bytes>
//
// The C++ pkt_pong_response_cb_parity encodes the same `time` via
// PacketBuffer.writeLong and must produce byte-for-byte identical bytes, report the
// same readableBytes, and round-trip-decode them back to the same time.
//
// write(FriendlyByteBuf) and the FriendlyByteBuf constructor are private, so the
// public STREAM_CODEC is the only encode/decode path — we drive it directly (the
// STREAM_CODEC field is public static final; no reflection / Unsafe needed).
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.ping.ClientboundPongResponsePacket;

public class PktPongResponseCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The public StreamCodec<FriendlyByteBuf, ClientboundPongResponsePacket>.
        StreamCodec<FriendlyByteBuf, ClientboundPongResponsePacket> codec =
            ClientboundPongResponsePacket.STREAM_CODEC;

        // FINITE / PHYSICAL longs: zero, small positives, fixed-width boundaries,
        // negatives, signed extremes, and bit-pattern edges. `time` echoes the
        // client ping payload (typically Util.getMillis()), so a couple of plausible
        // millisecond timestamps are included too.
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
            16777215L,
            16777216L,
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

        for (long time : times) {
            ClientboundPongResponsePacket pkt = new ClientboundPongResponsePacket(time);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);

            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            // Snapshot every byte WITHOUT consuming the decode path: read by index.
            int base = buf.readerIndex();
            for (int i = 0; i < n; i++) {
                hex.append(String.format("%02x", buf.getByte(base + i) & 0xFF));
            }

            // Round-trip decode through the SAME codec and confirm time survives.
            ClientboundPongResponsePacket back = codec.decode(buf);
            if (back.time() != time) {
                throw new IllegalStateException("round-trip mismatch: " + time + " -> " + back.time());
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
