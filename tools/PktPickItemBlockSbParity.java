// Ground truth for net.minecraft.network.protocol.game.ServerboundPickItemFromBlockPacket.
//
// The packet is a record(BlockPos pos, boolean includeData) implements Packet<...>.
// Its STREAM_CODEC is, VERBATIM (ServerboundPickItemFromBlockPacket.java:11-17):
//   StreamCodec.composite(
//     BlockPos.STREAM_CODEC,        ServerboundPickItemFromBlockPacket::pos,
//     ByteBufCodecs.BOOL,           ServerboundPickItemFromBlockPacket::includeData,
//     ServerboundPickItemFromBlockPacket::new)
//
//   BlockPos.STREAM_CODEC encode = FriendlyByteBuf.writeBlockPos = writeLong(pos.asLong())
//        (BlockPos.java:39-47, FriendlyByteBuf.java:398-400) -- big-endian 8-byte long.
//        asLong packs x(26)/z(26)/y(12): X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0
//        (BlockPos.java:107-116, PACKED_HORIZONTAL_LENGTH=26, PACKED_Y_LENGTH=12).
//   ByteBufCodecs.BOOL encode = output.writeBoolean(value)  (ByteBufCodecs.java:56-64)
//        -- a single byte, 0x01 for true, 0x00 for false.
//
// So the FULL wire body, in order, is:
//   writeLong(pos.asLong())   (BE 8 bytes)
//   writeBoolean(includeData) (1 byte)
// No Holder / ItemStack / Component / NBT / registry id is on the wire -- both fields decompose
// to primitives the certified PacketBuffer supports -- so it rebuilds the body directly.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <posLong-dec> <includeData 0|1> <readableBytes> <hex>
// where posLong is pos.asLong() (signed 64-bit decimal), includeData is the bool as 0/1,
// readableBytes is the total payload length, and hex is the full payload as lowercase bytes.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ServerboundPickItemFromBlockPacket;
import net.minecraft.server.Bootstrap;

public class PktPickItemBlockSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

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

        // Sweep every position with includeData both false and true.
        for (int[] p : positions) {
            emit("pos", p[0], p[1], p[2], false);
            emit("pos", p[0], p[1], p[2], true);
        }
    }

    static void emit(String name, int x, int y, int z, boolean includeData) throws Exception {
        BlockPos pos = new BlockPos(x, y, z);
        ServerboundPickItemFromBlockPacket pkt = new ServerboundPickItemFromBlockPacket(pos, includeData);

        // STREAM_CODEC is over a plain ByteBuf (no RegistryAccess needed).
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ServerboundPickItemFromBlockPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ServerboundPickItemFromBlockPacket back = ServerboundPickItemFromBlockPacket.STREAM_CODEC.decode(rbuf);
        boolean ok = back.pos().equals(pos)
            && back.includeData() == includeData
            && rbuf.readableBytes() == 0;
        if (!ok) {
            throw new IllegalStateException("round-trip mismatch for " + name + " pos=" + pos
                + " includeData=" + includeData
                + " -> pos=" + back.pos() + " includeData=" + back.includeData()
                + " rem=" + rbuf.readableBytes());
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(pos.asLong());                 // signed 64-bit decimal
        O.print('\t');
        O.print(includeData ? 1 : 0);          // bool as 0/1
        O.print('\t');
        O.print(n);                            // readableBytes
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
