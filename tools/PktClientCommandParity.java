// Ground truth for ServerboundClientCommandPacket's StreamCodec.
//
// The packet body is exactly FriendlyByteBuf.writeEnum(action) on encode and
// readEnum(Action.class) on decode
// (net.minecraft.network.protocol.game.ServerboundClientCommandPacket lines 18-24).
//   writeEnum(value)        = writeVarInt(value.ordinal())          (FriendlyByteBuf:471-473)
//   readEnum(Action.class)  = getEnumConstants()[readVarInt()]      (FriendlyByteBuf:467-469)
// So the wire form is just a VarInt of the enum ordinal:
//   PERFORM_RESPAWN=0, REQUEST_STATS=1, REQUEST_GAMERULE_VALUES=2
//   (ServerboundClientCommandPacket.Action declaration order, lines 39-43).
//
// Row format (tab separated):
//   ENUM   <ordinal> <name>                   per Action constant (enum gate)
//   ENC    <ordinal> <hex>                     encode: STREAM_CODEC.encode(buf, packet)
//   DEC    <hex> <ordinal_in> <ordinal_out>    decode: STREAM_CODEC.decode(buf).getAction().ordinal()
//
// The C++ side encodes writeVarInt(ordinal) and decodes readVarInt() and must
// match byte-for-byte (ENC) and value-for-value (DEC).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundClientCommandPacket;

public class PktClientCommandParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ServerboundClientCommandPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundClientCommandPacket>)
                ServerboundClientCommandPacket.STREAM_CODEC;

        // Enum gate: dump ordinal()+name() for every Action constant in declaration order.
        ServerboundClientCommandPacket.Action[] actions =
            ServerboundClientCommandPacket.Action.values();
        for (ServerboundClientCommandPacket.Action a : actions) {
            O.print("ENUM\t");
            O.print(a.ordinal());
            O.print('\t');
            O.print(a.name());
            O.print('\n');
        }

        // ENC / DEC: round-trip every Action constant through the REAL STREAM_CODEC.
        for (ServerboundClientCommandPacket.Action a : actions) {
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundClientCommandPacket pkt = new ServerboundClientCommandPacket(a);
            CODEC.encode(buf, pkt);
            String hex = toHex(buf);
            O.print("ENC\t");
            O.print(a.ordinal());
            O.print('\t');
            O.print(hex);
            O.print('\n');

            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundClientCommandPacket dec = CODEC.decode(rbuf);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(a.ordinal());
            O.print('\t');
            O.print(dec.getAction().ordinal());
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
