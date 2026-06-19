// Ground truth for net.minecraft.network.protocol.game.ServerboundUseItemOnPacket.
//
// The packet's real STREAM_CODEC is Packet.codec(::write, ::new) over a plain
// FriendlyByteBuf. The write(FriendlyByteBuf) body is, VERBATIM
// (ServerboundUseItemOnPacket.java:30-34):
//   output.writeEnum(this.hand);              // VarInt(hand.ordinal())
//   output.writeBlockHitResult(this.blockHit);
//   output.writeVarInt(this.sequence);        // VarInt
//
// writeEnum(value) -> writeVarInt(value.ordinal())             (FriendlyByteBuf.java:471-473)
//   InteractionHand ordinals: MAIN_HAND=0, OFF_HAND=1          (InteractionHand.java:10-12)
//
// writeBlockHitResult(blockHit)                                (FriendlyByteBuf.java:636-646):
//   BlockPos blockPos = blockHit.getBlockPos();
//   this.writeBlockPos(blockPos);                              // writeLong(pos.asLong()), big-endian
//   this.writeEnum(blockHit.getDirection());                  // VarInt(direction.ordinal())
//   Vec3 location = blockHit.getLocation();
//   this.writeFloat((float)(location.x - blockPos.getX()));    // BE float
//   this.writeFloat((float)(location.y - blockPos.getY()));    // BE float
//   this.writeFloat((float)(location.z - blockPos.getZ()));    // BE float
//   this.writeBoolean(blockHit.isInside());                    // one byte 0/1
//   this.writeBoolean(blockHit.isWorldBorderHit());            // one byte 0/1
//
//   writeBlockPos -> output.writeLong(pos.asLong())            (FriendlyByteBuf.java:398-400)
//   asLong packs x(26)/z(26)/y(12): X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0
//        (BlockPos.java:107-116, PACKED_HORIZONTAL_LENGTH=26, PACKED_Y_LENGTH=12)
//   Direction ordinals (declaration order, used by writeEnum):
//        DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5        (Direction.java:33-38)
//
// NOTE: writeEnum uses Enum.ordinal(), NOT the STREAM_CODEC idMapper. For both
// InteractionHand and Direction the ordinal happens to equal the id/3d-data, but
// we emit the *ordinal actually written* so the C++ side just rolls the same VarInt.
//
// No Holder / ItemStack / Component / NBT-with-registry / SoundEvent is on the wire:
// every field decomposes to primitives the C++ PacketBuffer supports.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <handOrd-dec> <posLong-dec> <dirOrd-dec> <fxBits-hex8> <fyBits-hex8>
//       <fzBits-hex8> <inside-dec> <worldBorder-dec> <seq-dec> <readableBytes> <hex>
// where:
//   handOrd / dirOrd  = the VarInt ordinal values actually written
//   posLong           = pos.asLong() (signed 64-bit decimal)
//   fxBits/fyBits/fzBits = the EXACT 32-bit IEEE-754 raw bits of the three floats
//                       that the codec wrote (Float.floatToRawIntBits), lowercase hex.
//                       This sidesteps any double->float rounding ambiguity: the C++
//                       side writes these raw float bits verbatim.
//   inside/worldBorder = 0/1
//   seq               = VarInt sequence (signed 32-bit decimal)
//   readableBytes     = total payload length
//   hex               = full payload as lowercase bytes
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ServerboundUseItemOnPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.InteractionHand;
import net.minecraft.world.phys.BlockHitResult;
import net.minecraft.world.phys.Vec3;

