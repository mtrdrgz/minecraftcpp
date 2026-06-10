// Ground truth for ServerboundSwingPacket's StreamCodec.
//
// The packet body is exactly FriendlyByteBuf.writeEnum(hand) on encode and
// readEnum(InteractionHand.class) on decode
// (net.minecraft.network.protocol.game.ServerboundSwingPacket lines 19-25).
//   writeEnum(Enum) == writeVarInt(value.ordinal())   (FriendlyByteBuf.java:471-473)
//   readEnum(clazz) == clazz.getEnumConstants()[readVarInt()]  (FriendlyByteBuf.java:467-468)
// InteractionHand has two constants: MAIN_HAND (ordinal 0), OFF_HAND (ordinal 1)
// (net.minecraft.world.InteractionHand.java:10-13). So the wire body is a single
// VarInt of the ordinal: 0x00 for MAIN_HAND, 0x01 for OFF_HAND.
//
// Row formats (tab separated):
//   ENUM <ordinal>      <name>                       per InteractionHand constant
//   ENC  <ordinal>      <hex>                         encode: STREAM_CODEC.encode(buf, packet)
//   DEC  <hex> <ord_in> <ordinal_decoded>             decode: STREAM_CODEC.decode(buf).getHand().ordinal()
//
// The C++ side encodes writeVarInt(ordinal) and decodes readVarInt() (the certified
// PacketBuffer == FriendlyByteBuf port) and must match byte-for-byte (ENC) and
// value-for-value (DEC).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSwingPacket;
import net.minecraft.world.InteractionHand;

public class PktSwingParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Pin the enum's ordinals/names — the wire value is the ordinal.
        for (InteractionHand h : InteractionHand.values()) {
            O.print("ENUM\t");
            O.print(h.ordinal());
            O.print('\t');
            O.print(h.name());
            O.print('\n');
        }

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundSwingPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSwingPacket>)
                ServerboundSwingPacket.STREAM_CODEC;

        // Finite/physical inputs: both legal hands.
        for (InteractionHand hand : InteractionHand.values()) {
            // ENC: encode through the real codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundSwingPacket pkt = new ServerboundSwingPacket(hand);
            CODEC.encode(buf, pkt);
            String hex = toHex(buf);
            O.print("ENC\t");
            O.print(hand.ordinal());
            O.print('\t');
            O.print(hex);
            O.print('\n');

            // DEC: decode the same bytes through the real codec, dump getHand().ordinal().
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundSwingPacket dec = CODEC.decode(rbuf);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(hand.ordinal());
            O.print('\t');
            O.print(dec.getHand().ordinal());
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
