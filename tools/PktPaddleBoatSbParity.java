// Ground truth for net.minecraft.network.protocol.game.ServerboundPaddleBoatPacket.
//
// The packet has two boolean fields `left` and `right`. Its STREAM_CODEC is
// Packet.codec(write, new):
//   write : output.writeBoolean(this.left); output.writeBoolean(this.right);
//             (ServerboundPaddleBoatPacket.java:25-28)
//   read  : this.left  = input.readBoolean();
//           this.right = input.readBoolean();
//             (ServerboundPaddleBoatPacket.java:20-23)
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is exactly two bytes — left then right — each FriendlyByteBuf.writeBoolean
// (a single byte, 0x01 for true / 0x00 for false).
//
// Row format (tab separated):
//   ENC <left-dec(0/1)> <right-dec(0/1)> <readableBytes-dec> <hex>
//
// We round-trip-decode every case through the SAME codec and assert (left,right) equality
// (via the public getLeft()/getRight()) as a sanity check before emitting. The C++
// pkt_paddle_boat_sb_parity rebuilds the packet from <left,right>, re-encodes via
// PacketBuffer.writeBool, and must match <hex> byte-for-byte (+ readableBytes); it also
// decodes <hex> via readBool and checks the recovered (left,right).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPaddleBoatPacket;
import net.minecraft.server.Bootstrap;

public class PktPaddleBoatSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundPaddleBoatPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPaddleBoatPacket>)
                ServerboundPaddleBoatPacket.STREAM_CODEC;

        // Finite/physical input battery: two independent booleans -> all 4 combinations.
        // writeBoolean is a single byte each (no VarInt), so every case is exactly 2 bytes.
        boolean[][] cases = {
            { false, false },
            { false, true  },
            { true,  false },
            { true,  true  },
        };

        for (boolean[] c : cases) {
            boolean left = c[0];
            boolean right = c[1];

            // ENC: construct the real packet, encode through the real codec, dump bytes.
            ServerboundPaddleBoatPacket pkt = new ServerboundPaddleBoatPacket(left, right);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundPaddleBoatPacket dec = CODEC.decode(rbuf);
            if (dec.getLeft() != left || dec.getRight() != right) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=(" + left + "," + right + ")"
                    + " out=(" + dec.getLeft() + "," + dec.getRight() + ")");
            }

            O.print("ENC\t");
            O.print(left ? 1 : 0);
            O.print('\t');
            O.print(right ? 1 : 0);
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
