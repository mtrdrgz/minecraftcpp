// Ground truth for net.minecraft.network.protocol.game.ClientboundGameTestHighlightPosPacket.
//
// The packet is a record(BlockPos absolutePos, BlockPos relativePos) whose real
// STREAM_CODEC is, VERBATIM (ClientboundGameTestHighlightPosPacket.java:11-17):
//   StreamCodec.composite(
//      BlockPos.STREAM_CODEC, ::absolutePos,
//      BlockPos.STREAM_CODEC, ::relativePos,
//      ::new)
//
// Note the codec is typed StreamCodec<ByteBuf, ...> (a PLAIN io.netty ByteBuf, NOT a
// RegistryFriendlyByteBuf): there is no registry/Holder/ResourceLocation/NBT on the
// wire, just two BlockPos longs back-to-back. The wire body, field-by-field in codec
// order, is:
//   BlockPos.STREAM_CODEC.encode(absolutePos) -> FriendlyByteBuf.writeBlockPos(out, pos)
//        -> out.writeLong(pos.asLong())   (BlockPos.java:39-46, FriendlyByteBuf.java:398-400)
//        big-endian 8-byte long; asLong packs x(26)/z(26)/y(12):
//        X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0 (BlockPos.java:107-116).
//   BlockPos.STREAM_CODEC.encode(relativePos) -> writeLong(relativePos.asLong())
//        a second big-endian 8-byte long.
//   read: absolutePos = readBlockPos(); relativePos = readBlockPos().
//
// So the whole payload is exactly two 8-byte big-endian longs (16 bytes total) and
// the C++ PacketBuffer rebuilds it from (absLong, relLong) via writeLong+writeLong.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <absLong-dec> <relLong-dec> <readableBytes> <hex>
// where absLong/relLong are pos.asLong() (signed 64-bit decimal), readableBytes is
// the total payload length, and hex is the full payload as lowercase bytes.
//
// The C++ pkt_game_test_highlight_pos_parity rebuilds the body from absLong/relLong
// via PacketBuffer (writeLong + writeLong) and must match hex byte-for-byte and
// readableBytes; it then decodes hex back and checks both fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundGameTestHighlightPosPacket;
import net.minecraft.server.Bootstrap;

public class PktGameTestHighlightPosParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (plain ByteBuf, packet).
        StreamCodec<ByteBuf, ClientboundGameTestHighlightPosPacket> CODEC =
            (StreamCodec<ByteBuf, ClientboundGameTestHighlightPosPacket>)
                ClientboundGameTestHighlightPosPacket.STREAM_CODEC;

        // BlockPos battery: 0, small, signs, and the exact 26/12/26 packing corners.
        // y is the 12-bit signed field [-2048, 2047]; x,z the 26-bit signed field
        // [-33554432, 33554431]. All in range so the asLong round-trip is exact.
        int[][] positions = {
            {0, 0, 0},
            {1, 2, 3},
            {-1, -1, -1},
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 1},
            {33554431, 2047, 33554431},    // max corner
            {-33554432, -2048, -33554432}, // min corner
            {16777216, 100, -16777216},
            {-16777216, -100, 16777216},
            {1000000, 320, -1000000},
            {-7, 2047, 7},
            {123456, -2048, -654321},
        };

        // (A) Sweep abs positions paired with a fixed (zero) relative pos and a small
        //     non-zero relative pos, exercising both fields independently.
        for (int[] p : positions) {
            emit(CODEC, "abs", p[0], p[1], p[2], 0, 0, 0);
            emit(CODEC, "abs", p[0], p[1], p[2], 1, 2, 3);
        }

        // (B) Sweep relative positions at a fixed abs pos.
        for (int[] p : positions) {
            emit(CODEC, "rel", 5, 6, 7, p[0], p[1], p[2]);
        }

        // (C) A few fully-mixed cases (both corners at once).
        emit(CODEC, "mix", 33554431, 2047, 33554431, -33554432, -2048, -33554432);
        emit(CODEC, "mix", -33554432, -2048, -33554432, 33554431, 2047, 33554431);
        emit(CODEC, "mix", -1, -1, -1, 1, 1, 1);
    }

    static void emit(StreamCodec<ByteBuf, ClientboundGameTestHighlightPosPacket> CODEC, String name,
                     int ax, int ay, int az, int rx, int ry, int rz) throws Exception {
        BlockPos absPos = new BlockPos(ax, ay, az);
        BlockPos relPos = new BlockPos(rx, ry, rz);

        ClientboundGameTestHighlightPosPacket pkt =
            new ClientboundGameTestHighlightPosPacket(absPos, relPos);

        ByteBuf buf = Unpooled.buffer();
        CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        ByteBuf rbuf = Unpooled.wrappedBuffer(unhex(hex.toString()));
        ClientboundGameTestHighlightPosPacket back = CODEC.decode(rbuf);
        if (!back.absolutePos().equals(absPos) || !back.relativePos().equals(relPos)) {
            throw new IllegalStateException("round-trip mismatch for " + name
                + " abs=" + absPos + " rel=" + relPos
                + " -> abs=" + back.absolutePos() + " rel=" + back.relativePos());
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(absPos.asLong());       // signed 64-bit decimal
        O.print('\t');
        O.print(relPos.asLong());       // signed 64-bit decimal
        O.print('\t');
        O.print(n);                     // readableBytes
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
