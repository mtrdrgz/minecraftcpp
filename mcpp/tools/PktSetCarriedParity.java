// Ground truth for ServerboundSetCarriedItemPacket's StreamCodec.
//
// The packet body is exactly FriendlyByteBuf.writeShort(slot) on encode and
// readShort() on decode (net.minecraft.network.protocol.game.ServerboundSetCarriedItemPacket
// lines 18-24). writeShort(int) writes only the low 16 bits big-endian; readShort()
// returns a SIGNED 16-bit short sign-extended to int. We exercise that truncation /
// sign behavior by round-tripping a battery of int slots through the REAL STREAM_CODEC.
//
// Row format (tab separated):
//   ENC <slot_in>            <hex>             encode: STREAM_CODEC.encode(buf, packet)
//   DEC <hex> <slot_in>      <slot_decoded>    decode: STREAM_CODEC.decode(buf) -> getSlot()
//
// The C++ side encodes writeShort(slot) and decodes readShort() and must match
// byte-for-byte (ENC) and value-for-value (DEC, the sign-extended low 16 bits).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSetCarriedItemPacket;

public class PktSetCarriedParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ServerboundSetCarriedItemPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSetCarriedItemPacket>)
                ServerboundSetCarriedItemPacket.STREAM_CODEC;

        // Finite/physical inputs: real hotbar slots 0..8, the short bounds, and a
        // handful of out-of-range ints to pin the low-16-bit truncation + sign rules.
        int[] slots = {
            0, 1, 2, 3, 4, 5, 6, 7, 8,
            255, 256, 32766, 32767,           // 0x7fff = max signed short
            -1, -2, -32768,                    // 0x8000 = min signed short
            32768, 65535, 65536, 65537,        // out-of-range: low 16 bits matter
            -32769, -65536, 100000,
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        for (int slot : slots) {
            // ENC: encode through the real codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundSetCarriedItemPacket pkt = new ServerboundSetCarriedItemPacket(slot);
            CODEC.encode(buf, pkt);
            String hex = toHex(buf);
            O.print("ENC\t");
            O.print(slot);
            O.print('\t');
            O.print(hex);
            O.print('\n');

            // DEC: decode the same bytes through the real codec, dump getSlot().
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundSetCarriedItemPacket dec = CODEC.decode(rbuf);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(slot);
            O.print('\t');
            O.print(dec.getSlot());
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
