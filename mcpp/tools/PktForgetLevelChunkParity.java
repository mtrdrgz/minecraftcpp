// Ground truth for net.minecraft.network.protocol.game.ClientboundForgetLevelChunkPacket.
//
// The packet body is exactly one ChunkPos (ClientboundForgetLevelChunkPacket lines
// 9-20 in 26.1.2/src; Packet.codec -> StreamCodec.ofMember: body only, no packet-id
// prefix):
//
//     private ClientboundForgetLevelChunkPacket(final FriendlyByteBuf input) {
//        this(input.readChunkPos());
//     }
//     private void write(final FriendlyByteBuf output) {
//        output.writeChunkPos(this.pos);
//     }
//
// FriendlyByteBuf.writeChunkPos(pos) == writeLong(pos.pack()), and (ChunkPos.java
// lines 76-78):
//
//     public static long pack(int x, int z) {
//        return x & 4294967295L | (z & 4294967295L) << 32;
//     }
//
// So the wire is one big-endian 8-byte long whose high 32 bits are z (unsigned) and
// low 32 bits are x (unsigned). writeLong is big-endian, so the bytes are z (4 bytes
// BE) followed by x (4 bytes BE). readChunkPos == ChunkPos.unpack(readLong()):
// x = (int)key, z = (int)(key >> 32) -- a plain narrowing cast (NOT >>>), so the sign
// bits of x and z are preserved exactly and negative coords round-trip.
//
// Row format (tab separated):
//   ENC <x> <z> <readableBytes> <hexBytes>   encode the real packet -> body bytes
// where x,z are decimal ints and hexBytes is the full encoded body as lowercase %02x.
// The decode-equality round-trip is asserted in-tool below; a mismatch aborts with a
// thrown exception so no bad row is ever emitted.
//
// The C++ pkt_forget_level_chunk_parity rebuilds the body via PacketBuffer.writeLong
// of the packed key and must match <hex> + readableBytes byte-for-byte, then decode
// the bytes back via readLong()/unpack and require (x,z) round-trip exactly.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundForgetLevelChunkPacket;
import net.minecraft.world.level.ChunkPos;

public class PktForgetLevelChunkParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundForgetLevelChunkPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundForgetLevelChunkPacket>)
                ClientboundForgetLevelChunkPacket.STREAM_CODEC;

        // Finite/physical battery of chunk coordinates. Each int is a chunk coord
        // (block>>4). Because pack() masks to 32 bits and unpack() narrow-casts back,
        // the int extremes and the sign bits must round-trip exactly. Values pin 0,
        // small, byte-boundary, large magnitude, sign, and the int extremes.
        int[] vals = {
            0, 1, 2, 7, 8, 16, 31, 32,
            127, 128, 255, 256,
            32767, 32768, 65535, 65536,
            8388607, 8388608, 16777215, 16777216,
            1875000, 1875066,
            -1, -2, -8, -127, -128, -256, -65536, -1875000,
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        for (int x : vals) {
            for (int z : vals) {
                emit(CODEC, x, z);
            }
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundForgetLevelChunkPacket> CODEC,
                     int x, int z) {
        // Build the REAL packet via the public canonical record ctor.
        ClientboundForgetLevelChunkPacket pkt =
            new ClientboundForgetLevelChunkPacket(new ChunkPos(x, z));

        // ENC: encode through the real codec, dump the body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: decode the same bytes back through the SAME codec; the ChunkPos
        // must round-trip exactly. Abort on any drift so no bad row is emitted.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundForgetLevelChunkPacket dec = CODEC.decode(rbuf);
        if (dec.pos().x() != x || dec.pos().z() != z) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + x + "," + z + ") out=("
                    + dec.pos().x() + "," + dec.pos().z() + ")");
        }

        O.print("ENC\t");
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
