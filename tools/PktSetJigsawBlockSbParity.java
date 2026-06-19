import java.io.PrintStream;

import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSetJigsawBlockPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.block.entity.JigsawBlockEntity;

/**
 * Ground-truth dump for net.minecraft.network.protocol.game.ServerboundSetJigsawBlockPacket.
 *
 * Real codec order (ServerboundSetJigsawBlockPacket.write, 26.1.2):
 *   output.writeBlockPos(pos)                    -> writeLong(pos.asLong())  (big-endian 8-byte long)
 *   output.writeIdentifier(name)                 -> writeUtf(name.toString())   = VarInt(byteLen)+UTF-8
 *   output.writeIdentifier(target)               -> writeUtf(target.toString())
 *   output.writeIdentifier(pool)                 -> writeUtf(pool.toString())
 *   output.writeUtf(finalState)                  -> VarInt(byteLen)+UTF-8
 *   output.writeUtf(joint.getSerializedName())   -> "rollable" (ROLLABLE) | "aligned" (ALIGNED)
 *   output.writeVarInt(selectionPriority)
 *   output.writeVarInt(placementPriority)
 *
 * Identifier.toString() == "namespace:path"; a bare "path" parses to "minecraft:path".
 * JointType.getSerializedName(): ROLLABLE="rollable", ALIGNED="aligned".
 *
 * Row format (tab-separated, all strings emitted as UTF-8 hex so the ASCII TSV is safe):
 *   ENC \t name \t posLong(dec) \t nameHex \t targetHex \t poolHex \t finalStateHex
 *       \t jointName \t selectionPriority(dec) \t placementPriority(dec)
 *       \t readableBytes(dec) \t hexBytes
 */
public class PktSetJigsawBlockSbParity {
    static final PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundSetJigsawBlockPacket> codec =
                ServerboundSetJigsawBlockPacket.STREAM_CODEC;

        // Battery: coordinate extremes/signs, both joint enum values, empty + ASCII +
        // multibyte-UTF-8 finalState, namespaced vs bare identifiers, VarInt-length
        // boundaries on the string lengths, negative/zero/large priorities.
        emit(codec, "defaults_empty",
                bp(0, 0, 0),
                Identifier.withDefaultNamespace("empty"),
                Identifier.withDefaultNamespace("empty"),
                Identifier.withDefaultNamespace("empty"),
                "", JigsawBlockEntity.JointType.ALIGNED, 0, 0);
        emit(codec, "rollable_simple",
                bp(1, 2, 3),
                Identifier.parse("minecraft:bottom"),
                Identifier.parse("minecraft:top"),
                Identifier.parse("minecraft:village/plains/houses"),
                "minecraft:oak_planks", JigsawBlockEntity.JointType.ROLLABLE, 1, 2);
        emit(codec, "neg_coords",
                bp(-1, -1, -1),
                Identifier.parse("namespace:name"),
                Identifier.parse("namespace:target"),
                Identifier.parse("namespace:pool"),
                "stone", JigsawBlockEntity.JointType.ALIGNED, -1, -1);
        emit(codec, "maxxyz",
                bp(33554431, 2047, 33554431),
                Identifier.parse("a:b"),
                Identifier.parse("c:d"),
                Identifier.parse("e:f"),
                "x", JigsawBlockEntity.JointType.ROLLABLE, 2147483647, 0);
        emit(codec, "minxyz",
                bp(-33554432, -2048, -33554432),
                Identifier.parse("a:b"),
                Identifier.parse("c:d"),
                Identifier.parse("e:f"),
                "y", JigsawBlockEntity.JointType.ALIGNED, -2147483648, 100);
        emit(codec, "multibyte_finalstate",
                bp(10, 64, -20),
                Identifier.parse("mod:thing_one"),
                Identifier.parse("mod:thing_two"),
                Identifier.parse("mod:pool/sub_pool"),
                "minecraft:note_block[note=12,instrument=harp] éü中文😀",
                JigsawBlockEntity.JointType.ROLLABLE, 5, 7);
        emit(codec, "bare_default_namespace",
                bp(100, 70, 100),
                Identifier.withDefaultNamespace("bottom"),
                Identifier.withDefaultNamespace("top"),
                Identifier.withDefaultNamespace("empty"),
                "minecraft:air", JigsawBlockEntity.JointType.ALIGNED, 10, -10);
        emit(codec, "len127_boundary",
                bp(7, 8, 9),
                Identifier.parse("ns:a"),
                Identifier.parse("ns:b"),
                Identifier.parse("ns:c"),
                repeat('a', 127), JigsawBlockEntity.JointType.ROLLABLE, 123, 456);
        emit(codec, "len128_boundary",
                bp(7, 8, 9),
                Identifier.parse("ns:a"),
                Identifier.parse("ns:b"),
                Identifier.parse("ns:c"),
                repeat('b', 128), JigsawBlockEntity.JointType.ALIGNED, 256, 512);
        emit(codec, "long_pool_path",
                bp(-5, 200, 5),
                Identifier.parse("minecraft:jigsaw"),
                Identifier.parse("minecraft:jigsaw"),
                Identifier.parse("minecraft:bastion/units/center_pieces"),
                "minecraft:structure_block[mode=data]",
                JigsawBlockEntity.JointType.ROLLABLE, 0, 1);
    }

    static net.minecraft.core.BlockPos bp(int x, int y, int z) {
        return new net.minecraft.core.BlockPos(x, y, z);
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

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundSetJigsawBlockPacket> codec,
                     String caseName, net.minecraft.core.BlockPos pos,
                     Identifier name, Identifier target, Identifier pool,
                     String finalState, JigsawBlockEntity.JointType joint,
                     int selectionPriority, int placementPriority) throws Exception {
        ServerboundSetJigsawBlockPacket pkt = new ServerboundSetJigsawBlockPacket(
                pos, name, target, pool, finalState, joint, selectionPriority, placementPriority);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);

        int n = buf.readableBytes();
        byte[] bytes = new byte[n];
        buf.getBytes(buf.readerIndex(), bytes);

        // Round-trip decode sanity through the real codec.
        FriendlyByteBuf back = new FriendlyByteBuf(Unpooled.wrappedBuffer(bytes));
        ServerboundSetJigsawBlockPacket dec = codec.decode(back);
        if (dec.getPos().asLong() != pos.asLong()
                || !dec.getName().equals(name)
                || !dec.getTarget().equals(target)
                || !dec.getPool().equals(pool)
                || !dec.getFinalState().equals(finalState)
                || dec.getJoint() != joint
                || dec.getSelectionPriority() != selectionPriority
                || dec.getPlacementPriority() != placementPriority) {
            throw new IllegalStateException("round-trip mismatch for " + caseName);
        }

        O.println("ENC\t" + caseName
                + "\t" + pos.asLong()
                + "\t" + utf8Hex(name.toString())
                + "\t" + utf8Hex(target.toString())
                + "\t" + utf8Hex(pool.toString())
                + "\t" + utf8Hex(finalState)
                + "\t" + joint.getSerializedName()
                + "\t" + selectionPriority
                + "\t" + placementPriority
                + "\t" + n
                + "\t" + toHex(bytes));
    }
}
