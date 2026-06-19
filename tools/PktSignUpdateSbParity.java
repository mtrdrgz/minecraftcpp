// Ground truth for net.minecraft.network.protocol.game.ServerboundSignUpdatePacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet body is exactly (ServerboundSignUpdatePacket.java:26-43):
//   write(FriendlyByteBuf output):
//     output.writeBlockPos(this.pos);          // == output.writeLong(pos.asLong())  (FriendlyByteBuf.java:398-400)
//     output.writeBoolean(this.isFrontText);   // single byte 0/1
//     for (int i = 0; i < 4; i++)
//         output.writeUtf(this.lines[i]);      // VarInt byte-len + UTF-8 (default maxLen 32767)
//   read(FriendlyByteBuf input)  (ServerboundSignUpdatePacket.java:26-34):
//     this.pos         = input.readBlockPos();   // == BlockPos.of(input.readLong())  (FriendlyByteBuf.java:389-391)
//     this.isFrontText = input.readBoolean();
//     for (int i = 0; i < 4; i++)
//         this.lines[i] = input.readUtf(384);    // MAX_STRING_LENGTH = 384 on read
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
// Utf8String.write (FriendlyByteBuf.writeUtf): VarInt(byte length) + UTF-8 bytes.
// Note write() uses the default maxLen 32767 while read() caps at 384; all battery
// strings are well under 384 chars so both bounds pass.
//
// Row formats (tab separated). Every field that is not a String/binary is decimal;
// hexBytes is lowercase %02x of the encoded body. The four sign lines are emitted as
// lowercase UTF-8 HEX (ASCII-safe transport for the TSV).
//   ENC \t <name> \t <x> \t <y> \t <z> \t <isFrontText> \t <line0hex> \t <line1hex> \t <line2hex> \t <line3hex> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeLong(asLong(x,y,z)) + writeBool + 4x writeString) and must match byte-for-byte,
// then round-trips the bytes back to (pos, isFrontText, line0..3).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSignUpdatePacket;

public class PktSignUpdateSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundSignUpdatePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSignUpdatePacket>)
                ServerboundSignUpdatePacket.STREAM_CODEC;

        // Finite / physical battery. Columns: name, x, y, z, isFrontText, line0..3.
        // x/z are 26-bit signed (range -33554432..33554431), y is 12-bit
        // (BlockPos sign-extends 12 bits: -2048..2047). isFrontText is a bool.
        // Lines exercise: empty, ASCII, VarInt length boundaries (127/128 byte len),
        // multibyte UTF-8 (2/3/4-byte: e accent, euro sign, emoji), and a mix.
        Object[][] cases = {
            // {x, y, z, isFrontText, l0, l1, l2, l3}
            {0, 0, 0, false, "", "", "", ""},
            {0, 0, 0, true,  "", "", "", ""},
            {1, 1, 1, true,  "Hello", "World", "Line2", "Line3"},
            {-1, -1, -1, false, "a", "b", "c", "d"},
            {100, 64, -100, true, "Welcome!", "", "to the", "shop"},
            {12345678, 320, -8765432, false, "0123456789", "ABCDEFGHIJ", "abcdefghij", "!@#$%^&*()"},
            {-30000000, -64, 30000000, true,
                "café", "€ euro", "emoji 😀", "mix é€😀"},
            {30000000, 2047, -30000000, false,
                repeat('A', 127), repeat('B', 128), repeat('C', 1), ""},
            {-33554432, -2048, 33554431, true,
                "ééé", "€€€", "😀😀", "tail"},
            {33554431, 2047, -33554432, false, "top", "right", "corner", "case"},
            {8388607, 100, -8388608, true,
                repeat('Z', 200), repeat('y', 50), "short", repeat('Q', 100)},
            {0, 320, 0, false, " leading", "trailing ", " both ", "\tno tab\t"},
        };

        for (Object[] c : cases) {
            int x = (Integer) c[0], y = (Integer) c[1], z = (Integer) c[2];
            boolean isFrontText = (Boolean) c[3];
            String l0 = (String) c[4], l1 = (String) c[5],
                   l2 = (String) c[6], l3 = (String) c[7];
            BlockPos pos = new BlockPos(x, y, z);

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundSignUpdatePacket pkt =
                new ServerboundSignUpdatePacket(pos, isFrontText, l0, l1, l2, l3);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; sanity-assert field equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundSignUpdatePacket dec = CODEC.decode(rbuf);
            long expLong = BlockPos.asLong(x, y, z);
            if (dec.getPos().asLong() != expLong)
                throw new IllegalStateException("pos roundtrip " + dec.getPos().asLong()
                    + " != " + expLong);
            if (dec.isFrontText() != isFrontText)
                throw new IllegalStateException("isFrontText roundtrip " + dec.isFrontText()
                    + " != " + isFrontText);
            String[] dl = dec.getLines();
            if (!dl[0].equals(l0) || !dl[1].equals(l1) || !dl[2].equals(l2) || !dl[3].equals(l3))
                throw new IllegalStateException("lines roundtrip mismatch");

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
            O.print(utf8Hex(l0));
            O.print('\t');
            O.print(utf8Hex(l1));
            O.print('\t');
            O.print(utf8Hex(l2));
            O.print('\t');
            O.print(utf8Hex(l3));
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static String repeat(char ch, int n) {
        StringBuilder sb = new StringBuilder(n);
        for (int i = 0; i < n; i++) sb.append(ch);
        return sb.toString();
    }

    static String utf8Hex(String s) {
        byte[] b = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
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
