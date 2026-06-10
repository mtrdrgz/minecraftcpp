// Ground truth for net.minecraft.network.protocol.game.ClientboundBlockDestructionPacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet body is exactly (ClientboundBlockDestructionPacket.java:29-33):
//   write(FriendlyByteBuf output):
//     output.writeVarInt(this.id);          // VarInt entity/destruction id
//     output.writeBlockPos(this.pos);       // == output.writeLong(pos.asLong())  (FriendlyByteBuf.java:398-400)
//     output.writeByte(this.progress);      // low 8 bits of the int progress
//   read(FriendlyByteBuf input)  (ClientboundBlockDestructionPacket.java:23-27):
//     this.id       = input.readVarInt();
//     this.pos      = input.readBlockPos();        // == BlockPos.of(input.readLong())  (FriendlyByteBuf.java:389-391)
//     this.progress = input.readUnsignedByte();    // 0..255
//
// Packet.codec -> StreamCodec.ofMember (Packet.java:22-24): body only, NO packet-id
// or length prefix on the wire.
//
// BlockPos.asLong (BlockPos.java:107-116) packs a single big-endian long:
//   PACKED_HORIZONTAL_LENGTH = 26, PACKED_Y_LENGTH = 12,
//   X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0;
//   node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
// writeLong is big-endian 8 bytes (FriendlyByteBuf -> netty).
//
// Row formats (tab separated). Every field that is not a String/binary is decimal;
// hexBytes is lowercase %02x of the encoded body.
//   ENC \t <name> \t <id> \t <x> \t <y> \t <z> \t <progress> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeVarInt(id) + writeLong(asLong(x,y,z)) + writeByte(progress)) and must match
// byte-for-byte, then round-trips the bytes back to (id,x,y,z,progress).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundBlockDestructionPacket;

public class PktBlockDestructionParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundBlockDestructionPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundBlockDestructionPacket>)
                ClientboundBlockDestructionPacket.STREAM_CODEC;

        // Finite / physical battery. Columns: name, id, x, y, z, progress.
        // id exercises VarInt 1->2->3->4->5 byte boundaries and sign.
        // x/z are 26-bit signed (range -33554432..33554431), y is 12-bit
        // (BlockPos sign-extends 12 bits: -2048..2047). progress is an int whose
        // low byte is written; readUnsignedByte yields 0..255.
        int[][] cases = {
            // {id, x, y, z, progress}
            {0, 0, 0, 0, 0},
            {1, 1, 1, 1, 1},
            {127, 100, 64, -100, 9},
            {128, -1, -1, -1, 255},
            {16383, 12345678, 320, -8765432, 7},
            {16384, -30000000, -64, 30000000, 200},
            {2097151, 30000000, 2047, -30000000, 128},
            {2097152, -33554432, -2048, 33554431, 1},
            {268435455, 33554431, 2047, -33554432, 254},
            {2147483647, 0, 0, 0, 100},   // Integer.MAX_VALUE id (5-byte VarInt)
            {-1, -33554432, -2048, -33554432, 0},  // id=-1 -> 5-byte VarInt 0xff*4 0x0f
            {-2147483648, 1, -1, 2, 127}, // Integer.MIN_VALUE id (5-byte VarInt)
            {42, 8388607, 100, -8388608, 256}, // progress 256 -> low byte 0x00
            {77, 250, 5, 250, 511},            // progress 511 -> low byte 0xff
        };

        for (int[] c : cases) {
            int id = c[0], x = c[1], y = c[2], z = c[3], progress = c[4];
            BlockPos pos = new BlockPos(x, y, z);

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ClientboundBlockDestructionPacket pkt =
                new ClientboundBlockDestructionPacket(id, pos, progress);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; sanity-assert field equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundBlockDestructionPacket dec = CODEC.decode(rbuf);
            // id round-trips exactly; pos round-trips through asLong/of (12-bit y
            // sign-extends, 26-bit x/z sign-extend); progress comes back as the
            // unsigned low byte (0..255).
            if (dec.getId() != id)
                throw new IllegalStateException("id roundtrip " + dec.getId() + " != " + id);
            long expLong = BlockPos.asLong(x, y, z);
            if (dec.getPos().asLong() != expLong)
                throw new IllegalStateException("pos roundtrip " + dec.getPos().asLong()
                    + " != " + expLong);
            if (dec.getProgress() != (progress & 0xff))
                throw new IllegalStateException("progress roundtrip " + dec.getProgress()
                    + " != " + (progress & 0xff));

            String name = "case_id" + id + "_x" + x + "_y" + y + "_z" + z + "_p" + progress;
            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(id);
            O.print('\t');
            O.print(x);
            O.print('\t');
            O.print(y);
            O.print('\t');
            O.print(z);
            O.print('\t');
            O.print(progress);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
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
