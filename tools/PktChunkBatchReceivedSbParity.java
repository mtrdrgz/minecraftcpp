// Ground truth for net.minecraft.network.protocol.game.ServerboundChunkBatchReceivedPacket's
// StreamCodec.
//
// The packet (ServerboundChunkBatchReceivedPacket.java:8-19) is a record holding:
//     float desiredChunksPerTick
// Its STREAM_CODEC is Packet.codec(write, new) and write(FriendlyByteBuf) is, in this exact
// wire order (ServerboundChunkBatchReceivedPacket.java:17-19):
//     output.writeFloat(this.desiredChunksPerTick); -> big-endian IEEE-754 4-byte float
// and the FriendlyByteBuf decode ctor reads it back in the same order
// (ServerboundChunkBatchReceivedPacket.java:13-15): readFloat().
// Packet.codec -> no packet-id prefix, just the body.
//
// The only field is a plain float: no registry/ItemStack/Component/Holder/NBT, so the body
// is fully representable by the certified PacketBuffer (FriendlyByteBuf) port (writeFloat).
//
// Row format (tab separated):
//   ENC <chunksBits> <readableBytes> <hexBytes>
//     desiredChunksPerTick as %08x of its raw int bits (Float.floatToRawIntBits) so
//     NaN/Inf/-0.0 survive exactly without parse rounding; hex columns lowercase. encode
//     through the REAL codec, dump readableBytes + every byte, then decode the same bytes
//     back through the SAME codec for a round-trip sanity check (abort on any drift).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundChunkBatchReceivedPacket;

public class PktChunkBatchReceivedSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundChunkBatchReceivedPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundChunkBatchReceivedPacket>)
                ServerboundChunkBatchReceivedPacket.STREAM_CODEC;

        // Finite/physical desiredChunksPerTick battery (floats): zero/sign, unit, the small
        // fractional values the client actually sends (it is a per-tick chunk rate, typically
        // in the ~0.01..tens range), world-scale magnitudes, plus IEEE-754 specials to
        // exercise the full 4-byte big-endian encoding.
        float[] rates = {
            0.0f, -0.0f, 1.0f, -1.0f,
            0.01f, 0.1f, -0.1f, 0.25f, 0.5f, -0.5f, 0.0625f, 0.07f,
            2.0f, 2.5f, -2.5f, 7.5f, 16.0f, 64.0f, 100.5f, -100.5f,
            1234.5678f, -1234.5678f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };

        for (float r : rates)
            emit(CODEC, r);
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundChunkBatchReceivedPacket> CODEC,
                     float rate) {
        // Build the packet via its public (float desiredChunksPerTick) record ctor.
        ServerboundChunkBatchReceivedPacket pkt =
            new ServerboundChunkBatchReceivedPacket(rate);

        // ENC: encode through the REAL codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int n = buf.readableBytes();
        String hex = toHex(buf);

        int rateBits = Float.floatToRawIntBits(rate);

        O.print("ENC\t");
        O.print(String.format("%08x", rateBits));
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex);
        O.print('\n');

        // Round-trip decode through the SAME codec; abort on any drift so no bad row is ever
        // emitted.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundChunkBatchReceivedPacket dec = CODEC.decode(rbuf);
        int decRateBits = Float.floatToRawIntBits(dec.desiredChunksPerTick());
        if (decRateBits != rateBits) {
            throw new IllegalStateException(
                "round-trip mismatch: in=" + String.format("%08x", rateBits)
                    + " out=" + String.format("%08x", decRateBits));
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
