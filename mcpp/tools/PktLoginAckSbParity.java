// Ground truth for net.minecraft.network.protocol.login.ServerboundLoginAcknowledgedPacket.
//
// The packet's wire form is defined ENTIRELY by:
//     STREAM_CODEC = StreamCodec.unit(INSTANCE)
// (ServerboundLoginAcknowledgedPacket.java line 10). StreamCodec.unit
// (StreamCodec.java 49-63):
//     encode(output, value): if (!value.equals(instance)) throw ...;   // writes NOTHING
//     decode(input):         return instance;                          // reads NOTHING
// So this packet's body is EMPTY — it encodes to zero bytes and decodes by
// returning the singleton without consuming any bytes. There are no fields.
//
// Row format (tab separated):
//   ENC \t <name> \t <readableBytes> \t <hexBytes>
// where hexBytes is the empty string for the empty body. We round-trip decode
// through the SAME codec and assert the decoded instance is the singleton and
// that no bytes were consumed (sanity), matching the unit-codec contract.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.login.ServerboundLoginAcknowledgedPacket;

public class PktLoginAckSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The REAL StreamCodec for this packet. Bound type is ByteBuf (unit codec).
        @SuppressWarnings("unchecked")
        StreamCodec<ByteBuf, ServerboundLoginAcknowledgedPacket> CODEC =
            (StreamCodec<ByteBuf, ServerboundLoginAcknowledgedPacket>)
                ServerboundLoginAcknowledgedPacket.STREAM_CODEC;

        // The only physical value of this packet is the singleton INSTANCE.
        // Encode it (a few times to show determinism). All must be zero bytes.
        for (int rep = 0; rep < 3; rep++) {
            ByteBuf buf = Unpooled.buffer();
            CODEC.encode(buf, ServerboundLoginAcknowledgedPacket.INSTANCE);
            int n = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip sanity: decode from the SAME bytes through the SAME codec.
            ByteBuf rbuf = Unpooled.wrappedBuffer(unhex(hex));
            int before = rbuf.readerIndex();
            ServerboundLoginAcknowledgedPacket dec = CODEC.decode(rbuf);
            int consumed = rbuf.readerIndex() - before;
            if (dec != ServerboundLoginAcknowledgedPacket.INSTANCE)
                throw new IllegalStateException("decode did not return the singleton");
            if (consumed != 0)
                throw new IllegalStateException("unit codec consumed " + consumed + " bytes");
            if (n != 0)
                throw new IllegalStateException("unit codec wrote " + n + " bytes");

            O.print("ENC\t");
            O.print("INSTANCE");
            O.print('\t');
            O.print(n);          // readableBytes (decimal) — 0
            O.print('\t');
            O.print(hex);        // empty hex string
            O.print('\n');
        }
    }

    static String toHex(ByteBuf b) {
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
