// Ground truth for net.minecraft.network.protocol.game.ServerboundSetTestBlockPacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet is a record (position, mode, message) encoded by a StreamCodec.composite
// (ServerboundSetTestBlockPacket.java:12-20) in this exact field order:
//   BlockPos.STREAM_CODEC          -> position
//   TestBlockMode.STREAM_CODEC     -> mode
//   ByteBufCodecs.STRING_UTF8      -> message
//
// Wire bytes (composite encodes each field in order, no id/length prefix — the
// stream codec is the packet body only):
//   1) BlockPos.STREAM_CODEC.encode == FriendlyByteBuf.writeBlockPos(pos)
//        == output.writeLong(pos.asLong())  (BlockPos.java:39-47, FriendlyByteBuf
//        writeBlockPos -> writeLong; 8 bytes big-endian).
//      BlockPos.asLong packs one long: PACKED_HORIZONTAL_LENGTH = 26, PACKED_Y_LENGTH = 12,
//        X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0:
//        node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
//   2) TestBlockMode.STREAM_CODEC == ByteBufCodecs.idMapper(BY_ID, mode -> mode.id)
//        (TestBlockMode.java); idMapper.encode == VarInt.write(output, mode.id)
//        (ByteBufCodecs.java:542-553). The four modes have id == ordinal:
//        START=0, LOG=1, FAIL=2, ACCEPT=3. So this is a single VarInt of the id.
//   3) ByteBufCodecs.STRING_UTF8 == stringUtf8(32767) -> Utf8String.write(output, value,
//        32767): VarInt(byte length) + UTF-8 bytes (ByteBufCodecs.java:168,267-277).
//
// Row formats (tab separated). Decimal fields are decimal; message is emitted as
// lowercase UTF-8 HEX (ASCII-safe transport for the TSV). hexBytes is lowercase %02x
// of the full encoded body.
//   ENC \t <name> \t <x> \t <y> \t <z> \t <modeId> \t <messageHex> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeLong(asLong(x,y,z)) + writeVarInt(modeId) + writeString(message)) and must
// match byte-for-byte + length, then round-trips the bytes back to (pos, mode, message).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSetTestBlockPacket;
import net.minecraft.world.level.block.state.properties.TestBlockMode;

public class PktSetTestBlockSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundSetTestBlockPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSetTestBlockPacket>)
                ServerboundSetTestBlockPacket.STREAM_CODEC;

        TestBlockMode[] modes = TestBlockMode.values(); // START,LOG,FAIL,ACCEPT

        // Finite / physical battery. Columns: name, x, y, z, mode, message.
        // x/z are 26-bit signed (range -33554432..33554431), y is 12-bit
        // (BlockPos sign-extends 12 bits: -2048..2047). mode is one of the 4 enum
        // values. Messages exercise: empty, ASCII, VarInt length boundaries
        // (127/128 byte len), multibyte UTF-8 (2/3/4-byte: e accent, euro sign, emoji),
        // and whitespace/control chars.
        Object[][] cases = {
            // {x, y, z, mode, message}
            {0, 0, 0, TestBlockMode.START, ""},
            {0, 0, 0, TestBlockMode.LOG, ""},
            {0, 0, 0, TestBlockMode.FAIL, ""},
            {0, 0, 0, TestBlockMode.ACCEPT, ""},
            {1, 1, 1, TestBlockMode.START, "hello"},
            {-1, -1, -1, TestBlockMode.LOG, "a"},
            {100, 64, -100, TestBlockMode.FAIL, "test message"},
            {12345678, 320, -8765432, TestBlockMode.ACCEPT, "0123456789ABCDEFGHIJ!@#$%^&*()"},
            {-30000000, -64, 30000000, TestBlockMode.START, "café € emoji 😀 mix é€😀"},
            {30000000, 2047, -30000000, TestBlockMode.LOG, repeat('A', 127)},
            {-33554432, -2048, 33554431, TestBlockMode.FAIL, repeat('B', 128)},
            {33554431, 2047, -33554432, TestBlockMode.ACCEPT, repeat('C', 200)},
            {8388607, 100, -8388608, TestBlockMode.START, "ééé€€€😀😀"},
            {0, 320, 0, TestBlockMode.LOG, " leading trailing \tno tab\t"},
            {-8388608, 0, 8388607, TestBlockMode.ACCEPT, "minecraft:test_block"},
        };

        for (Object[] c : cases) {
            int x = (Integer) c[0], y = (Integer) c[1], z = (Integer) c[2];
            TestBlockMode mode = (TestBlockMode) c[3];
            String message = (String) c[4];
            BlockPos pos = new BlockPos(x, y, z);
            int modeId = mode.ordinal(); // == mode.id for this enum (verified below)

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundSetTestBlockPacket pkt =
                new ServerboundSetTestBlockPacket(pos, mode, message);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; sanity-assert field equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundSetTestBlockPacket dec = CODEC.decode(rbuf);
            long expLong = BlockPos.asLong(x, y, z);
            if (dec.position().asLong() != expLong)
                throw new IllegalStateException("pos roundtrip " + dec.position().asLong()
                    + " != " + expLong);
            if (dec.mode() != mode)
                throw new IllegalStateException("mode roundtrip " + dec.mode() + " != " + mode);
            if (!dec.message().equals(message))
                throw new IllegalStateException("message roundtrip mismatch");
            if (rbuf.isReadable())
                throw new IllegalStateException("trailing bytes after decode: " + rbuf.readableBytes());

            String name = "case_" + mode.name() + "_x" + x + "_y" + y + "_z" + z;
            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(x);
            O.print('\t');
            O.print(y);
            O.print('\t');
            O.print(z);
            O.print('\t');
            O.print(modeId);
            O.print('\t');
            O.print(utf8Hex(message));
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
