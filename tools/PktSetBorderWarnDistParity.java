// Ground truth for net.minecraft.network.protocol.game.ClientboundSetBorderWarningDistancePacket.
//
// The packet holds a single `private final int warningBlocks`. Its STREAM_CODEC is
// Packet.codec(ClientboundSetBorderWarningDistancePacket::write, ::new), where write/read are
// exactly:
//   write : FriendlyByteBuf.writeVarInt(this.warningBlocks)
//   read  : input.readVarInt()
// (net.minecraft.network.protocol.game.ClientboundSetBorderWarningDistancePacket lines 9-39).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is a single VarInt (LEB128) of the signed int `warningBlocks` (no zig-zag).
//
// There is no public int ctor (only `(WorldBorder)` and a private `(FriendlyByteBuf)`); we
// build each case packet by reflecting the private `(FriendlyByteBuf)` constructor against a
// buffer pre-loaded with the desired VarInt. That construction path itself runs through the
// real readVarInt, so it doubles as a decode sanity check for the field value.
//
// Row format (tab separated):
//   ENC <warningBlocks-dec> <readableBytes-dec> <hex>   encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert warningBlocks equality as
// a sanity check before emitting. The C++ pkt_set_border_warn_dist_parity rebuilds the packet
// from <warningBlocks>, re-encodes via PacketBuffer.writeVarInt, and must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt and checks the
// recovered value.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Constructor;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetBorderWarningDistancePacket;
import net.minecraft.server.Bootstrap;

public class PktSetBorderWarnDistParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetBorderWarningDistancePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetBorderWarningDistancePacket>)
                ClientboundSetBorderWarningDistancePacket.STREAM_CODEC;

        // Finite/physical input battery for the VarInt `warningBlocks`.
        // In vanilla the warning distance is a small non-negative block count (default 5), but
        // the codec is a bare signed-int VarInt, so we pin every LEB128 byte boundary
        // (1->2->3->4->5 bytes) and the int extremes. Negatives encode as 5 bytes in LEB128
        // since writeVarInt does not zig-zag.
        int[] vals = {
            0, 1, 2, 3, 5, 8, 16, 29, 42, 100,
            127, 128, 129,                   // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,             // 2->3 byte boundary
            2097151, 2097152, 2097153,       // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,               // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE                // 0x80000000 -> 5 bytes
        };

        for (int v : vals) {
            // Build the real packet via the private (FriendlyByteBuf) ctor fed the VarInt.
            ClientboundSetBorderWarningDistancePacket pkt = make(v);

            // ENC: encode through the real codec, dump bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundSetBorderWarningDistancePacket dec = CODEC.decode(rbuf);
            if (dec.getWarningBlocks() != v) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + v + " out=" + dec.getWarningBlocks());
            }

            O.print("ENC\t");
            O.print(v);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    // Construct the packet with a specific warningBlocks value by reflecting the private
    // (FriendlyByteBuf) constructor against a buffer pre-loaded with writeVarInt(value).
    static ClientboundSetBorderWarningDistancePacket make(int value) throws Exception {
        FriendlyByteBuf src = new FriendlyByteBuf(Unpooled.buffer());
        src.writeVarInt(value);
        Constructor<ClientboundSetBorderWarningDistancePacket> ctor =
            ClientboundSetBorderWarningDistancePacket.class
                .getDeclaredConstructor(FriendlyByteBuf.class);
        ctor.setAccessible(true);
        return ctor.newInstance(src);
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
