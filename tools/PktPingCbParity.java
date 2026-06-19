// Ground truth for net.minecraft.network.protocol.common.ClientboundPingPacket.
//
// The packet body is a single int `id` written via FriendlyByteBuf.writeInt
// (netty big-endian, fixed 4 bytes) per the class's STREAM_CODEC = Packet.codec(
// ClientboundPingPacket::write, ClientboundPingPacket::new), where write() does
// output.writeInt(this.id). We encode through the REAL StreamCodec into a fresh
// FriendlyByteBuf and dump readableBytes() + the body hex. The C++ PacketBuffer
// must reproduce identical bytes.
//
//   PING_CB <id> <readableBytes> <hex>
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ClientboundPingPacket;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktPingCbParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundPingPacket> codec = ClientboundPingPacket.STREAM_CODEC;

        int[] ids = {
            0, 1, 2, 127, 128, 255, 256, 1000, 32767, 32768, 65535, 65536,
            16777215, 16777216, 2147483647, -1, -2, -256, -65536, -2147483648,
            305419896 /* 0x12345678 */, 1000000
        };

        for (int id : ids) {
            ClientboundPingPacket pkt = new ClientboundPingPacket(id);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);

            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.readByte()));

            // sanity: decode round trip via the real codec
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.buffer());
            for (int i = 0; i < n; i++) rbuf.writeByte(Integer.parseInt(hex.substring(i * 2, i * 2 + 2), 16));
            ClientboundPingPacket back = codec.decode(rbuf);
            if (back.getId() != id) throw new IllegalStateException("decode mismatch " + id);

            O.print("PING_CB\t" + id + "\t" + n + "\t" + hex + "\n");
        }
    }
}
