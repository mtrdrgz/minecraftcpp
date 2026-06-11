import java.io.PrintStream;

import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Vec3i;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSetStructureBlockPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.entity.StructureBlockEntity;
import net.minecraft.world.level.block.state.properties.StructureMode;

/**
 * Ground-truth dump for net.minecraft.network.protocol.game.ServerboundSetStructureBlockPacket.
 *
 * Real codec order (ServerboundSetStructureBlockPacket.write, 26.1.2):
 *   writeBlockPos(pos)               -> writeLong(pos.asLong())   (big-endian 8-byte long)
 *   writeEnum(updateType)            -> writeVarInt(ordinal())    UPDATE_DATA=0 SAVE_AREA=1 LOAD_AREA=2 SCAN_AREA=3
 *   writeEnum(mode)                  -> writeVarInt(ordinal())    SAVE=0 LOAD=1 CORNER=2 DATA=3
 *   writeUtf(name)                   -> VarInt(utf8 byteLen)+UTF-8
 *   writeByte(offset.getX())         -> low 8 bits of int
 *   writeByte(offset.getY())
 *   writeByte(offset.getZ())
 *   writeByte(size.getX())
 *   writeByte(size.getY())
 *   writeByte(size.getZ())
 *   writeEnum(mirror)                -> writeVarInt(ordinal())    NONE=0 LEFT_RIGHT=1 FRONT_BACK=2
 *   writeEnum(rotation)              -> writeVarInt(ordinal())    NONE=0 CLOCKWISE_90=1 CLOCKWISE_180=2 COUNTERCLOCKWISE_90=3
 *   writeUtf(data)                   -> VarInt(utf8 byteLen)+UTF-8  (write uses default maxLen 32767)
 *   writeFloat(integrity)            -> 4 big-endian bytes of floatToRawIntBits
 *   writeVarLong(seed)               -> LEB128, up to 10 bytes
 *   writeByte(flags)                 -> bit0=ignoreEntities bit1=showAir bit2=showBoundingBox bit3=strict
 *
 * Every field reduces to long/varInt/string/byte/float/varLong; the certified
 * mc::net::PacketBuffer (the FriendlyByteBuf port) implements each 1:1.
 *
 * Row format (tab-separated; strings emitted as UTF-8 hex so the TSV stays ASCII):
 *   ENC \t name \t posLong(dec) \t updateTypeOrd \t modeOrd \t nameHex
 *       \t offX \t offY \t offZ \t sizeX \t sizeY \t sizeZ \t mirrorOrd \t rotationOrd
 *       \t dataHex \t integrityBits(int dec) \t seed(dec) \t flags(dec)
 *       \t readableBytes(dec) \t hexBytes
 */
public class PktSetStructureBlockSbParity {
    static final PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundSetStructureBlockPacket> codec =
                ServerboundSetStructureBlockPacket.STREAM_CODEC;

