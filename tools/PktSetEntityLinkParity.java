// Ground truth for net.minecraft.network.protocol.game.ClientboundSetEntityLinkPacket's
// StreamCodec.
//
// The packet body is exactly (ClientboundSetEntityLinkPacket.java:22-30):
//   write : FriendlyByteBuf.writeInt(this.sourceId)   // big-endian 4-byte int
//           FriendlyByteBuf.writeInt(this.destId)      // big-endian 4-byte int
//   read  : this.sourceId = input.readInt();
//           this.destId   = input.readInt();
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: BE int(sourceId) then BE int(destId).
//
// Both fields are plain signed 32-bit ints (entity network ids); no registry/ItemStack/
// Component/Holder/NBT/SoundEvent — fully representable by the certified PacketBuffer
// (FriendlyByteBuf) port. The public ctor takes (Entity, @Nullable Entity); to pin
// arbitrary (sourceId, destId) pairs we invoke the PRIVATE FriendlyByteBuf decode ctor
// via reflection to build a packet, then encode every packet through the REAL
// STREAM_CODEC.
//
// Row formats (tab separated). sourceId and destId are decimal; hex columns are
// lowercase hex:
//   ENC <sourceId> <destId> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <sourceId_in> <destId_in> <sourceId_decoded> <destId_decoded>
//        decode: STREAM_CODEC.decode(buf) -> getSourceId()/getDestId() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetEntityLinkPacket;

public class PktSetEntityLinkParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetEntityLinkPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetEntityLinkPacket>)
                ClientboundSetEntityLinkPacket.STREAM_CODEC;

        // Finite/physical battery. Both fields are entity network ids written as raw
        // big-endian 4-byte ints (NOT VarInt), so the only thing to exercise is the full
        // signed 32-bit range incl. zero/sign/byte-boundaries/max-min.
        int[] vals = {
            0, 1, 2, 5, 100, 127, 128, 255, 256,
            16383, 16384, 65535, 65536, 16777215, 16777216,
            2097151, 2097152, 268435455, 268435456,
            -1, -2, -128, -129, -256,
            Integer.MAX_VALUE, Integer.MIN_VALUE,
        };

        for (int sourceId : vals) {
            for (int destId : vals) {
                // Build a packet with this exact (sourceId, destId) via the private
                // FriendlyByteBuf decode ctor (the same path the canonical record ctor
                // would yield, but with arbitrary controllable values).
                ClientboundSetEntityLinkPacket pkt = make(CODEC, sourceId, destId);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                O.print("ENC\t");
                O.print(sourceId);
                O.print('\t');
                O.print(destId);
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundSetEntityLinkPacket dec = CODEC.decode(rbuf);
                if (dec.getSourceId() != sourceId)
                    throw new IllegalStateException("sourceId round-trip " + sourceId + " -> " + dec.getSourceId());
                if (dec.getDestId() != destId)
                    throw new IllegalStateException("destId round-trip " + destId + " -> " + dec.getDestId());
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(sourceId);
                O.print('\t');
                O.print(destId);
                O.print('\t');
                O.print(dec.getSourceId());
                O.print('\t');
                O.print(dec.getDestId());
                O.print('\n');
            }
        }
    }

    // Build a ClientboundSetEntityLinkPacket carrying exactly (sourceId, destId) by
    // decoding a hand-built body through the REAL codec (BE int sourceId + BE int destId).
    // Uses no reflection beyond the codec itself; guarantees the produced packet is what
    // the codec would yield on the wire.
    static ClientboundSetEntityLinkPacket make(
            StreamCodec<FriendlyByteBuf, ClientboundSetEntityLinkPacket> codec,
            int sourceId, int destId) throws Exception {
        FriendlyByteBuf b = new FriendlyByteBuf(Unpooled.buffer());
        b.writeInt(sourceId);
        b.writeInt(destId);
        return codec.decode(b);
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
