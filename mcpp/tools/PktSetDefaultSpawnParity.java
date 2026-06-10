// Ground truth for net.minecraft.network.protocol.game.ClientboundSetDefaultSpawnPositionPacket.
//
// The packet is a record(LevelData.RespawnData respawnData). Its STREAM_CODEC is, VERBATIM
// (ClientboundSetDefaultSpawnPositionPacket.java:10-12):
//   StreamCodec.composite(LevelData.RespawnData.STREAM_CODEC, ::respawnData, ::new)
//
// LevelData.RespawnData is record(GlobalPos globalPos, float yaw, float pitch) and its
// STREAM_CODEC is, VERBATIM (LevelData.java:44-52):
//   StreamCodec.composite(
//     GlobalPos.STREAM_CODEC,  ::globalPos,
//     ByteBufCodecs.FLOAT,     ::yaw,
//     ByteBufCodecs.FLOAT,     ::pitch,
//     ::new)
//
// GlobalPos is record(ResourceKey<Level> dimension, BlockPos pos), STREAM_CODEC is, VERBATIM
// (GlobalPos.java:18-20):
//   StreamCodec.composite(
//     ResourceKey.streamCodec(Registries.DIMENSION), ::dimension,
//     BlockPos.STREAM_CODEC,                         ::pos,
//     ::of)
//
//   ResourceKey.streamCodec = Identifier.STREAM_CODEC.map(...)              (ResourceKey.java:21-23)
//   Identifier.STREAM_CODEC  = ByteBufCodecs.STRING_UTF8.map(...)           (Identifier.java:19)
//        -> wire = writeUtf(dimension.identifier().toString())  i.e. "minecraft:overworld"
//           (VarInt UTF-8 byte length + UTF-8 bytes)
//   BlockPos.STREAM_CODEC encode = FriendlyByteBuf.writeBlockPos = writeLong(pos.asLong())
//        (BlockPos.java:39-47, FriendlyByteBuf.java:398-400) -- big-endian 8-byte long.
//        asLong packs x(26)/z(26)/y(12): X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0
//        (BlockPos.java:107-116, PACKED_HORIZONTAL_LENGTH=26, PACKED_Y_LENGTH=12)
//   ByteBufCodecs.FLOAT  = writeFloat (big-endian 4 bytes, rawIntBits).
//
// So the FULL wire body, in order, is:
//   writeUtf(dimensionId)   (VarInt len + UTF-8 bytes)
//   writeLong(pos.asLong()) (BE 8 bytes)
//   writeFloat(yaw)         (BE 4 bytes)
//   writeFloat(pitch)       (BE 4 bytes)
// No Holder / ItemStack / Component / NBT / registry id is on the wire -- the dimension
// is a plain ResourceLocation string -- so the certified PacketBuffer rebuilds it directly.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <dimHex> <posLong-dec> <yawBits-%08x> <pitchBits-%08x> <readableBytes> <hex>
// where dimHex is the UTF-8 bytes of the dimension id ("minecraft:overworld") as lowercase
// hex, posLong is pos.asLong() (signed 64-bit decimal), yawBits/pitchBits are
// Float.floatToRawIntBits(...) as 8-hex-digit lowercase, readableBytes is the total payload
// length, and hex is the full payload as lowercase bytes.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.GlobalPos;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetDefaultSpawnPositionPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.storage.LevelData;

public class PktSetDefaultSpawnParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Dimension keys: real vanilla ones plus a few synthesized custom ResourceLocations
        // that exercise ASCII + multibyte boundaries in the namespace:path string. Each is a
        // valid Identifier (lowercase, allowed chars) so .toString() round-trips cleanly.
        ResourceKey<Level>[] dims = new ResourceKey[] {
            Level.OVERWORLD,                                                  // minecraft:overworld
            Level.NETHER,                                                     // minecraft:the_nether
            Level.END,                                                        // minecraft:the_end
            ResourceKey.create(Registries.DIMENSION, Identifier.parse("a:b")),
            ResourceKey.create(Registries.DIMENSION, Identifier.parse("minecraft:custom_dimension_with_a_fairly_long_path_name")),
            ResourceKey.create(Registries.DIMENSION, Identifier.parse("my_mod:nether_like")),
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

        // Float battery for yaw/pitch including 0.0/-0.0/1.0/-1.0 and physical angle values.
        // yaw is a degree value (wrapped to [-180,180] in callers, but the wire codec is a
        // raw ByteBufCodecs.FLOAT so any finite float encodes verbatim); pitch [-90,90].
        float[] yaws   = {0.0f, -0.0f, 1.0f, -1.0f, 90.0f, -90.0f, 180.0f, -180.0f, 45.5f, -179.99f, 123.456f};
        float[] pitches = {0.0f, -0.0f, 1.0f, -1.0f, 90.0f, -90.0f, 45.0f, -45.0f, 12.34f, -89.99f, 7.5f};

        // (A) Sweep dimensions at origin with a fixed angle.
        for (ResourceKey<Level> dim : dims) {
            emit("dim", dim, 0, 0, 0, 0.0f, 0.0f);
        }

        // (B) Sweep positions at OVERWORLD with a fixed angle.
        for (int[] p : positions) {
            emit("pos", Level.OVERWORLD, p[0], p[1], p[2], 90.0f, -45.0f);
        }

        // (C) Sweep yaw values at OVERWORLD origin with a fixed pitch.
        for (float yaw : yaws) {
            emit("yaw", Level.OVERWORLD, 8, 64, -8, yaw, 0.0f);
        }

        // (D) Sweep pitch values at OVERWORLD origin with a fixed yaw.
        for (float pitch : pitches) {
            emit("pitch", Level.OVERWORLD, -3, 200, 5, 0.0f, pitch);
        }

        // (E) A few fully-mixed cases across all fields.
        emit("mix", Level.NETHER, 33554431, 2047, 33554431, 180.0f, 90.0f);
        emit("mix", Level.END, -33554432, -2048, -33554432, -180.0f, -90.0f);
        emit("mix", dims[3], -1, -1, -1, 45.5f, -12.34f);
        emit("mix", dims[4], 123456, -2048, -654321, -179.99f, 89.99f);
    }

    @SuppressWarnings("deprecation")
    static void emit(String name, ResourceKey<Level> dim,
                     int x, int y, int z, float yaw, float pitch) throws Exception {
        BlockPos pos = new BlockPos(x, y, z);
        GlobalPos gp = GlobalPos.of(dim, pos);
        LevelData.RespawnData rd = new LevelData.RespawnData(gp, yaw, pitch);
        ClientboundSetDefaultSpawnPositionPacket pkt = new ClientboundSetDefaultSpawnPositionPacket(rd);

        // STREAM_CODEC is over plain ByteBuf (no RegistryAccess needed for a ResourceLocation).
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSetDefaultSpawnPositionPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ClientboundSetDefaultSpawnPositionPacket back =
            ClientboundSetDefaultSpawnPositionPacket.STREAM_CODEC.decode(rbuf);
        LevelData.RespawnData brd = back.respawnData();
        boolean ok = brd.globalPos().dimension().equals(dim)
            && brd.globalPos().pos().equals(pos)
            && Float.floatToRawIntBits(brd.yaw()) == Float.floatToRawIntBits(yaw)
            && Float.floatToRawIntBits(brd.pitch()) == Float.floatToRawIntBits(pitch)
            && rbuf.readableBytes() == 0;
        if (!ok) {
            throw new IllegalStateException("round-trip mismatch for " + name + " dim=" + dim
                + " pos=" + pos + " yaw=" + yaw + " pitch=" + pitch
                + " -> dim=" + brd.globalPos().dimension() + " pos=" + brd.globalPos().pos()
                + " yaw=" + brd.yaw() + " pitch=" + brd.pitch()
                + " rem=" + rbuf.readableBytes());
        }

        // dimension wire string == dim.identifier().toString() ("minecraft:overworld").
        String dimStr = dim.identifier().toString();
        StringBuilder dimHex = new StringBuilder();
        byte[] dimBytes = dimStr.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        for (byte b : dimBytes) dimHex.append(String.format("%02x", b & 0xff));

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(dimHex.toString());                          // dimension id UTF-8 bytes as hex
        O.print('\t');
        O.print(pos.asLong());                               // signed 64-bit decimal
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(yaw)));   // yaw raw int bits
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(pitch))); // pitch raw int bits
        O.print('\t');
        O.print(n);                                          // readableBytes
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
