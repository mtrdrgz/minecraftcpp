import java.io.PrintStream;

import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSetCommandBlockPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.block.entity.CommandBlockEntity;

/**
 * Ground-truth dump for net.minecraft.network.protocol.game.ServerboundSetCommandBlockPacket.
 *
 * Real codec order (ServerboundSetCommandBlockPacket.write):
 *   output.writeBlockPos(pos)   -> writeLong(pos.asLong())  (big-endian 8-byte long)
 *   output.writeUtf(command)    -> VarInt(byteLen) + UTF-8 bytes
 *   output.writeEnum(mode)      -> VarInt(mode.ordinal())   SEQUENCE=0, AUTO=1, REDSTONE=2
 *   output.writeByte(flags)     -> low 8 bits; bit0=trackOutput, bit1=conditional, bit2=automatic
 *
 * Row format (tab-separated):
 *   ENC \t name \t posLong(dec) \t commandHex \t modeOrdinal(dec) \t flags(dec) \t readableBytes(dec) \t hexBytes
 */
public class PktSetCommandBlockSbParity {
    static final PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundSetCommandBlockPacket> codec =
                ServerboundSetCommandBlockPacket.STREAM_CODEC;

        // Battery: coordinate extremes / sign, all 3 modes, every flag combination,
        // ASCII + multibyte-UTF-8 + empty command strings, VarInt-length boundaries.
        emit(codec, "zero_seq_noflags",      new BlockPos(0, 0, 0),
                "", CommandBlockEntity.Mode.SEQUENCE, false, false, false);
        emit(codec, "origin_auto_track",     new BlockPos(0, 0, 0),
                "say hi", CommandBlockEntity.Mode.AUTO, true, false, false);
        emit(codec, "redstone_allflags",     new BlockPos(1, 2, 3),
                "/setblock ~ ~ ~ stone", CommandBlockEntity.Mode.REDSTONE, true, true, true);
        emit(codec, "neg_coords_cond",       new BlockPos(-1, -1, -1),
                "fill", CommandBlockEntity.Mode.SEQUENCE, false, true, false);
        emit(codec, "maxxyz",                new BlockPos(33554431, 2047, 33554431),
                "x", CommandBlockEntity.Mode.AUTO, false, false, true);
        emit(codec, "minxyz",                new BlockPos(-33554432, -2048, -33554432),
                "y", CommandBlockEntity.Mode.REDSTONE, true, false, true);
        emit(codec, "multibyte_utf8",        new BlockPos(10, 64, -20),
                "say éü中文😀", CommandBlockEntity.Mode.AUTO, true, true, false);
        emit(codec, "flags_track_only",      new BlockPos(100, 70, 100),
                "tp @p 0 100 0", CommandBlockEntity.Mode.SEQUENCE, true, false, false);
        emit(codec, "flags_cond_only",       new BlockPos(100, 70, 100),
                "tp @p 0 100 0", CommandBlockEntity.Mode.SEQUENCE, false, true, false);
        emit(codec, "flags_auto_only",       new BlockPos(100, 70, 100),
                "tp @p 0 100 0", CommandBlockEntity.Mode.SEQUENCE, false, false, true);
        emit(codec, "len127_boundary",       new BlockPos(7, 8, 9),
                repeat('a', 127), CommandBlockEntity.Mode.REDSTONE, true, true, true);
        emit(codec, "len128_boundary",       new BlockPos(7, 8, 9),
                repeat('b', 128), CommandBlockEntity.Mode.AUTO, false, true, true);
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

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundSetCommandBlockPacket> codec,
                     String name, BlockPos pos, String command, CommandBlockEntity.Mode mode,
                     boolean trackOutput, boolean conditional, boolean automatic) throws Exception {
        ServerboundSetCommandBlockPacket pkt = new ServerboundSetCommandBlockPacket(
                pos, command, mode, trackOutput, conditional, automatic);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);

        int n = buf.readableBytes();
        byte[] bytes = new byte[n];
        buf.getBytes(buf.readerIndex(), bytes);

        // Round-trip decode sanity through the real codec.
        FriendlyByteBuf back = new FriendlyByteBuf(Unpooled.wrappedBuffer(bytes));
        ServerboundSetCommandBlockPacket dec = codec.decode(back);
        if (dec.getPos().asLong() != pos.asLong()
                || !dec.getCommand().equals(command)
                || dec.getMode() != mode
                || dec.isTrackOutput() != trackOutput
                || dec.isConditional() != conditional
                || dec.isAutomatic() != automatic) {
            throw new IllegalStateException("round-trip mismatch for " + name);
        }

        int flags = 0;
        if (trackOutput) flags |= 1;
        if (conditional) flags |= 2;
        if (automatic)   flags |= 4;

        O.println("ENC\t" + name
                + "\t" + pos.asLong()
                + "\t" + utf8Hex(command)
                + "\t" + mode.ordinal()
                + "\t" + flags
                + "\t" + n
                + "\t" + toHex(bytes));
    }
}
