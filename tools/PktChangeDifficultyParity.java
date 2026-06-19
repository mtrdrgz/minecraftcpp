// Ground truth for net.minecraft.network.protocol.game.ClientboundChangeDifficultyPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL source:
//   ClientboundChangeDifficultyPacket.java (record(Difficulty difficulty, boolean locked)):
//     STREAM_CODEC = StreamCodec.composite(
//         Difficulty.STREAM_CODEC, ::difficulty,    -> the Difficulty
//         ByteBufCodecs.BOOL,      ::locked,        -> a single-byte boolean
//         ::new)
//   Difficulty.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Difficulty::getId)
//     (net.minecraft.world.Difficulty lines 19-20). idMapper.encode does
//     VarInt.write(output, toId.applyAsInt(value)) -> VarInt(getId())
//     (ByteBufCodecs.idMapper lines 542-553). So the difficulty is a VarInt of
//     getId(), NOT writeEnum(ordinal) and NOT a raw fixed byte. getId() values:
//       PEACEFUL=0, EASY=1, NORMAL=2, HARD=3 (Difficulty lines 13-16) — these
//       happen to equal the ordinals, but the wire value is getId() per the codec.
//   ByteBufCodecs.BOOL = a single byte (0/1) -> FriendlyByteBuf.writeBoolean.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <locked-0|1> <readableBytes-dec> <hexBytes>
// <id-dec> is Difficulty.getId() (the VarInt value the codec writes); <locked> is the
// boolean as 0/1; <hexBytes> the full lowercase-hex packet payload. The C++ side replays
// writeVarInt(id) + writeBoolean(locked) through mc::net::PacketBuffer and must match
// <hexBytes> byte-for-byte AND <readableBytes>, then decode the bytes back and round-trip.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundChangeDifficultyPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.Difficulty;

public class PktChangeDifficultyParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Finite / physical battery: every Difficulty constant (id 0..3) x both
        // locked states. This exhausts the entire value space the packet can carry.
        Difficulty[] difficulties = Difficulty.values(); // PEACEFUL,EASY,NORMAL,HARD
        boolean[] lockedStates = { false, true };

        int caseNo = 0;
        for (Difficulty d : difficulties) {
            for (boolean locked : lockedStates) {
                emit("c" + (caseNo++) + "_" + d.getSerializedName() + (locked ? "_locked" : "_unlocked"),
                     d, locked);
            }
        }
    }

    static void emit(String name, Difficulty difficulty, boolean locked) {
        ClientboundChangeDifficultyPacket pkt =
            new ClientboundChangeDifficultyPacket(difficulty, locked);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundChangeDifficultyPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++)
            hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec (sanity): both fields must match exactly.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundChangeDifficultyPacket back =
            ClientboundChangeDifficultyPacket.STREAM_CODEC.decode(rb);
        if (back.difficulty() != difficulty) {
            throw new IllegalStateException(
                "round-trip difficulty mismatch: " + back.difficulty() + " != " + difficulty
                + " for " + name);
        }
        if (back.locked() != locked) {
            throw new IllegalStateException(
                "round-trip locked mismatch: " + back.locked() + " != " + locked + " for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes()
                + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(difficulty.getId());   // the VarInt value the codec writes (getId())
        O.print('\t');
        O.print(locked ? 1 : 0);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