        // Battery: coordinate extremes/signs, every enum value covered, empty + ASCII +
        // multibyte-UTF-8 strings, offset/size byte extremes (negative truncation),
        // integrity float specials, VarLong-length boundaries on the seed, flag bit combos.
        emit(codec, "defaults_zero",
                bp(0, 0, 0), StructureBlockEntity.UpdateType.UPDATE_DATA, StructureMode.SAVE,
                "", new BlockPos(0, 0, 0), new Vec3i(0, 0, 0),
                Mirror.NONE, Rotation.NONE, "", 1.0F, 0L,
                false, false, false, false);
        emit(codec, "load_area_basic",
                bp(1, 2, 3), StructureBlockEntity.UpdateType.LOAD_AREA, StructureMode.LOAD,
                "minecraft:village/plains/houses", new BlockPos(-1, -2, -3), new Vec3i(48, 48, 48),
                Mirror.LEFT_RIGHT, Rotation.CLOCKWISE_90, "metadata", 0.5F, 12345L,
                true, false, true, false);
        emit(codec, "save_corner",
                bp(-1, -1, -1), StructureBlockEntity.UpdateType.SAVE_AREA, StructureMode.CORNER,
                "ns:thing", new BlockPos(48, 48, 48), new Vec3i(1, 1, 1),
                Mirror.FRONT_BACK, Rotation.CLOCKWISE_180, "data_string", 0.0F, -1L,
                false, true, false, true);
        emit(codec, "scan_data_allflags",
                bp(33554431, 2047, 33554431), StructureBlockEntity.UpdateType.SCAN_AREA, StructureMode.DATA,
                "x", new BlockPos(-48, -48, -48), new Vec3i(0, 0, 0),
                Mirror.NONE, Rotation.COUNTERCLOCKWISE_90, "y", 1.0F, 9223372036854775807L,
                true, true, true, true);
        emit(codec, "minxyz",
                bp(-33554432, -2048, -33554432), StructureBlockEntity.UpdateType.UPDATE_DATA, StructureMode.SAVE,
                "a:b", new BlockPos(7, -7, 0), new Vec3i(48, 0, 24),
                Mirror.LEFT_RIGHT, Rotation.CLOCKWISE_180, "c:d", 0.25F, -9223372036854775808L,
                false, false, true, false);
        emit(codec, "multibyte_strings",
                bp(10, 64, -20), StructureBlockEntity.UpdateType.LOAD_AREA, StructureMode.LOAD,
                "structure_éü中文😀", new BlockPos(-30, 12, 30), new Vec3i(12, 34, 5),
                Mirror.FRONT_BACK, Rotation.NONE, "metadata_éü中文😀", 0.75F, 4294967296L,
                true, true, false, false);
        emit(codec, "integrity_specials",
                bp(100, 70, 100), StructureBlockEntity.UpdateType.SAVE_AREA, StructureMode.DATA,
                "minecraft:air", new BlockPos(1, 2, 3), new Vec3i(5, 6, 7),
                Mirror.NONE, Rotation.CLOCKWISE_90, "tiny", 1.401298464324817E-45F, 127L,
                false, true, true, false);
        emit(codec, "seed_varlong_128_boundary",
                bp(7, 8, 9), StructureBlockEntity.UpdateType.UPDATE_DATA, StructureMode.SAVE,
                "ns:a", new BlockPos(-48, 48, -1), new Vec3i(48, 1, 48),
                Mirror.LEFT_RIGHT, Rotation.CLOCKWISE_90, "ns:b", 0.123456F, 128L,
                true, false, false, true);
        emit(codec, "long_name_127",
                bp(-5, 200, 5), StructureBlockEntity.UpdateType.LOAD_AREA, StructureMode.LOAD,
                repeat('n', 127), new BlockPos(0, 0, 0), new Vec3i(2, 2, 2),
                Mirror.NONE, Rotation.COUNTERCLOCKWISE_90, repeat('d', 128), 0.999F, 65535L,
                false, false, false, false);
        emit(codec, "byte_truncation_edges",
                bp(0, 1, 0), StructureBlockEntity.UpdateType.SCAN_AREA, StructureMode.CORNER,
                "edge", new BlockPos(-128, 127, -1), new Vec3i(255 - 256, 200 - 256, 48),
                Mirror.FRONT_BACK, Rotation.CLOCKWISE_180, "edge2", 0.0F, 2147483648L,
                true, true, true, true);
    }

    static BlockPos bp(int x, int y, int z) {
        return new BlockPos(x, y, z);
    }

    static String repeat(char c, int n) {
        StringBuilder sb = new StringBuilder(n);
        for (int i = 0; i < n; i++) sb.append(c);
        return sb.toString();
    }

    static String toHex(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte x : b) sb.append(String.format("%02x", x & 0xFF));
        return sb.toString();
    }

    static String utf8Hex(String s) {
        return toHex(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundSetStructureBlockPacket> codec,
                     String caseName, BlockPos pos,
                     StructureBlockEntity.UpdateType updateType, StructureMode mode,
                     String name, BlockPos offset, Vec3i size,
                     Mirror mirror, Rotation rotation, String data,
                     float integrity, long seed,
                     boolean ignoreEntities, boolean strict, boolean showAir, boolean showBoundingBox)
            throws Exception {
        ServerboundSetStructureBlockPacket pkt = new ServerboundSetStructureBlockPacket(
                pos, updateType, mode, name, offset, size, mirror, rotation, data,
                ignoreEntities, strict, showAir, showBoundingBox, integrity, seed);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);

        int n = buf.readableBytes();
        byte[] bytes = new byte[n];
        buf.getBytes(buf.readerIndex(), bytes);

        // Round-trip decode sanity through the real codec. Note: the decode clamps
        // offset/size into [-48,48]/[0,48] and integrity into [0,1] and re-truncates
        // the seed losslessly, so we only assert the codec-stable fields here.
        FriendlyByteBuf back = new FriendlyByteBuf(Unpooled.wrappedBuffer(bytes));
        ServerboundSetStructureBlockPacket dec = codec.decode(back);
        if (dec.getPos().asLong() != pos.asLong()
                || dec.getUpdateType() != updateType
                || dec.getMode() != mode
                || !dec.getName().equals(name)
                || dec.getMirror() != mirror
                || dec.getRotation() != rotation
                || !dec.getData().equals(data)
                || dec.getSeed() != seed
                || dec.isIgnoreEntities() != ignoreEntities
                || dec.isStrict() != strict
                || dec.isShowAir() != showAir
                || dec.isShowBoundingBox() != showBoundingBox) {
            throw new IllegalStateException("round-trip mismatch for " + caseName);
        }

        int flags = 0;
        if (ignoreEntities) flags |= 1;
        if (showAir) flags |= 2;
        if (showBoundingBox) flags |= 4;
        if (strict) flags |= 8;

        O.println("ENC\t" + caseName
                + "\t" + pos.asLong()
                + "\t" + updateType.ordinal()
                + "\t" + mode.ordinal()
                + "\t" + utf8Hex(name)
                + "\t" + (byte) offset.getX()
                + "\t" + (byte) offset.getY()
                + "\t" + (byte) offset.getZ()
                + "\t" + (byte) size.getX()
                + "\t" + (byte) size.getY()
                + "\t" + (byte) size.getZ()
                + "\t" + mirror.ordinal()
                + "\t" + rotation.ordinal()
                + "\t" + utf8Hex(data)
                + "\t" + Float.floatToRawIntBits(integrity)
                + "\t" + seed
                + "\t" + flags
                + "\t" + n
                + "\t" + toHex(bytes));
    }
}
