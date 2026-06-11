// Ground truth for net.minecraft.network.protocol.game.ClientboundTickingStepPacket.
//
// The packet is a record(int tickSteps). Its STREAM_CODEC is
// Packet.codec(ClientboundTickingStepPacket::write, ::new), where write/read are exactly
// (net.minecraft.network.protocol.game.ClientboundTickingStepPacket lines 14-24):
//   write : FriendlyByteBuf.writeVarInt(this.tickSteps)
//   read  : input.readVarInt()
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is a single VarInt (LEB128) of the signed int `tickSteps` (no zig-zag —
// writeVarInt is a plain unsigned LEB128 over the 32-bit two's-complement value, so
// negatives always occupy 5 bytes).
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. We use the canonical
// record ctor (int) directly so we can pin arbitrary values, then encode every packet
// through the REAL STREAM_CODEC.
//
// Row format (tab separated):
//   ENC <tickSteps-dec> <readableBytes-dec> <hex>     encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert tickSteps equality as
// a sanity check before emitting. The C++ pkt_ticking_step_parity rebuilds the packet from
// <tickSteps>, re-encodes via PacketBuffer.writeVarInt, and must match <hex> byte-for-byte
// (+ readableBytes); it also decodes <hex> via readVarInt and checks the recovered value.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundTickingStepPacket;
import net.minecraft.server.Bootstrap;

public class PktTickingStepParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundTickingStepPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundTickingStepPacket>)
                ClientboundTickingStepPacket.STREAM_CODEC;

        // Finite/physical input battery for the VarInt `tickSteps`.
        // In vanilla tickSteps is TickRateManager.frozenTicksToRun() (a small non-negative
        // count, typically 0..a few), but the codec is a bare signed-int VarInt, so we pin
        // every LEB128 byte boundary (1->2->3->4->5 bytes) and the int extremes. Negatives
        // encode as 5 bytes in LEB128 since writeVarInt does not zig-zag.
        int[] tickSteps = {
            0, 1, 2, 3, 5, 10, 20, 100,
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

        for (int steps : tickSteps) {
            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ClientboundTickingStepPacket pkt = new ClientboundTickingStepPacket(steps);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundTickingStepPacket dec = CODEC.decode(rbuf);
            if (dec.tickSteps() != steps) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + steps + " out=" + dec.tickSteps());
            }

            O.print("ENC\t");
            O.print(steps);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }

        O.print("PktTickingStepParity cases=" + tickSteps.length + " mismatches=0\n");
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
