// Ground truth for ClientboundSetChunkCacheCenterPacket's StreamCodec.
//
// The packet body is exactly (net.minecraft.network.protocol.game.
// ClientboundSetChunkCacheCenterPacket lines 20-28, Packet.codec -> no id prefix):
//   write : output.writeVarInt(this.x); output.writeVarInt(this.z);
//   read  : this.x = input.readVarInt(); this.z = input.readVarInt();
// writeVarInt is LEB128 over the int's unsigned 32-bit bit pattern, so negatives
// always encode to a full 5 bytes. We round-trip a battery of (x,z) chunk-coord
// ints that pin each VarInt size boundary (1->2->3->4->5 bytes), the sign cases,
// and the int extremes through the REAL STREAM_CODEC.
//
// Row format (tab separated):
//   ENC <name> <x> <z> <readableBytes> <hexBytes>
// where x,z are decimal ints and hexBytes is the full encoded body as lowercase
// %02x. The decode-equality (sanity) round-trip is asserted in-tool below; a
// mismatch aborts with a thrown exception so no bad row is ever emitted.
//
// The C++ side writes writeVarInt(x), writeVarInt(z) through the certified
// PacketBuffer and must match byte-for-byte and readableBytes-for-readableBytes,
// then decode the expected bytes back to (x,z) and round-trip.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetChunkCacheCenterPacket;

public class PktSetChunkCenterParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ClientboundSetChunkCacheCenterPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetChunkCacheCenterPacket>)
                ClientboundSetChunkCacheCenterPacket.STREAM_CODEC;

        // Finite/physical inputs. Each value is a chunk coordinate (block>>4).
        // The values pin VarInt size boundaries (127/128, 16383/16384,
        // 2097151/2097152, 268435455/268435456) plus 0, small, sign, and the int
        // extremes (which always encode to 5 bytes through the unsigned LEB128).
        int[] vals = {
            0, 1, 2, 7, 8, 16, 31, 32,
            127, 128, 129,
            16383, 16384, 16385,
            2097151, 2097152, 2097153,
            268435455, 268435456, 268435457,
            -1, -2, -8, -127, -128, -129, -1875000, 1875000,
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        for (int x : vals) {
            for (int z : vals) {
                emit(CODEC, x, z);
            }
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundSetChunkCacheCenterPacket> CODEC,
                     int x, int z) {
        // ENC: encode through the real codec, dump the body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSetChunkCacheCenterPacket pkt = new ClientboundSetChunkCacheCenterPacket(x, z);
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: decode the same bytes back through the SAME codec; the fields
        // must round-trip exactly. Abort on any drift so no bad row is emitted.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundSetChunkCacheCenterPacket dec = CODEC.decode(rbuf);
        if (dec.getX() != x || dec.getZ() != z) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + x + "," + z + ") out=("
                    + dec.getX() + "," + dec.getZ() + ")");
        }

        O.print("ENC\t");
        O.print("set_chunk_center");
        O.print('\t');
        O.print(x);
        O.print('\t');
        O.print(z);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
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
