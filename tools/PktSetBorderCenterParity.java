// Ground truth for net.minecraft.network.protocol.game.ClientboundSetBorderCenterPacket's
// StreamCodec.
//
// ClientboundSetBorderCenterPacket (ClientboundSetBorderCenterPacket.java:9-47) carries:
//   private final double newCenterX;
//   private final double newCenterZ;
// Its STREAM_CODEC is Packet.codec(ClientboundSetBorderCenterPacket::write,
// ClientboundSetBorderCenterPacket::new) -- a plain body codec, NO packet-id prefix.
// The write() method (ClientboundSetBorderCenterPacket.java:26-29) emits, IN THIS
// EXACT ORDER:
//   output.writeDouble(this.newCenterX)   // big-endian IEEE-754, 8 bytes
//   output.writeDouble(this.newCenterZ)   // big-endian IEEE-754, 8 bytes
// and the read ctor (ClientboundSetBorderCenterPacket.java:21-24) reads in the same
// order:
//   this.newCenterX = input.readDouble();
//   this.newCenterZ = input.readDouble();
// So the on-wire body is exactly: double(newCenterX) ++ double(newCenterZ),
// 16 bytes total, no packet-id prefix.
//
// Both fields are plain primitives (double) -- no registry/ItemStack/Component/Holder/
// SoundEvent/NBT -- fully representable by the certified PacketBuffer (FriendlyByteBuf)
// port. We construct the packet via reflection on the public-WorldBorder ctor's private
// twin... actually the only ctor that pins arbitrary doubles is the private
// ClientboundSetBorderCenterPacket(FriendlyByteBuf) read ctor; we instead drive the
// fields directly by constructing a real WorldBorder and setting its center, then
// reading back -- but WorldBorder.setCenter clamps coords. To pin ARBITRARY (incl.
// special IEEE-754) doubles exactly, we build the packet by decoding a hand-built
// FriendlyByteBuf carrying the two doubles through the REAL STREAM_CODEC, then re-encode
// it through the REAL STREAM_CODEC and verify the bytes round-trip. This exercises both
// the real read ctor and the real write() with no clamping.
//
// Row formats (tab separated). newCenterX / newCenterZ are emitted as %016x of their
// raw long bits (Double.doubleToRawLongBits) so NaN/Inf/-0.0 survive exactly:
//   ENC <xBitsHex> <zBitsHex> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <xBits_in> <zBits_in> <xBits_dec> <zBits_dec>
//        decode: STREAM_CODEC.decode(buf) -> field round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetBorderCenterPacket;

public class PktSetBorderCenterParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, ClientboundSetBorderCenterPacket).
        StreamCodec<FriendlyByteBuf, ClientboundSetBorderCenterPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetBorderCenterPacket>)
                ClientboundSetBorderCenterPacket.STREAM_CODEC;

        // Finite/physical battery of doubles: world-coordinate range, sign / zero /
        // boundary / special IEEE-754 doubles to exercise the full 8-byte encoding.
        double[] coords = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
            64.0, 128.5, 256.25, -256.25, 1000.0, -1000.0,
            3.141592653589793, 2.718281828459045,
            30000000.0, -30000000.0,              // world border extents
            29999984.0, -29999984.0,              // max usable border center
            12345.678901234, -98765.432109876,
            Double.MIN_VALUE, Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.NaN,
        };

        // Pair the coord list index-wise across the X / Z slots (rotated) so every double
        // bit pattern lands in each slot at least once.
        int n = coords.length;
        for (int i = 0; i < n; i++) {
            double x = coords[i];
            double z = coords[(n - 1 - i)];

            // Build the REAL packet by decoding a hand-built body through the REAL codec
            // (this drives the real private read ctor with arbitrary, un-clamped doubles).
            ClientboundSetBorderCenterPacket pkt = makePacket(CODEC, x, z);

            // ENC: encode through the REAL codec, dump readableBytes + body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);
            int rn = buf.readableBytes();
            String hex = toHex(buf);
            long xBits = Double.doubleToRawLongBits(x);
            long zBits = Double.doubleToRawLongBits(z);
            O.print("ENC\t");
            O.print(String.format("%016x", xBits));
            O.print('\t');
            O.print(String.format("%016x", zBits));
            O.print('\t');
            O.print(rn);
            O.print('\t');
            O.print(hex);
            O.print('\n');

            // DEC: decode the same bytes through the REAL codec; round-trip sanity.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundSetBorderCenterPacket dec = CODEC.decode(rbuf);
            long dxBits = Double.doubleToRawLongBits(dec.getNewCenterX());
            long dzBits = Double.doubleToRawLongBits(dec.getNewCenterZ());
            if (dxBits != xBits || dzBits != zBits)
                throw new IllegalStateException("round-trip mismatch for " + hex);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(String.format("%016x", xBits));
            O.print('\t');
            O.print(String.format("%016x", zBits));
            O.print('\t');
            O.print(String.format("%016x", dxBits));
            O.print('\t');
            O.print(String.format("%016x", dzBits));
            O.print('\n');
        }
    }

    // Build a ClientboundSetBorderCenterPacket carrying the exact doubles x,z by feeding a
    // hand-built body (two big-endian doubles) through the REAL STREAM_CODEC's decoder,
    // which invokes the packet's private FriendlyByteBuf read ctor. No clamping.
    static ClientboundSetBorderCenterPacket makePacket(
            StreamCodec<FriendlyByteBuf, ClientboundSetBorderCenterPacket> codec,
            double x, double z) throws Exception {
        FriendlyByteBuf body = new FriendlyByteBuf(Unpooled.buffer());
        body.writeDouble(x);
        body.writeDouble(z);
        return codec.decode(body);
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
