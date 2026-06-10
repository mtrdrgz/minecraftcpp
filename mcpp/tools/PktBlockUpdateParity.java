// Ground truth for net.minecraft.network.protocol.game.ClientboundBlockUpdatePacket.
//
// The packet's real STREAM_CODEC is, VERBATIM
// (ClientboundBlockUpdatePacket.java:14-20):
//   StreamCodec.composite(
//      BlockPos.STREAM_CODEC,                         ::getPos,
//      ByteBufCodecs.idMapper(Block.BLOCK_STATE_REGISTRY), ::getBlockState,
//      ::new)
//
// So the wire body, field-by-field in codec order, is:
//   BlockPos.STREAM_CODEC.encode -> FriendlyByteBuf.writeBlockPos(out, pos)
//        -> out.writeLong(pos.asLong())   (BlockPos.java:39-46, FriendlyByteBuf.java:398-400)
//        big-endian 8-byte long; asLong packs x(26)/z(26)/y(12):
//        X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0 (BlockPos.java:107-116).
//   ByteBufCodecs.idMapper(Block.BLOCK_STATE_REGISTRY).encode
//        -> VarInt.write(out, BLOCK_STATE_REGISTRY.getIdOrThrow(blockState))
//        (ByteBufCodecs.java:542-558) -- a PLAIN integer registry id of the
//        BlockState (the IdMapper<BlockState> id), NOT a Holder / ResourceLocation /
//        ItemStack / NBT on the wire. So the C++ PacketBuffer rebuilds the body from
//        just (posLong, stateId).
//   read: pos = BlockPos.STREAM_CODEC.decode -> readBlockPos();
//         state = idMapper.decode -> byIdOrThrow(VarInt.read).
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <posLong-dec> <stateId-dec> <readableBytes> <hex>
// where posLong is pos.asLong() (signed 64-bit decimal), stateId is the
// BLOCK_STATE_REGISTRY id (the VarInt payload, decimal), readableBytes is the total
// payload length, and hex is the full payload as lowercase bytes.
//
// The C++ pkt_block_update_parity rebuilds the body from posLong/stateId via
// PacketBuffer (writeLong + writeVarInt) and must match hex byte-for-byte and
// readableBytes; it then decodes hex back and checks the fields.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundBlockUpdatePacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

public class PktBlockUpdateParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The STREAM_CODEC is typed RegistryFriendlyByteBuf; the idMapper itself only
        // needs the (already-frozen) Block.BLOCK_STATE_REGISTRY, but build a real
        // RegistryAccess-backed buffer so the typed codec is exercised faithfully.
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // Block.BLOCK_STATE_REGISTRY is populated during Bootstrap; size() is the count
        // of distinct BlockStates (every state of every block).
        int stateCount = Block.BLOCK_STATE_REGISTRY.size();
        if (stateCount <= 0) throw new IllegalStateException("BLOCK_STATE_REGISTRY empty");

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

        // BlockState-id battery crossing VarInt 1->2->3->4 byte boundaries, clamped to
        // the real registry size. byIdOrThrow(idx) gives the state whose getId == idx.
        int[] wantedIds = {
            0, 1, 2, 126, 127, 128, 129, 254, 255, 256,
            16383, 16384, 16385,
            stateCount - 1,
        };

        // (A) Sweep positions with a fixed (small) state id 0 and id 1.
        for (int[] p : positions) {
            emit(registryAccess, "pos", p[0], p[1], p[2], 0);
            emit(registryAccess, "pos", p[0], p[1], p[2], 1);
        }

        // (B) Sweep state ids (VarInt boundaries) at a fixed pos.
        for (int id : wantedIds) {
            if (id < 0 || id >= stateCount) continue;
            emit(registryAccess, "id", 5, 6, 7, id);
        }

        // (C) A few fully-mixed cases.
        emit(registryAccess, "mix", 33554431, 2047, 33554431, stateCount - 1);
        emit(registryAccess, "mix", -33554432, -2048, -33554432, Math.min(16384, stateCount - 1));
        emit(registryAccess, "mix", -1, -1, -1, Math.min(255, stateCount - 1));
    }

    @SuppressWarnings("deprecation")
    static void emit(RegistryAccess registryAccess, String name,
                     int x, int y, int z, int stateId) throws Exception {
        BlockPos pos = new BlockPos(x, y, z);
        BlockState state = Block.BLOCK_STATE_REGISTRY.byIdOrThrow(stateId);

        // sanity: the registry id of this state is exactly the id we asked for.
        int gotId = Block.BLOCK_STATE_REGISTRY.getId(state);
        if (gotId != stateId) {
            throw new IllegalStateException("state id mismatch: wanted " + stateId + " got " + gotId);
        }

        ClientboundBlockUpdatePacket pkt = new ClientboundBlockUpdatePacket(pos, state);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundBlockUpdatePacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        RegistryFriendlyByteBuf rbuf = new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())), registryAccess);
        ClientboundBlockUpdatePacket back = ClientboundBlockUpdatePacket.STREAM_CODEC.decode(rbuf);
        int backId = Block.BLOCK_STATE_REGISTRY.getId(back.getBlockState());
        if (!back.getPos().equals(pos) || backId != stateId) {
            throw new IllegalStateException("round-trip mismatch for " + name + " pos=" + pos
                + " id=" + stateId + " -> pos=" + back.getPos() + " id=" + backId);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(pos.asLong());          // signed 64-bit decimal
        O.print('\t');
        O.print(stateId);               // VarInt registry-id payload, decimal
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
