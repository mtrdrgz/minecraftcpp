// Ground truth for net.minecraft.network.protocol.game.ClientboundProjectilePowerPacket's
// StreamCodec.
//
// The packet (ClientboundProjectilePowerPacket.java:8-46) holds:
//     private final int id;
//     private final double accelerationPower;
// Its STREAM_CODEC is Packet.codec(write, new) and write(FriendlyByteBuf) is, in this
// exact wire order (ClientboundProjectilePowerPacket.java:25-28):
//     output.writeVarInt(this.id);          -> VarInt (LEB128) id
//     output.writeDouble(this.accelerationPower); -> big-endian IEEE-754 8-byte double
// and the FriendlyByteBuf decode ctor reads them back in the same order
// (ClientboundProjectilePowerPacket.java:20-23): readVarInt() then readDouble().
// Packet.codec -> no packet-id prefix, just the body.
//
// Both fields are plain primitives: no registry/ItemStack/Component/Holder/NBT, so the
// body is fully representable by the certified PacketBuffer (FriendlyByteBuf) port
// (writeVarInt + writeDouble).
//
// Row format (tab separated):
//   ENC <id> <powBits> <readableBytes> <hexBytes>
//     id as a decimal int; accelerationPower as %016x of its raw long bits
//     (Double.doubleToRawLongBits) so NaN/Inf/-0.0 survive exactly without parse
//     rounding; hex columns lowercase. encode through the REAL codec, dump
//     readableBytes + every byte, then decode the same bytes back through the SAME
//     codec for a round-trip sanity check (abort on any drift).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundProjectilePowerPacket;

public class PktProjectilePowerParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundProjectilePowerPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundProjectilePowerPacket>)
                ClientboundProjectilePowerPacket.STREAM_CODEC;

        // VarInt battery: zero/small, the exact 1->2->3->4->5 byte boundaries
        // (127/128, 16383/16384, 2097151/2097152, 268435455/268435456), and the signed
        // extremes (-1 and Integer.MIN/MAX, which VarInt encodes as 5 bytes via the raw
        // 32-bit two's-complement representation). These are real entity ids on the wire.
        int[] ids = {
            0, 1, 2, 127, 128, 255, 256, 16383, 16384, 65535, 65536,
            2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };

        // Finite/physical acceleration-power battery (doubles): zero/sign, unit, small
        // fractional, world-scale magnitudes, plus IEEE-754 specials to exercise the full
        // 8-byte encoding. accelerationPower is a plain double (e.g. fireball power ~0.1).
        double[] powers = {
            0.0, -0.0, 1.0, -1.0, 0.1, -0.1, 0.5, -0.5,
            0.07, 0.0625, 2.5, -2.5, 100.5, -100.5, 1234.5678, -1234.5678,
            Double.MIN_VALUE, Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.NaN,
        };

        // Sweep every id against every power so each VarInt-length boundary is paired with
        // every special double and vice versa.
        for (int id : ids)
            for (double pow : powers)
                emit(CODEC, id, pow);
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundProjectilePowerPacket> CODEC,
                     int id, double power) {
        // Build the packet via its public (int id, double accelerationPower) ctor.
        ClientboundProjectilePowerPacket pkt =
            new ClientboundProjectilePowerPacket(id, power);

        // ENC: encode through the REAL codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int n = buf.readableBytes();
        String hex = toHex(buf);

        long powBits = Double.doubleToRawLongBits(power);

        O.print("ENC\t");
        O.print(id);
        O.print('\t');
        O.print(String.format("%016x", powBits));
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex);
        O.print('\n');

        // Round-trip decode through the SAME codec; abort on any drift so no bad row is
        // ever emitted.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundProjectilePowerPacket dec = CODEC.decode(rbuf);
        int decId = dec.getId();
        long decPowBits = Double.doubleToRawLongBits(dec.getAccelerationPower());
        if (decId != id || decPowBits != powBits) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + id + "," + String.format("%016x", powBits)
                    + ") out=(" + decId + "," + String.format("%016x", decPowBits) + ")");
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
