// Ground truth for net.minecraft.network.protocol.game.ClientboundTickingStatePacket's
// StreamCodec.
//
// The packet is a record(float tickRate, boolean isFrozen) and its body is exactly
// (ClientboundTickingStatePacket.java:22-25):
//   write : FriendlyByteBuf.writeFloat(this.tickRate)   // big-endian IEEE-754 4 bytes
//           FriendlyByteBuf.writeBoolean(this.isFrozen)  // 1 byte: 0x00 / 0x01
//   read  : this.tickRate = input.readFloat();
//           this.isFrozen = input.readBoolean();
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: 4-byte float(tickRate) then the
// 1-byte boolean(isFrozen).
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. We use the
// canonical record ctor (float, boolean) directly so we can pin arbitrary pairs, then
// encode every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). tickRate is emitted as %08x of its raw int bits
// (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly; isFrozen is 0/1; hex
// columns are lowercase hex:
//   ENC <tickRateRawBitsHex> <isFrozen0or1> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <tickRateRawBitsHex_in> <isFrozen_in> <tickRateRawBitsHex_decoded> <isFrozen_decoded>
//        decode: STREAM_CODEC.decode(buf) -> tickRate()/isFrozen() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundTickingStatePacket;

public class PktTickingStateParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundTickingStatePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundTickingStatePacket>)
                ClientboundTickingStatePacket.STREAM_CODEC;

        // tickRate: the physical range is the server tick rate (vanilla default 20.0),
        // plus sign / zero / boundary / special IEEE-754 floats to exercise the full
        // 4-byte encoding.
        float[] tickRates = {
            20.0f, 0.0f, -0.0f, 1.0f, -1.0f, 0.5f, 10.0f, 40.0f, 60.0f,
            3.14159265f, 123.456f, -123.456f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };
        boolean[] frozens = { false, true };

        for (float tickRate : tickRates) {
            for (boolean isFrozen : frozens) {
                // Build a packet with this exact (tickRate, isFrozen) via the record ctor.
                ClientboundTickingStatePacket pkt = new ClientboundTickingStatePacket(tickRate, isFrozen);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                int rateBits = Float.floatToRawIntBits(tickRate);
                O.print("ENC\t");
                O.print(String.format("%08x", rateBits));
                O.print('\t');
                O.print(isFrozen ? 1 : 0);
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundTickingStatePacket dec = CODEC.decode(rbuf);
                int decRateBits = Float.floatToRawIntBits(dec.tickRate());
                if (decRateBits != rateBits)
                    throw new IllegalStateException("tickRate round-trip " + rateBits + " -> " + decRateBits);
                if (dec.isFrozen() != isFrozen)
                    throw new IllegalStateException("isFrozen round-trip " + isFrozen + " -> " + dec.isFrozen());
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(String.format("%08x", rateBits));
                O.print('\t');
                O.print(isFrozen ? 1 : 0);
                O.print('\t');
                O.print(String.format("%08x", decRateBits));
                O.print('\t');
                O.print(dec.isFrozen() ? 1 : 0);
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
