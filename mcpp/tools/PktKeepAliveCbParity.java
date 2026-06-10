// Ground truth for net.minecraft.network.protocol.common.ClientboundKeepAlivePacket.
//
// The packet's STREAM_CODEC writes a single long id via FriendlyByteBuf.writeLong
// (-> netty ByteBuf.writeLong, big-endian 8 bytes) and reads it back via readLong.
// We drive the REAL STREAM_CODEC: encode the packet into a fresh Unpooled buffer,
// dump readableBytes() + the hex of every byte; then round-trip decode and dump
// getId() so the C++ side can verify both directions byte-for-byte.
//
// Row format:
//   PKT \t <id-decimal> \t <readableBytes-decimal> \t <hex> \t <decodedId-decimal>
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ClientboundKeepAlivePacket;

public class PktKeepAliveCbParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ClientboundKeepAlivePacket> codec =
                (StreamCodec<FriendlyByteBuf, ClientboundKeepAlivePacket>) ClientboundKeepAlivePacket.STREAM_CODEC;

        long[] ids = {
            0L, 1L, 2L, 127L, 128L, 255L, 256L, 65535L, 65536L,
            -1L, -2L, -256L, 1234567890123456789L, -1234567890123456789L,
            Long.MAX_VALUE, Long.MIN_VALUE,
            0x0102030405060708L, 0x00000000FFFFFFFFL, 0xFFFFFFFF00000000L,
            42L, -42L, 9223372036854775807L
        };

        for (long id : ids) {
            // ENCODE via the real STREAM_CODEC into a fresh buffer
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ClientboundKeepAlivePacket pkt = new ClientboundKeepAlivePacket(id);
            codec.encode(buf, pkt);

            int readable = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            // peek without consuming so we can decode afterward
            for (int i = 0; i < readable; i++) {
                hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));
            }

            // ROUND-TRIP DECODE via the real STREAM_CODEC
            ClientboundKeepAlivePacket dec = codec.decode(buf);
            long decodedId = dec.getId();

            O.print("PKT\t");
            O.print(id);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\t');
            O.print(decodedId);
            O.print('\n');
        }
    }
}
