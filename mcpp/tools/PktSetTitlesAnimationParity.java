// Ground truth for net.minecraft.network.protocol.game.ClientboundSetTitlesAnimationPacket's
// StreamCodec.
//
// The packet is class(int fadeIn, int stay, int fadeOut) and its body is exactly
// (ClientboundSetTitlesAnimationPacket.java:28-32):
//   write : FriendlyByteBuf.writeInt(this.fadeIn)    // big-endian 4-byte int
//           FriendlyByteBuf.writeInt(this.stay)      // big-endian 4-byte int
//           FriendlyByteBuf.writeInt(this.fadeOut)   // big-endian 4-byte int
//   read  : this.fadeIn  = input.readInt();
//           this.stay    = input.readInt();
//           this.fadeOut = input.readInt();
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: three big-endian 4-byte ints.
//
// All three fields are plain ints; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. The public ctor
// (int fadeIn, int stay, int fadeOut) is used directly so we can pin arbitrary triples,
// then encode every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). fadeIn/stay/fadeOut are decimal; hex columns are
// lowercase hex:
//   ENC <fadeIn> <stay> <fadeOut> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <fadeIn_in> <stay_in> <fadeOut_in> <fadeIn_dec> <stay_dec> <fadeOut_dec>
//        decode: STREAM_CODEC.decode(buf) -> getFadeIn()/getStay()/getFadeOut() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetTitlesAnimationPacket;

public class PktSetTitlesAnimationParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetTitlesAnimationPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetTitlesAnimationPacket>)
                ClientboundSetTitlesAnimationPacket.STREAM_CODEC;

        // Finite/physical battery. writeInt is a fixed 4-byte big-endian int with no
        // VarInt boundaries, so we exercise zero, sign, small/large magnitudes, and the
        // int32 extremes. Title animation tick counts are physically small/positive in
        // vanilla (e.g. fadeIn=10, stay=70, fadeOut=20), but the codec is a raw int so we
        // sweep the full representable range.
        int[] vals = {
            0, 1, -1, 2, 10, 20, 70, 100, 255, 256,
            127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -128, -256, 65535, 65536, 1000000, -1000000,
            Integer.MAX_VALUE, Integer.MIN_VALUE,
        };

        // (A) the canonical vanilla animation triple plus a few physical title timings.
        emit(CODEC, 10, 70, 20);
        emit(CODEC, 0, 0, 0);
        emit(CODEC, 20, 100, 20);
        emit(CODEC, 5, 200, 10);

        // (B) sweep each field independently across the battery (others pinned).
        for (int v : vals) {
            emit(CODEC, v, 70, 20);   // vary fadeIn
            emit(CODEC, 10, v, 20);   // vary stay
            emit(CODEC, 10, 70, v);   // vary fadeOut
        }

        // (C) all-three-equal across the battery (exercises identical-int repetition).
        for (int v : vals) {
            emit(CODEC, v, v, v);
        }

        // (D) a few hand-picked sign/extreme combinations.
        emit(CODEC, Integer.MAX_VALUE, Integer.MIN_VALUE, -1);
        emit(CODEC, Integer.MIN_VALUE, 0, Integer.MAX_VALUE);
        emit(CODEC, -1, -1, -1);
        emit(CODEC, 16384, 2097152, 268435456);
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundSetTitlesAnimationPacket> CODEC,
                     int fadeIn, int stay, int fadeOut) throws Exception {
        // Build a packet with this exact triple via the canonical public ctor.
        ClientboundSetTitlesAnimationPacket pkt =
            new ClientboundSetTitlesAnimationPacket(fadeIn, stay, fadeOut);

        // ENC: encode through the REAL codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int n = buf.readableBytes();
        String hex = toHex(buf);
        O.print("ENC\t");
        O.print(fadeIn);
        O.print('\t');
        O.print(stay);
        O.print('\t');
        O.print(fadeOut);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex);
        O.print('\n');

        // DEC: decode the same bytes through the REAL codec; round-trip sanity.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundSetTitlesAnimationPacket dec = CODEC.decode(rbuf);
        if (dec.getFadeIn() != fadeIn)
            throw new IllegalStateException("fadeIn round-trip " + fadeIn + " -> " + dec.getFadeIn());
        if (dec.getStay() != stay)
            throw new IllegalStateException("stay round-trip " + stay + " -> " + dec.getStay());
        if (dec.getFadeOut() != fadeOut)
            throw new IllegalStateException("fadeOut round-trip " + fadeOut + " -> " + dec.getFadeOut());
        O.print("DEC\t");
        O.print(hex);
        O.print('\t');
        O.print(fadeIn);
        O.print('\t');
        O.print(stay);
        O.print('\t');
        O.print(fadeOut);
        O.print('\t');
        O.print(dec.getFadeIn());
        O.print('\t');
        O.print(dec.getStay());
        O.print('\t');
        O.print(dec.getFadeOut());
        O.print('\n');
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
