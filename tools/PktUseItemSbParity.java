// Ground truth for net.minecraft.network.protocol.game.ServerboundUseItemPacket's
// StreamCodec.
//
// The packet (ServerboundUseItemPacket.java) has four fields:
//   InteractionHand hand; int sequence; float yRot; float xRot
// Its STREAM_CODEC = Packet.codec(ServerboundUseItemPacket::write, new)  (line 10-12),
// which is body-only (no packet-id / length prefix). The write body, IN THIS EXACT
// ORDER (lines 32-37), is:
//   output.writeEnum(this.hand)     == writeVarInt(hand.ordinal())   (FriendlyByteBuf.java:471-473)
//   output.writeVarInt(this.sequence)                                 // LEB128 VarInt
//   output.writeFloat(this.yRot)                                      // big-endian IEEE-754 4 bytes
//   output.writeFloat(this.xRot)                                      // big-endian IEEE-754 4 bytes
// and the read (lines 25-30) mirrors it: readEnum(InteractionHand.class) ++ readVarInt()
//   ++ readFloat() ++ readFloat().  readEnum(clazz) == clazz.getEnumConstants()[readVarInt()]
//   (FriendlyByteBuf.java:467-468).
//
// InteractionHand (net.minecraft.world.InteractionHand.java:10-13): MAIN_HAND ordinal 0,
// OFF_HAND ordinal 1. NOTE: the packet uses writeEnum/readEnum (the ORDINAL), not the
// enum's own idMapper STREAM_CODEC; ordinal == id here so the wire byte coincides, but
// the wire value emitted is the ordinal.
//
// All four fields are plain primitives (enum-ordinal VarInt / VarInt / float / float) --
// no registry/ItemStack/Component/Holder/SoundEvent/NBT -- so this is fully representable
// by the certified PacketBuffer (FriendlyByteBuf) port.
//
// Row formats (tab separated). yRot/xRot are emitted as %08x of their raw int bits
// (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly; hand-ordinal/sequence as
// decimal; hex columns are lowercase hex:
//   ENUM <ordinal> <name>                                    per InteractionHand constant
//   ENC  <handOrdinal> <sequence> <yRotRawBitsHex> <xRotRawBitsHex> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC  <hexBytes> <handOrd_in> <seq_in> <yRotBits_in> <xRotBits_in> \
//        <handOrd_dec> <seq_dec> <yRotBits_dec> <xRotBits_dec>
//        decode: STREAM_CODEC.decode(buf) -> field round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundUseItemPacket;
import net.minecraft.world.InteractionHand;

public class PktUseItemSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Pin the enum's ordinals/names -- the wire value is the ordinal.
        for (InteractionHand h : InteractionHand.values()) {
            O.print("ENUM\t");
            O.print(h.ordinal());
            O.print('\t');
            O.print(h.name());
            O.print('\n');
        }

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundUseItemPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundUseItemPacket>)
                ServerboundUseItemPacket.STREAM_CODEC;

        // Finite/physical battery for the int `sequence` VarInt: zero, small values,
        // and every LEB128 1->5 byte boundary (127/128 .. 268435455) plus max/min.
        int[] sequences = {
            0, 1, 2, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            Integer.MAX_VALUE, Integer.MIN_VALUE, -1,
        };

        // Finite/physical battery of rotation floats: the physical rotation range is
        // -180..180 / 0..360 degrees, plus sign / zero / boundary / special IEEE-754
        // floats to exercise the full 4-byte encoding.
        float[] rots = {
            0.0f, -0.0f, 1.0f, -1.0f, 45.0f, 90.0f, 180.0f, -180.0f, 270.0f, 360.0f,
            0.5f, 3.14159265f, 89.9f, -89.9f, 123.456f, -123.456f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };

        InteractionHand[] hands = InteractionHand.values();

        // Cross hand x sequence; pair the float lists index-wise for yRot/xRot (reversed)
        // so every float bit pattern lands in both float slots. This keeps the matrix
        // bounded while covering all enum/VarInt-boundary/float cases.
        for (int s = 0; s < sequences.length; s++) {
            int sequence = sequences[s];
            for (InteractionHand hand : hands) {
                // Rotate which float pair we use so each (sequence,hand) row hits a
                // different float index; over the whole battery all rots are exercised.
                int fi = (s * hands.length + hand.ordinal()) % rots.length;
                float yRot = rots[fi];
                float xRot = rots[rots.length - 1 - fi];

                ServerboundUseItemPacket pkt =
                    new ServerboundUseItemPacket(hand, sequence, yRot, xRot);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                int yBits = Float.floatToRawIntBits(yRot);
                int xBits = Float.floatToRawIntBits(xRot);
                O.print("ENC\t");
                O.print(hand.ordinal());
                O.print('\t');
                O.print(sequence);
                O.print('\t');
                O.print(String.format("%08x", yBits));
                O.print('\t');
                O.print(String.format("%08x", xBits));
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ServerboundUseItemPacket dec = CODEC.decode(rbuf);
                int dyBits = Float.floatToRawIntBits(dec.getYRot());
                int dxBits = Float.floatToRawIntBits(dec.getXRot());
                if (dec.getHand().ordinal() != hand.ordinal()
                    || dec.getSequence() != sequence
                    || dyBits != yBits || dxBits != xBits)
                    throw new IllegalStateException("round-trip mismatch for " + hex);
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(hand.ordinal());
                O.print('\t');
                O.print(sequence);
                O.print('\t');
                O.print(String.format("%08x", yBits));
                O.print('\t');
                O.print(String.format("%08x", xBits));
                O.print('\t');
                O.print(dec.getHand().ordinal());
                O.print('\t');
                O.print(dec.getSequence());
                O.print('\t');
                O.print(String.format("%08x", dyBits));
                O.print('\t');
                O.print(String.format("%08x", dxBits));
                O.print('\n');
            }
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
