// Ground truth for net.minecraft.network.protocol.login.ClientboundLoginCompressionPacket.
//
// The packet body is a single int `compressionThreshold` written via
// FriendlyByteBuf.writeVarInt (LEB128) per the class's
//   STREAM_CODEC = Packet.codec(ClientboundLoginCompressionPacket::write,
//                               ClientboundLoginCompressionPacket::new)
// where write() does output.writeVarInt(this.compressionThreshold) and the
// private ctor does input.readVarInt(). We encode each value through the REAL
// StreamCodec into a fresh FriendlyByteBuf, dump readableBytes() + the body hex,
// and round-trip decode through the SAME codec as a sanity check. The C++
// PacketBuffer.writeVarInt must reproduce identical bytes.
//
//   LOGIN_COMPRESSION_CB <threshold-dec> <readableBytes> <hex>
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.login.ClientboundLoginCompressionPacket;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktLoginCompressionCbParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundLoginCompressionPacket> codec =
            ClientboundLoginCompressionPacket.STREAM_CODEC;

        // Finite/physical battery: VarInt 1->2->3->4->5 byte boundaries, sign,
        // max/min, and the canonical "disable compression" value -1.
        int[] thresholds = {
            0, 1, 2, 127, 128, 255, 256,
            16383, 16384, 16385,
            2097151, 2097152, 2097153,
            268435455, 268435456,
            32767, 32768, 65535, 65536,
            16777215, 16777216,
            256, 1000000, 305419896 /* 0x12345678 */,
            2147483647, -1, -2, -256, -65536, -2147483648
        };

        for (int threshold : thresholds) {
            ClientboundLoginCompressionPacket pkt =
                new ClientboundLoginCompressionPacket(threshold);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);

            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.readByte()));

            // sanity: decode round trip via the real codec
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.buffer());
            for (int i = 0; i < n; i++)
                rbuf.writeByte(Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16));
            ClientboundLoginCompressionPacket back = codec.decode(rbuf);
            if (back.getCompressionThreshold() != threshold)
                throw new IllegalStateException("decode mismatch " + threshold);

            O.print("LOGIN_COMPRESSION_CB\t" + threshold + "\t" + n + "\t" + hex + "\n");
        }
    }
}
