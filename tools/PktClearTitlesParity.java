// Ground truth for net.minecraft.network.protocol.game.ClientboundClearTitlesPacket.
//
// The packet has a single field `boolean resetTimes`. Its STREAM_CODEC is
// Packet.codec(ClientboundClearTitlesPacket::write, ClientboundClearTitlesPacket::new),
// where write/read are exactly:
//   write : FriendlyByteBuf.writeBoolean(this.resetTimes)
//   read  : input.readBoolean()
// (net.minecraft.network.protocol.game.ClientboundClearTitlesPacket lines 9-24).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the
// whole wire payload is a single byte: 0x01 if resetTimes else 0x00.
//
// Row format (tab separated):
//   ENC <resetTimes-dec(0/1)> <readableBytes-dec> <hex>   encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert resetTimes equality
// as a sanity check before emitting. The C++ pkt_clear_titles_parity rebuilds the packet
// from <resetTimes>, re-encodes via PacketBuffer.writeBool, and must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via readBool and checks the
// recovered boolean.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundClearTitlesPacket;
import net.minecraft.server.Bootstrap;

public class PktClearTitlesParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundClearTitlesPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundClearTitlesPacket>)
                ClientboundClearTitlesPacket.STREAM_CODEC;

        // Finite/physical input battery for the single boolean `resetTimes`.
        // FriendlyByteBuf.writeBoolean writes a single byte 0x01 (true) / 0x00 (false);
        // both states are the entire physical domain of this packet.
        boolean[] resetTimesCases = { false, true };

        for (boolean resetTimes : resetTimesCases) {
            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ClientboundClearTitlesPacket pkt = new ClientboundClearTitlesPacket(resetTimes);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundClearTitlesPacket dec = CODEC.decode(rbuf);
            if (dec.shouldResetTimes() != resetTimes) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + resetTimes + " out=" + dec.shouldResetTimes());
            }

            O.print("ENC\t");
            O.print(resetTimes ? 1 : 0);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
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
