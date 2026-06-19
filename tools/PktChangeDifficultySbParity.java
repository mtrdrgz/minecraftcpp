// Ground truth for net.minecraft.network.protocol.game.ServerboundChangeDifficultyPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL source:
//   ServerboundChangeDifficultyPacket.java (record(Difficulty difficulty)):
//     STREAM_CODEC = StreamCodec.composite(
//         Difficulty.STREAM_CODEC, ::difficulty,    -> the Difficulty
//         ::new)
//   So this packet carries EXACTLY ONE field: the Difficulty (no boolean, no
//   trailing data — unlike the clientbound variant which also has `locked`).
//   Difficulty.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, Difficulty::getId)
//     (net.minecraft.world.Difficulty lines 19-20). idMapper.encode does
//     VarInt.write(output, toId.applyAsInt(value)) -> VarInt(getId())
//     (ByteBufCodecs.idMapper lines 542-553). So the difficulty is a VarInt of
//     getId(), NOT writeEnum(ordinal) and NOT a raw fixed byte. getId() values:
//       PEACEFUL=0, EASY=1, NORMAL=2, HARD=3 (Difficulty lines 13-16) — these
//       happen to equal the ordinals, but the wire value is getId() per the codec.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <readableBytes-dec> <hexBytes>
// <id-dec> is Difficulty.getId() (the VarInt value the codec writes); <hexBytes> is
// the full lowercase-hex packet payload. The C++ side replays writeVarInt(id) through
// mc::net::PacketBuffer and must match <hexBytes> byte-for-byte AND <readableBytes>,
// then decode the bytes back and round-trip.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ServerboundChangeDifficultyPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.Difficulty;

public class PktChangeDifficultySbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Finite / physical battery: every Difficulty constant (id 0..3). This
        // exhausts the entire value space the packet can carry — the codec is a
        // single VarInt(getId()) and getId() is only ever 0..3.
        Difficulty[] difficulties = Difficulty.values(); // PEACEFUL,EASY,NORMAL,HARD

        int caseNo = 0;
        for (Difficulty d : difficulties) {
            emit("c" + (caseNo++) + "_" + d.getSerializedName(), d);
        }
    }

    static void emit(String name, Difficulty difficulty) {
        ServerboundChangeDifficultyPacket pkt =
            new ServerboundChangeDifficultyPacket(difficulty);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ServerboundChangeDifficultyPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++)
            hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec (sanity): field must match exactly.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ServerboundChangeDifficultyPacket back =
            ServerboundChangeDifficultyPacket.STREAM_CODEC.decode(rb);
        if (back.difficulty() != difficulty) {
            throw new IllegalStateException(
                "round-trip difficulty mismatch: " + back.difficulty() + " != " + difficulty
                + " for " + name);
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
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