public class PktUseItemOnSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // BlockPos battery: 0, small, signs, and the exact 26/12/26 packing corners.
        // y is the 12-bit signed field [-2048, 2047]; x,z the 26-bit signed field
        // [-33554432, 33554431]. All in range so asLong round-trips exactly.
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

        // Local-offset (clickX/Y/Z) battery: the special floats the spec calls for,
        // plus typical [0,1) block-local hit fractions.
        float[] offsets = {
            0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 0.25f, 0.75f,
            0.1f, 0.9f, 0.499999f, 0.500001f,
            Float.MIN_VALUE, Float.MAX_VALUE,
        };

        // Direction ordinals: DOWN=0 .. EAST=5.
        Direction[] dirs = Direction.values();

        // VarInt sequence battery crossing 1->5 byte boundaries + signs/extremes.
        int[] sequences = {
            0, 1, -1, 127, 128, 16383, 16384, 2097151, 2097152,
            268435455, 268435456, Integer.MAX_VALUE, Integer.MIN_VALUE, -127, -128,
        };

        // (A) Sweep positions x both hands x DOWN, fixed offsets + flags + seq.
        for (int[] p : positions) {
            emit("posMain", InteractionHand.MAIN_HAND, p[0], p[1], p[2],
                 Direction.DOWN, 0.5f, 0.5f, 0.5f, false, false, 0);
            emit("posOff", InteractionHand.OFF_HAND, p[0], p[1], p[2],
                 Direction.UP, 0.25f, 0.75f, 0.1f, true, true, 1);
        }

        // (B) Sweep every Direction at origin, MAIN_HAND, fixed offsets + seq.
        for (Direction d : dirs) {
            emit("dir", InteractionHand.MAIN_HAND, 5, 6, 7, d,
                 0.5f, 0.5f, 0.5f, false, false, 7);
        }

        // (C) Sweep the float offset battery on the X axis at a fixed pos.
        for (float off : offsets) {
            emit("offX", InteractionHand.MAIN_HAND, 10, 20, 30,
                 Direction.NORTH, off, 0.5f, 0.5f, false, false, 3);
        }
        // ... and a few all-three-axes-equal cases.
        for (float off : offsets) {
            emit("offXYZ", InteractionHand.OFF_HAND, -3, 0, 4,
                 Direction.SOUTH, off, off, off, true, false, 5);
        }

        // (D) Sweep the boolean flags (inside, worldBorder) combinations.
        boolean[] bools = {false, true};
        for (boolean in : bools) {
            for (boolean wb : bools) {
                emit("flag", InteractionHand.MAIN_HAND, 8, 9, 10,
                     Direction.EAST, 0.3f, 0.6f, 0.9f, in, wb, 42);
            }
        }

        // (E) Sweep the VarInt sequence battery at a fixed pos/dir/offsets/flags.
        for (int seq : sequences) {
            emit("seq", InteractionHand.OFF_HAND, 1, 2, 3,
                 Direction.WEST, 0.5f, 0.5f, 0.5f, false, true, seq);
        }

        // (F) A few fully-mixed extremes.
        emit("mix", InteractionHand.OFF_HAND, 33554431, 2047, 33554431,
             Direction.EAST, 1.0f, 0.0f, -1.0f, true, true, Integer.MAX_VALUE);
        emit("mix", InteractionHand.MAIN_HAND, -33554432, -2048, -33554432,
             Direction.DOWN, -0.0f, Float.MAX_VALUE, Float.MIN_VALUE, false, true, 268435455);
        emit("mix", InteractionHand.MAIN_HAND, -1, -1, -1,
             Direction.UP, 0.499999f, 0.500001f, 0.25f, true, false, 16384);
    }

    @SuppressWarnings("deprecation")
    static void emit(String name, InteractionHand hand,
                     int x, int y, int z, Direction dir,
                     float offX, float offY, float offZ,
                     boolean inside, boolean worldBorder, int sequence) {
        BlockPos pos = new BlockPos(x, y, z);

        // Build the location so that (location.coord - pos.coord) reproduces the
        // intended local offset exactly: location is double, pos coords are int, so
        // location.x - blockPos.getX() == (double)offX, and the codec's
        // (float)(...) cast recovers offX bit-for-bit.
        Vec3 location = new Vec3((double) x + (double) offX,
                                 (double) y + (double) offY,
                                 (double) z + (double) offZ);
        BlockHitResult blockHit = new BlockHitResult(location, dir, pos, inside, worldBorder);

        ServerboundUseItemOnPacket pkt = new ServerboundUseItemOnPacket(hand, blockHit, sequence);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ServerboundUseItemOnPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ServerboundUseItemOnPacket back = ServerboundUseItemOnPacket.STREAM_CODEC.decode(rbuf);
        BlockHitResult bh = back.getHitResult();
        if (back.getHand() != hand
                || !bh.getBlockPos().equals(pos)
                || bh.getDirection() != dir
                || bh.isInside() != inside
                || bh.isWorldBorderHit() != worldBorder
                || back.getSequence() != sequence) {
            throw new IllegalStateException("round-trip mismatch for " + name
                + " hand=" + hand + " pos=" + pos + " dir=" + dir
                + " inside=" + inside + " wb=" + worldBorder + " seq=" + sequence);
        }

        // The exact float bits the codec wrote: recompute the same (float) casts the
        // codec did, then take raw int bits. These are the bytes on the wire.
        int fxBits = Float.floatToRawIntBits((float) (location.x - pos.getX()));
        int fyBits = Float.floatToRawIntBits((float) (location.y - pos.getY()));
        int fzBits = Float.floatToRawIntBits((float) (location.z - pos.getZ()));

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(hand.ordinal());        // VarInt ordinal actually written
        O.print('\t');
        O.print(pos.asLong());          // signed 64-bit decimal
        O.print('\t');
        O.print(dir.ordinal());         // VarInt ordinal actually written
        O.print('\t');
        O.print(String.format("%08x", fxBits));
        O.print('\t');
        O.print(String.format("%08x", fyBits));
        O.print('\t');
        O.print(String.format("%08x", fzBits));
        O.print('\t');
        O.print(inside ? 1 : 0);
        O.print('\t');
        O.print(worldBorder ? 1 : 0);
        O.print('\t');
        O.print(sequence);              // VarInt payload, signed decimal
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
