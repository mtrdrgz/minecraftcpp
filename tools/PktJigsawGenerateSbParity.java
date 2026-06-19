// Ground truth for net.minecraft.network.protocol.game.ServerboundJigsawGeneratePacket.
//
// The packet's real STREAM_CODEC is, VERBATIM
// (ServerboundJigsawGeneratePacket.java:10-12):
//   public static final StreamCodec<FriendlyByteBuf, ServerboundJigsawGeneratePacket>
//      STREAM_CODEC = Packet.codec(ServerboundJigsawGeneratePacket::write,
//                                  ServerboundJigsawGeneratePacket::new);
//
// i.e. a plain FriendlyByteBuf codec whose body is the write()/ctor pair, VERBATIM
// (ServerboundJigsawGeneratePacket.java:23-33), field-by-field in codec order:
//   write:  output.writeBlockPos(this.pos);       -> writeLong(pos.asLong())
//                                                    (FriendlyByteBuf.java:393-400)
//                                                    big-endian 8-byte long; asLong packs
//                                                    x(26)/z(26)/y(12): X_OFFSET=38,
//                                                    Z_OFFSET=12, Y_OFFSET=0
//                                                    (BlockPos.java:107-116).
//           output.writeVarInt(this.levels);       -> LEB128 VarInt of the int levels.
//           output.writeBoolean(this.keepJigsaws); -> 1 byte 0x00/0x01.
//   read:   this.pos = input.readBlockPos();       -> BlockPos.of(readLong()).
//           this.levels = input.readVarInt();
//           this.keepJigsaws = input.readBoolean();
//
// No Holder / ResourceLocation / ItemStack / Component / NBT / registry-held type is on
// the wire: every field is a primitive (BE long, VarInt, bool). So the certified
// PacketBuffer (the FriendlyByteBuf port) rebuilds the body directly:
//   writeLong(posLong) + writeVarInt(levels) + writeBoolean(keepJigsaws)
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <posLong-dec> <levels-dec> <keepJigsaws-0|1> <readableBytes> <hex>
// where posLong is pos.asLong() (signed 64-bit decimal), levels is the VarInt payload
// (signed 32-bit decimal), keepJigsaws is 0/1, readableBytes is the total payload
// length, and hex is the full payload as lowercase bytes.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ServerboundJigsawGeneratePacket;
import net.minecraft.server.Bootstrap;

public class PktJigsawGenerateSbParity {
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
            {33554431, 2047, 33554431},     // max corner
            {-33554432, -2048, -33554432},  // min corner
            {16777216, 100, -16777216},
            {-16777216, -100, 16777216},
            {1000000, 320, -1000000},
            {-7, 2047, 7},
            {123456, -2048, -654321},
        };

        // levels battery crossing the VarInt 1->2->3->4->5 byte boundaries, plus
        // signs / extremes (Integer.MIN/MAX encode as 5-byte VarInts).
        int[] levels = {
            0, 1, 2, 7,
            127, 128,
            16383, 16384,
            2097151, 2097152,
            268435455, 268435456,
            -1, -2,
            Integer.MAX_VALUE, Integer.MIN_VALUE,
        };

        // (A) Sweep positions with a fixed (small) levels and both keepJigsaws values.
        for (int[] p : positions) {
            emit("pos", p[0], p[1], p[2], 1, false);
            emit("pos", p[0], p[1], p[2], 1, true);
        }

        // (B) Sweep levels (VarInt boundaries) at a fixed pos, both keepJigsaws values.
        for (int lv : levels) {
            emit("lvl", 5, 6, 7, lv, false);
            emit("lvl", 5, 6, 7, lv, true);
        }

        // (C) A few fully-mixed cases.
        emit("mix", 33554431, 2047, 33554431, Integer.MAX_VALUE, true);
        emit("mix", -33554432, -2048, -33554432, Integer.MIN_VALUE, false);
        emit("mix", -1, -1, -1, 268435456, true);
        emit("mix", 123456, -2048, -654321, 16384, false);
    }

    static void emit(String name, int x, int y, int z, int levels, boolean keepJigsaws) throws Exception {
        BlockPos pos = new BlockPos(x, y, z);
        ServerboundJigsawGeneratePacket pkt = new ServerboundJigsawGeneratePacket(pos, levels, keepJigsaws);

        // Encode through the REAL STREAM_CODEC into a plain FriendlyByteBuf.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ServerboundJigsawGeneratePacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ServerboundJigsawGeneratePacket back = ServerboundJigsawGeneratePacket.STREAM_CODEC.decode(rbuf);
        if (!back.getPos().equals(pos) || back.levels() != levels || back.keepJigsaws() != keepJigsaws) {
            throw new IllegalStateException("round-trip mismatch for " + name + " pos=" + pos
                + " levels=" + levels + " keep=" + keepJigsaws
                + " -> pos=" + back.getPos() + " levels=" + back.levels()
                + " keep=" + back.keepJigsaws());
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(pos.asLong());          // signed 64-bit decimal
        O.print('\t');
        O.print(levels);                // VarInt payload, decimal
        O.print('\t');
        O.print(keepJigsaws ? 1 : 0);   // bool
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
