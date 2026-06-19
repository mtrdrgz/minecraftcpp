// Ground truth for net.minecraft.network.protocol.game.ServerboundPlayerActionPacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet body is exactly (ServerboundPlayerActionPacket.java:37-42):
//   write(FriendlyByteBuf output):
//     output.writeEnum(this.action);                  // == writeVarInt(action.ordinal())  (FriendlyByteBuf.java:471-473)
//     output.writeBlockPos(this.pos);                 // == writeLong(pos.asLong())         (FriendlyByteBuf.java:398-400)
//     output.writeByte(this.direction.get3DDataValue()); // 1 byte, low 8 bits (0..5)
//     output.writeVarInt(this.sequence);              // VarInt
//   read(FriendlyByteBuf input)  (ServerboundPlayerActionPacket.java:30-35):
//     this.action    = input.readEnum(Action.class);              // VarInt ordinal -> Action.values()[i]
//     this.pos       = input.readBlockPos();                      // BlockPos.of(input.readLong())
//     this.direction = Direction.from3DDataValue(input.readUnsignedByte()); // 0..255 -> from3DDataValue
//     this.sequence  = input.readVarInt();                        // VarInt
//
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, NO packet-id or
// length prefix on the wire.
//
// Action enum ordinals (ServerboundPlayerActionPacket.java:69-78):
//   START_DESTROY_BLOCK=0, ABORT_DESTROY_BLOCK=1, STOP_DESTROY_BLOCK=2,
//   DROP_ALL_ITEMS=3, DROP_ITEM=4, RELEASE_USE_ITEM=5, SWAP_ITEM_WITH_OFFHAND=6, STAB=7.
//
// Direction.get3DDataValue (Direction.java:33-38,155-157): the first ctor arg, i.e.
//   DOWN=0, UP=1, NORTH=2, SOUTH=3, WEST=4, EAST=5.
// Direction.from3DDataValue wraps the unsigned byte modulo 6 (BY_ID continuous WRAP).
//
// BlockPos.asLong (BlockPos.java:107-116) packs a single big-endian long:
//   PACKED_HORIZONTAL_LENGTH = 26, PACKED_Y_LENGTH = 12,
//   X_OFFSET = 38, Z_OFFSET = 12, Y_OFFSET = 0;
//   node = ((x & 0x3FFFFFF) << 38) | ((z & 0x3FFFFFF) << 12) | (y & 0xFFF).
// writeLong is big-endian 8 bytes (FriendlyByteBuf -> netty).
//
// Row format (tab separated). Every field that is not a String/binary is decimal;
// hexBytes is lowercase %02x of the encoded body.
//   ENC \t <name> \t <actionOrdinal> \t <x> \t <y> \t <z> \t <dir3d> \t <sequence> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeVarInt(actionOrdinal) + writeLong(asLong(x,y,z)) + writeByte(dir3d) + writeVarInt(sequence))
// and must match byte-for-byte, then round-trips the bytes back to all fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPlayerActionPacket;

public class PktPlayerActionSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundPlayerActionPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPlayerActionPacket>)
                ServerboundPlayerActionPacket.STREAM_CODEC;

        ServerboundPlayerActionPacket.Action[] ACTIONS =
            ServerboundPlayerActionPacket.Action.values();      // ordinal-indexed (8 actions)
        Direction[] DIRS = Direction.values();                  // declaration order: DOWN,UP,NORTH,SOUTH,WEST,EAST

        // Finite / physical battery. Columns: name, actionOrdinal, x, y, z, dir3d, sequence.
        // actionOrdinal: 0..7 (all actions). dir3d: get3DDataValue 0..5 (all directions).
        // x/z are 26-bit signed (range -33554432..33554431); y is 12-bit (-2048..2047).
        // sequence exercises VarInt 1->2->3->4->5 byte boundaries and sign.
        int[][] cases = {
            // {actionOrdinal, x, y, z, dirIndex(by 3d value), sequence}
            {0, 0, 0, 0, 0, 0},
            {1, 1, 1, 1, 1, 1},
            {2, 100, 64, -100, 2, 127},
            {3, -1, -1, -1, 3, 128},
            {4, 12345678, 320, -8765432, 4, 16383},
            {5, -30000000, -64, 30000000, 5, 16384},
            {6, 30000000, 2047, -30000000, 0, 2097151},
            {7, -33554432, -2048, 33554431, 1, 2097152},
            {0, 33554431, 2047, -33554432, 2, 268435455},
            {1, 0, 0, 0, 3, 2147483647},     // Integer.MAX_VALUE sequence (5-byte VarInt)
            {2, -33554432, -2048, -33554432, 4, -1},   // sequence=-1 -> 5-byte VarInt ff ff ff ff 0f
            {3, 1, -1, 2, 5, -2147483648},   // Integer.MIN_VALUE sequence (5-byte VarInt)
            {7, 8388607, 100, -8388608, 0, 268435456}, // sequence 2^28 -> 5-byte VarInt boundary
            {6, 250, 5, 250, 5, 16384},
        };

        for (int[] c : cases) {
            int actOrd = c[0], x = c[1], y = c[2], z = c[3], dirIdx = c[4], sequence = c[5];
            ServerboundPlayerActionPacket.Action action = ACTIONS[actOrd];
            // Pick the Direction whose get3DDataValue == dirIdx (DIRS is declared in 3d-value order).
            Direction direction = DIRS[dirIdx];
            int dir3d = direction.get3DDataValue();
            BlockPos pos = new BlockPos(x, y, z);

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundPlayerActionPacket pkt =
                new ServerboundPlayerActionPacket(action, pos, direction, sequence);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; sanity-assert field equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundPlayerActionPacket dec = CODEC.decode(rbuf);
            if (dec.getAction() != action)
                throw new IllegalStateException("action roundtrip " + dec.getAction() + " != " + action);
            long expLong = BlockPos.asLong(x, y, z);
            if (dec.getPos().asLong() != expLong)
                throw new IllegalStateException("pos roundtrip " + dec.getPos().asLong() + " != " + expLong);
            if (dec.getDirection() != direction)
                throw new IllegalStateException("direction roundtrip " + dec.getDirection() + " != " + direction);
            if (dec.getSequence() != sequence)
                throw new IllegalStateException("sequence roundtrip " + dec.getSequence() + " != " + sequence);

            String name = "case_a" + actOrd + "_x" + x + "_y" + y + "_z" + z
                + "_d" + dir3d + "_s" + sequence;
            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(actOrd);
            O.print('\t');
            O.print(x);
            O.print('\t');
            O.print(y);
            O.print('\t');
            O.print(z);
            O.print('\t');
            O.print(dir3d);
            O.print('\t');
            O.print(sequence);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
