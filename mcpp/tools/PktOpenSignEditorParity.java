// Ground truth for net.minecraft.network.protocol.game.ClientboundOpenSignEditorPacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet body is exactly (ClientboundOpenSignEditorPacket.java:26-29):
//   write(FriendlyByteBuf output):
//     output.writeBlockPos(this.pos);        // == output.writeLong(pos.asLong())  (FriendlyByteBuf.java:398-400)
//     output.writeBoolean(this.isFrontText); // single byte 0/1
//   read(FriendlyByteBuf input)  (ClientboundOpenSignEditorPacket.java:21-24):
//     this.pos         = input.readBlockPos();   // == BlockPos.of(input.readLong())  (FriendlyByteBuf.java:389-391)
//     this.isFrontText = input.readBoolean();
//
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, NO packet-id or
// length prefix on the wire.
//
// BlockPos.asLong (BlockPos.java) packs a single big-endian long:
//   PACKED_HORIZONTAL_LENGTH = 26, PACKED_Y_LENGTH = 12,
//   X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0;
//   node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
// writeLong is big-endian 8 bytes (FriendlyByteBuf -> netty).
//
// Row formats (tab separated). Every field that is not a String/binary is decimal;
// hexBytes is lowercase %02x of the encoded body.
//   ENC \t <name> \t <x> \t <y> \t <z> \t <isFrontText> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeLong(asLong(x,y,z)) + writeBool(isFrontText)) and must match byte-for-byte,
// then round-trips the bytes back to (x,y,z,isFrontText).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundOpenSignEditorPacket;

public class PktOpenSignEditorParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundOpenSignEditorPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundOpenSignEditorPacket>)
                ClientboundOpenSignEditorPacket.STREAM_CODEC;

        // Finite / physical battery. Columns: name, x, y, z, isFrontText.
        // x/z are 26-bit signed (range -33554432..33554431), y is 12-bit
        // (BlockPos sign-extends 12 bits: -2048..2047). isFrontText is a bool.
        // Cases exercise zero, small, sign, and the BlockPos packing extremes,
        // each with both boolean states.
        Object[][] cases = {
            // {x, y, z, isFrontText}
            {0, 0, 0, false},
            {0, 0, 0, true},
            {1, 1, 1, true},
            {-1, -1, -1, false},
            {100, 64, -100, true},
            {12345678, 320, -8765432, false},
            {-30000000, -64, 30000000, true},
            {30000000, 2047, -30000000, false},
            {-33554432, -2048, 33554431, true},
            {33554431, 2047, -33554432, false},
            {-33554432, -2048, -33554432, true},
            {8388607, 100, -8388608, false},
            {8388607, 100, -8388608, true},
        };

        for (Object[] c : cases) {
            int x = (Integer) c[0], y = (Integer) c[1], z = (Integer) c[2];
            boolean isFrontText = (Boolean) c[3];
            BlockPos pos = new BlockPos(x, y, z);

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ClientboundOpenSignEditorPacket pkt =
                new ClientboundOpenSignEditorPacket(pos, isFrontText);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; sanity-assert field equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundOpenSignEditorPacket dec = CODEC.decode(rbuf);
            // pos round-trips through asLong/of (12-bit y sign-extends, 26-bit x/z
            // sign-extend); isFrontText round-trips exactly.
            long expLong = BlockPos.asLong(x, y, z);
            if (dec.getPos().asLong() != expLong)
                throw new IllegalStateException("pos roundtrip " + dec.getPos().asLong()
                    + " != " + expLong);
            if (dec.isFrontText() != isFrontText)
                throw new IllegalStateException("isFrontText roundtrip " + dec.isFrontText()
                    + " != " + isFrontText);

            String name = "case_x" + x + "_y" + y + "_z" + z + "_f" + isFrontText;
            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(x);
            O.print('\t');
            O.print(y);
            O.print('\t');
            O.print(z);
            O.print('\t');
            O.print(isFrontText ? 1 : 0);
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
