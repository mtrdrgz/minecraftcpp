// Ground truth for ClientboundSetHeldSlotPacket's StreamCodec.
//
// net.minecraft.network.protocol.game.ClientboundSetHeldSlotPacket is a record
// with a single int field `slot`, encoded via:
//   STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.VAR_INT, ...::slot, ...::new)
// ByteBufCodecs.VAR_INT is exactly VarInt.write/read (the standard LEB128 VarInt).
// So the packet body is exactly ONE VarInt(slot) — no packet-id prefix (the body
// codec encodes only the payload).
//
// Row format (tab separated):
//   ENC <slot_in>            <readableBytes> <hex>   encode through STREAM_CODEC
//   DEC <hex>  <slot_in>     <slot_decoded>          decode through STREAM_CODEC -> slot()
//
// The C++ side encodes writeVarInt(slot) / decodes readVarInt() and must match
// byte-for-byte (ENC) and value-for-value (DEC). We round-trip decode each ENC
// through the SAME codec as a sanity assert.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetHeldSlotPacket;

public class PktSetHeldSlotCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet. It is StreamCodec<ByteBuf, ...>;
        // FriendlyByteBuf IS a ByteBuf, so it encodes/decodes directly through it.
        @SuppressWarnings("unchecked")
        StreamCodec<ByteBuf, ClientboundSetHeldSlotPacket> CODEC =
            (StreamCodec<ByteBuf, ClientboundSetHeldSlotPacket>)
                ClientboundSetHeldSlotPacket.STREAM_CODEC;

        // Finite/physical battery: real hotbar slots 0..8, then the VarInt
        // 1->2->3->4->5 byte boundaries (127/128, 16383/16384, 2097151/2097152,
        // 268435455/268435456), sign cases, and the int max/min extremes.
        int[] slots = {
            0, 1, 2, 3, 4, 5, 6, 7, 8,
            127, 128,                               // 1 -> 2 byte boundary
            255, 256,
            16383, 16384,                           // 2 -> 3 byte boundary
            2097151, 2097152,                       // 3 -> 4 byte boundary
            268435455, 268435456,                   // 4 -> 5 byte boundary
            -1, -2, -128, -129,                     // negatives always encode to 5 bytes
            100000,
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        for (int slot : slots) {
            // ENC: encode through the real codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ClientboundSetHeldSlotPacket pkt = new ClientboundSetHeldSlotPacket(slot);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf sbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundSetHeldSlotPacket rt = CODEC.decode(sbuf);
            if (rt.slot() != slot) {
                throw new IllegalStateException("round-trip mismatch slot=" + slot + " got=" + rt.slot());
            }

            O.print("ENC\t");
            O.print(slot);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');

            // DEC: decode the same bytes through the real codec, dump slot().
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundSetHeldSlotPacket dec = CODEC.decode(rbuf);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(slot);
            O.print('\t');
            O.print(dec.slot());
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
