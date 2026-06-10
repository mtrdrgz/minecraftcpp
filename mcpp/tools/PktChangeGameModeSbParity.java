// Ground truth for ServerboundChangeGameModePacket's StreamCodec.
//
// net.minecraft.network.protocol.game.ServerboundChangeGameModePacket (26.1.2):
//   public record ServerboundChangeGameModePacket(GameType mode) ...
//   STREAM_CODEC = StreamCodec.composite(
//       GameType.STREAM_CODEC, ServerboundChangeGameModePacket::mode,
//       ServerboundChangeGameModePacket::new);
// So the wire form is exactly GameType.STREAM_CODEC applied to mode().
//
// GameType.STREAM_CODEC = ByteBufCodecs.idMapper(BY_ID, GameType::getId)
//   encode: VarInt.write(out, value.getId())              (ByteBufCodecs:549-552)
//   decode: BY_ID.apply(VarInt.read(in))                  (ByteBufCodecs:544-546)
// GameType ids (GameType.java 17-20):
//   SURVIVAL=0, CREATIVE=1, ADVENTURE=2, SPECTATOR=3
// Therefore the body is a single VarInt of the GameType id.
//
// Row format (tab separated):
//   ENUM   <id> <name>                 per GameType constant (id gate)
//   ENC    <name> <id> <readableBytes> <hexBytes>   STREAM_CODEC.encode(buf, pkt)
//   DEC    <hexBytes> <id_in> <id_out>              STREAM_CODEC.decode(buf).mode().getId()
//
// The C++ side encodes writeVarInt(id) and decodes readVarInt() and must match
// byte-for-byte (ENC) and value-for-value (DEC).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundChangeGameModePacket;
import net.minecraft.world.level.GameType;

public class PktChangeGameModeSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet. STREAM_CODEC is typed
        // StreamCodec<ByteBuf, ...>; FriendlyByteBuf is a ByteBuf so this is safe.
        @SuppressWarnings("unchecked")
        StreamCodec<ByteBuf, ServerboundChangeGameModePacket> CODEC =
            (StreamCodec<ByteBuf, ServerboundChangeGameModePacket>)
                ServerboundChangeGameModePacket.STREAM_CODEC;

        GameType[] modes = GameType.values();

        // Id gate: dump getId()+name() for every GameType constant in declaration order.
        for (GameType m : modes) {
            O.print("ENUM\t");
            O.print(m.getId());
            O.print('\t');
            O.print(m.name());
            O.print('\n');
        }

        // ENC / DEC: round-trip every GameType constant through the REAL STREAM_CODEC.
        for (GameType m : modes) {
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundChangeGameModePacket pkt = new ServerboundChangeGameModePacket(m);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);
            O.print("ENC\t");
            O.print(m.name());
            O.print('\t');
            O.print(m.getId());
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');

            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundChangeGameModePacket dec = CODEC.decode(rbuf);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(m.getId());
            O.print('\t');
            O.print(dec.mode().getId());
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
