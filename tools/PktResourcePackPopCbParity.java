// Ground truth for net.minecraft.network.protocol.common.ClientboundResourcePackPopPacket's
// StreamCodec.
//
// The packet body is exactly (ClientboundResourcePackPopPacket.java:11-22):
//   write : FriendlyByteBuf.writeOptional(this.id, UUIDUtil.STREAM_CODEC)
//   read  : this(input.readOptional(UUIDUtil.STREAM_CODEC))
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember, so
// NO packet-id prefix on the wire, just the body.
//
// FriendlyByteBuf.writeOptional (FriendlyByteBuf.java:224-231):
//   if present: writeBoolean(true)  then valueWriter.encode(this, value.get())
//   if absent : writeBoolean(false)
// UUIDUtil.STREAM_CODEC.encode == FriendlyByteBuf.writeUUID (FriendlyByteBuf.java:498-501):
//   writeLong(mostSignificantBits) then writeLong(leastSignificantBits)  (both BE 8-byte).
//
// So the wire is exactly:
//   present=0 -> single byte 0x00
//   present=1 -> 0x01, then BE long(msb), then BE long(lsb)  (17 bytes)
// Both the boolean and the two longs are fully representable by the certified
// PacketBuffer (FriendlyByteBuf) port; no registry/ItemStack/Component/Holder/NBT.
//
// We build each packet via the PUBLIC record ctor ClientboundResourcePackPopPacket(Optional<UUID>)
// and encode every packet through the REAL STREAM_CODEC. A UUID is carried in the TSV as its
// two raw signed longs (msb, lsb), in decimal.
//
// Row format (tab separated). present is 0/1; msb/lsb are decimal signed longs (only
// meaningful when present=1, written as 0 otherwise); hex column is lowercase hex:
//   ENC <present> <msb> <lsb> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.Optional;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ClientboundResourcePackPopPacket;

public class PktResourcePackPopCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundResourcePackPopPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundResourcePackPopPacket>)
                ClientboundResourcePackPopPacket.STREAM_CODEC;

        // Finite battery. The empty-Optional case is the single 0x00 byte; the present
        // case exercises the full signed 64-bit range of both UUID halves incl.
        // zero/sign/byte-boundaries/max-min.
        long[] vals = {
            0L, 1L, 2L, 5L, 100L, 127L, 128L, 255L, 256L,
            65535L, 65536L, 16777215L, 16777216L,
            4294967295L, 4294967296L,
            -1L, -2L, -128L, -129L, -256L,
            Long.MAX_VALUE, Long.MIN_VALUE,
            0x0123456789abcdefL, 0xfedcba9876543210L,
        };

        // present=0 (Optional.empty) — one case.
        emit(CODEC, false, 0L, 0L);

        // present=1 (Optional.of(uuid)) over the cross product of the battery.
        for (long msb : vals) {
            for (long lsb : vals) {
                emit(CODEC, true, msb, lsb);
            }
        }
    }

    static void emit(
            StreamCodec<FriendlyByteBuf, ClientboundResourcePackPopPacket> codec,
            boolean present, long msb, long lsb) {
        Optional<UUID> id = present ? Optional.of(new UUID(msb, lsb)) : Optional.empty();
        ClientboundResourcePackPopPacket pkt = new ClientboundResourcePackPopPacket(id);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);
        int n = buf.readableBytes();
        String hex = toHex(buf);

        // Decode round-trip sanity through the REAL codec.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundResourcePackPopPacket dec = codec.decode(rbuf);
        if (dec.id().isPresent() != present)
            throw new IllegalStateException("present round-trip " + present + " -> " + dec.id().isPresent());
        if (present) {
            UUID u = dec.id().get();
            if (u.getMostSignificantBits() != msb || u.getLeastSignificantBits() != lsb)
                throw new IllegalStateException("uuid round-trip " + msb + "/" + lsb
                    + " -> " + u.getMostSignificantBits() + "/" + u.getLeastSignificantBits());
        }

        O.print("ENC\t");
        O.print(present ? 1 : 0);
        O.print('\t');
        O.print(msb);
        O.print('\t');
        O.print(lsb);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex);
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
