// Ground truth for net.minecraft.network.protocol.game.ServerboundLockDifficultyPacket.
//
// The packet wraps a single `private final boolean locked`. Its STREAM_CODEC is
// Packet.codec(ServerboundLockDifficultyPacket::write, ServerboundLockDifficultyPacket::new),
// where write/read are exactly:
//   write : FriendlyByteBuf.writeBoolean(this.locked)
//   read  : input.readBoolean()
// (net.minecraft.network.protocol.game.ServerboundLockDifficultyPacket lines 9-24).
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body, so the whole
// wire payload is a single byte 0x01 (true) / 0x00 (false).
//
// Row format (tab separated):
//   ENC \t <locked-dec 0|1> \t <readableBytes-dec> \t <hexBytes>
// where ENC encodes via STREAM_CODEC.encode(buf, pkt).
//
// We round-trip-decode every case through the SAME codec and assert equality as a sanity
// check before emitting. The C++ pkt_lock_difficulty_sb_parity rebuilds the packet from
// <locked>, re-encodes via PacketBuffer.writeBool, and must match <hexBytes> byte-for-byte
// (+ readableBytes); it also decodes <hexBytes> via readBool and checks recovered `locked`.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundLockDifficultyPacket;
import net.minecraft.server.Bootstrap;

public class PktLockDifficultySbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundLockDifficultyPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundLockDifficultyPacket>)
                ServerboundLockDifficultyPacket.STREAM_CODEC;

        // The only field is a boolean: both physical values are exercised.
        boolean[] cases = { false, true };

        for (boolean locked : cases) {
            // ENC: construct the real packet (public ctor), encode through the real codec.
            ServerboundLockDifficultyPacket pkt = new ServerboundLockDifficultyPacket(locked);
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundLockDifficultyPacket dec = CODEC.decode(rbuf);
            if (dec.isLocked() != locked) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=" + locked + " out=" + dec.isLocked());
            }

            O.print("ENC\t");
            O.print(locked ? 1 : 0);
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
