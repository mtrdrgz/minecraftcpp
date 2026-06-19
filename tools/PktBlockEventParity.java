// Ground truth for net.minecraft.network.protocol.game.ClientboundBlockEventPacket.
//
// The packet's real STREAM_CODEC is Packet.codec(write, ::new) over a
// RegistryFriendlyByteBuf. The write(RegistryFriendlyByteBuf) body is, VERBATIM
// (ClientboundBlockEventPacket.java:35-40):
//   output.writeBlockPos(this.pos);                              // long asLong(), big-endian
//   output.writeByte(this.b0);                                   // low 8 bits, one byte
//   output.writeByte(this.b1);                                   // low 8 bits, one byte
//   ByteBufCodecs.registry(Registries.BLOCK).encode(output, blk) // VarInt of registry id
//
//   writeBlockPos -> output.writeLong(pos.asLong())   (FriendlyByteBuf.java:398-400)
//   asLong packs x(26)/z(26)/y(12): X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0
//        (BlockPos.java:107-116, PACKED_HORIZONTAL_LENGTH=26, PACKED_Y_LENGTH=12)
//   ByteBufCodecs.registry encode = VarInt.write(out, registry.getIdOrThrow(value))
//        (ByteBufCodecs.java:560-577) -- a plain INT registry id, NOT a Holder /
//        ResourceLocation on the wire, so the C++ PacketBuffer can rebuild it from
//        just the integer id.
//   read: pos=readBlockPos(); b0=readUnsignedByte(); b1=readUnsignedByte();
//         block=registry(BLOCK).decode -> byIdOrThrow(VarInt.read)   (lines 28-33)
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <posLong-dec> <b0-dec> <b1-dec> <blockId-dec> <readableBytes> <hex>
// where posLong is pos.asLong() (signed 64-bit decimal), b0/b1 are the unsigned
// bytes actually on the wire (0..255), blockId is the BLOCK registry id (the VarInt
// payload), readableBytes is the total payload length, and hex is the full payload
// as lowercase bytes.
//
// The C++ pkt_block_event_parity rebuilds the body from posLong/b0/b1/blockId via
// PacketBuffer (writeLong + writeByte + writeByte + writeVarInt) and must match hex
// byte-for-byte and readableBytes; it then decodes hex back and checks the fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundBlockEventPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.block.Block;

public class PktBlockEventParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // A frozen RegistryAccess that resolves Registries.BLOCK -> BuiltInRegistries.BLOCK,
        // exactly what RegistryFriendlyByteBuf needs for ByteBufCodecs.registry(BLOCK).
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        int blockCount = BuiltInRegistries.BLOCK.size();

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

        // Unsigned-byte battery for b0/b1 (writeByte keeps the low 8 bits; read is
        // readUnsignedByte -> 0..255). Use a few representative pairs.
        int[][] bytePairs = {
            {0, 0}, {1, 0}, {0, 1}, {1, 1}, {127, 128}, {128, 127},
            {255, 255}, {0x12, 0x34}, {200, 1}, {64, 255},
        };

        // Block-id battery crossing VarInt 1->2->3 byte boundaries, clamped to the
        // real registry size. byIdOrThrow(idx) gives the block whose getId == idx.
        int[] wantedIds = {0, 1, 2, 126, 127, 128, 129, 254, 255, 256, blockCount - 1};

        // (A) Sweep positions with a fixed byte pair + block id 0 (AIR) and id 1.
        for (int[] p : positions) {
            emit(registryAccess, "pos", p[0], p[1], p[2], 0x12, 0x34, 0);
            emit(registryAccess, "pos", p[0], p[1], p[2], 255, 0,    1);
        }

        // (B) Sweep byte pairs at origin with a fixed (small) block id.
        for (int[] bp : bytePairs) {
            emit(registryAccess, "byte", 0, 0, 0, bp[0], bp[1], 1);
        }

        // (C) Sweep block ids (VarInt boundaries) at a fixed pos + byte pair.
        for (int id : wantedIds) {
            if (id < 0 || id >= blockCount) continue;
            emit(registryAccess, "id", 5, 6, 7, 0xAB, 0xCD, id);
        }

        // (D) A few fully-mixed cases.
        emit(registryAccess, "mix", 33554431, 2047, 33554431, 255, 255, Math.min(255, blockCount - 1));
        emit(registryAccess, "mix", -33554432, -2048, -33554432, 0, 0, blockCount - 1);
        emit(registryAccess, "mix", -1, -1, -1, 128, 1, Math.min(128, blockCount - 1));
    }

    @SuppressWarnings("deprecation")
    static void emit(RegistryAccess registryAccess, String name,
                     int x, int y, int z, int b0, int b1, int blockId) {
        BlockPos pos = new BlockPos(x, y, z);
        Block block = BuiltInRegistries.BLOCK.byIdOrThrow(blockId);

        // sanity: the registry id of this block is exactly the id we asked for.
        int gotId = BuiltInRegistries.BLOCK.getId(block);
        if (gotId != blockId) {
            throw new IllegalStateException("block id mismatch: wanted " + blockId + " got " + gotId);
        }

        ClientboundBlockEventPacket pkt = new ClientboundBlockEventPacket(pos, block, b0, b1);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundBlockEventPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        RegistryFriendlyByteBuf rbuf = new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())), registryAccess);
        ClientboundBlockEventPacket back = ClientboundBlockEventPacket.STREAM_CODEC.decode(rbuf);
        int backB0 = back.getB0() & 0xff;
        int backB1 = back.getB1() & 0xff;
        int backId = BuiltInRegistries.BLOCK.getId(back.getBlock());
        if (!back.getPos().equals(pos) || backB0 != (b0 & 0xff) || backB1 != (b1 & 0xff) || backId != blockId) {
            throw new IllegalStateException("round-trip mismatch for " + name + " pos=" + pos
                + " b0=" + b0 + " b1=" + b1 + " id=" + blockId
                + " -> pos=" + back.getPos() + " b0=" + backB0 + " b1=" + backB1 + " id=" + backId);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(pos.asLong());          // signed 64-bit decimal
        O.print('\t');
        O.print(b0 & 0xff);             // wire unsigned byte 0..255
        O.print('\t');
        O.print(b1 & 0xff);
        O.print('\t');
        O.print(blockId);               // VarInt registry-id payload, decimal
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
