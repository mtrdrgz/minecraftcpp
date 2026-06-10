// Ground truth for net.minecraft.network.protocol.game.ServerboundBlockEntityTagQueryPacket.
//
// The packet's real STREAM_CODEC is Packet.codec(write, ::new) over a plain
// FriendlyByteBuf (ServerboundBlockEntityTagQueryPacket.java:10-12). The
// write(FriendlyByteBuf) body is, VERBATIM (lines 26-29):
//   output.writeVarInt(this.transactionId);   // LEB128 VarInt of a signed int
//   output.writeBlockPos(this.pos);           // long asLong(), big-endian 8 bytes
//
//   writeBlockPos -> output.writeLong(pos.asLong())   (FriendlyByteBuf.java:398-400)
//   asLong packs x(26)/z(26)/y(12): X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0
//        (BlockPos.java:107-116, PACKED_HORIZONTAL_LENGTH=26, PACKED_Y_LENGTH=12)
//   read: transactionId=readVarInt(); pos=readBlockPos()=BlockPos.of(readLong())
//        (ServerboundBlockEntityTagQueryPacket.java:21-24)
//
// No registry-held type / ItemStack / Component / NBT / Holder / SoundEvent is on
// the wire: the body is a VarInt plus a packed BlockPos long, so the certified
// PacketBuffer (the FriendlyByteBuf port) rebuilds it directly with
// writeVarInt(transactionId) + writeLong(pos.asLong()).
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <transactionId-dec> <posLong-dec> <readableBytes> <hex>
// where transactionId is the signed 32-bit VarInt payload (decimal), posLong is
// pos.asLong() (signed 64-bit decimal), readableBytes is the total payload length,
// and hex is the full payload as lowercase bytes.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ServerboundBlockEntityTagQueryPacket;
import net.minecraft.server.Bootstrap;

public class PktBlockEntityTagQuerySbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // transactionId is a signed int written through writeVarInt; sweep VarInt
        // 1->2->3->4->5 byte boundaries plus signs and Integer extremes (negatives
        // become 5-byte VarInts because writeVarInt zero-extends to 32 bits).
        int[] txns = {
            0, 1, -1, 2, 127, 128, 16383, 16384, 2097151, 2097152,
            268435455, 268435456, Integer.MAX_VALUE, Integer.MIN_VALUE, -128, 1000,
        };

        // BlockPos battery: 0, small, signs, and the exact 26/12/26 packing corners.
        // y is the 12-bit signed field [-2048, 2047]; x,z the 26-bit signed field
        // [-33554432, 33554431]. All in range so the round-trip is exact.
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

        // (A) Sweep transactionId at a fixed pos (origin and a mixed corner).
        for (int t : txns) {
            emit("txn", t, 0, 0, 0);
            emit("txn", t, 1000000, 320, -1000000);
        }

        // (B) Sweep positions at a fixed transactionId (0 and a 3-byte VarInt).
        for (int[] p : positions) {
            emit("pos", 0, p[0], p[1], p[2]);
            emit("pos", 16384, p[0], p[1], p[2]);
        }

        // (C) A few fully-mixed cases.
        emit("mix", Integer.MAX_VALUE, 33554431, 2047, 33554431);
        emit("mix", Integer.MIN_VALUE, -33554432, -2048, -33554432);
        emit("mix", -1, -1, -1, -1);
        emit("mix", 2097152, 123456, -2048, -654321);
    }

    static void emit(String name, int transactionId, int x, int y, int z) throws Exception {
        BlockPos pos = new BlockPos(x, y, z);

        ServerboundBlockEntityTagQueryPacket pkt =
            new ServerboundBlockEntityTagQueryPacket(transactionId, pos);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ServerboundBlockEntityTagQueryPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ServerboundBlockEntityTagQueryPacket back =
            ServerboundBlockEntityTagQueryPacket.STREAM_CODEC.decode(rbuf);
        if (back.getTransactionId() != transactionId || !back.getPos().equals(pos)) {
            throw new IllegalStateException("round-trip mismatch for " + name
                + " txn=" + transactionId + " pos=" + pos
                + " -> txn=" + back.getTransactionId() + " pos=" + back.getPos());
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(transactionId);   // signed 32-bit VarInt payload, decimal
        O.print('\t');
        O.print(pos.asLong());    // signed 64-bit decimal
        O.print('\t');
        O.print(n);               // readableBytes
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
