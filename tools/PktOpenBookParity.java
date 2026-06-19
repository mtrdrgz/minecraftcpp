// Ground truth for ClientboundOpenBookPacket's StreamCodec.
//
// The packet body is exactly FriendlyByteBuf.writeEnum(hand) on encode and
// readEnum(InteractionHand.class) on decode
// (net.minecraft.network.protocol.game.ClientboundOpenBookPacket lines 19-25):
//   private ClientboundOpenBookPacket(FriendlyByteBuf input) { hand = input.readEnum(InteractionHand.class); }
//   private void write(FriendlyByteBuf output) { output.writeEnum(this.hand); }
// and STREAM_CODEC = Packet.codec(ClientboundOpenBookPacket::write, ClientboundOpenBookPacket::new).
//
//   writeEnum(Enum) == writeVarInt(value.ordinal())          (FriendlyByteBuf.java:471-473)
//   readEnum(clazz) == clazz.getEnumConstants()[readVarInt()] (FriendlyByteBuf.java:467-468)
//
// NOTE: the packet uses FriendlyByteBuf.writeEnum (ordinal as VarInt), NOT
// InteractionHand.STREAM_CODEC (the idMapper). For InteractionHand the ordinal and
// the id happen to coincide (MAIN_HAND ordinal 0 / OFF_HAND ordinal 1), but the
// wire format that matters here is the ordinal VarInt written by writeEnum.
// InteractionHand has exactly two constants (net.minecraft.world.InteractionHand.java:10-13):
//   MAIN_HAND (ordinal 0), OFF_HAND (ordinal 1). So the wire body is a single
//   VarInt of the ordinal: 0x00 for MAIN_HAND, 0x01 for OFF_HAND. These two are the
//   ONLY physically valid inputs (the full domain of the enum).
//
// Row formats (tab separated). The runner writes the TSV as ASCII; ordinals are decimal:
//   ENUM <ordinal>                 <name>                            per InteractionHand constant
//   ENC  <name> <ordinal> <readableBytes> <hexBytes>                 encode: STREAM_CODEC.encode(buf, packet)
//   DEC  <hexBytes> <ordinal_in> <ordinal_decoded>                   decode: STREAM_CODEC.decode(buf).getHand().ordinal()
//
// The C++ side encodes writeVarInt(ordinal) and decodes readVarInt() (the certified
// PacketBuffer == FriendlyByteBuf port) and must match byte-for-byte (ENC) and
// value-for-value (DEC), plus a PacketBuffer round-trip on the expected bytes.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundOpenBookPacket;
import net.minecraft.world.InteractionHand;

public class PktOpenBookParity {
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
        StreamCodec<FriendlyByteBuf, ClientboundOpenBookPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundOpenBookPacket>)
                ClientboundOpenBookPacket.STREAM_CODEC;

        // Finite/physical inputs: both legal hands (the entire enum domain).
        for (InteractionHand hand : InteractionHand.values()) {
            // ENC: encode through the real codec, dump readableBytes + the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ClientboundOpenBookPacket pkt = new ClientboundOpenBookPacket(hand);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);
            O.print("ENC\t");
            O.print(hand.name());
            O.print('\t');
            O.print(hand.ordinal());
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');

            // DEC (round-trip sanity): decode the same bytes through the real codec.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundOpenBookPacket dec = CODEC.decode(rbuf);
            if (dec.getHand() != hand) {
                throw new IllegalStateException("round-trip mismatch: " + hand + " -> " + dec.getHand());
            }
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
