// Ground truth for net.minecraft.network.protocol.game.ServerboundPlayerInputPacket.
//
// The packet is a record wrapping a single net.minecraft.world.entity.player.Input
// (real 26.1.2 source, ServerboundPlayerInputPacket lines 9-12):
//   public record ServerboundPlayerInputPacket(Input input) ...
//   STREAM_CODEC = StreamCodec.composite(Input.STREAM_CODEC, ...::input, ...::new)
// So the entire wire payload is exactly Input.STREAM_CODEC's output, NO packet-id
// prefix (StreamCodec.composite of one field). Input.STREAM_CODEC is a hand-written
// StreamCodec (real 26.1.2 source, Input lines 7-38):
//   FLAG_FORWARD=1, FLAG_BACKWARD=2, FLAG_LEFT=4, FLAG_RIGHT=8,
//   FLAG_JUMP=16, FLAG_SHIFT=32, FLAG_SPRINT=64
//   encode:
//     byte flags = 0;
//     flags |= forward  ? 1  : 0;
//     flags |= backward ? 2  : 0;
//     flags |= left     ? 4  : 0;
//     flags |= right    ? 8  : 0;
//     flags |= jump     ? 16 : 0;
//     flags |= shift    ? 32 : 0;
//     flags |= sprint   ? 64 : 0;
//     output.writeByte(flags);              // single raw byte
//   decode:
//     byte flags = input.readByte();
//     forward  = (flags & 1)  != 0;  backward = (flags & 2)  != 0;
//     left     = (flags & 4)  != 0;  right    = (flags & 8)  != 0;
//     jump     = (flags & 16) != 0;  shift    = (flags & 32) != 0;
//     sprint   = (flags & 64) != 0;
// So the wire payload is EXACTLY 1 byte. Bit 7 (0x80) is unused (max value 0x7f).
//
// Both Input and ServerboundPlayerInputPacket are public records with public canonical
// ctors, so we construct the real packet directly through public API.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name-hex> <forward> <backward> <left> <right> <jump> <shift> <sprint>
//       <readableBytes-dec> <hex>
// where name is a lowercase-hex label, the seven booleans are 0/1 decimal,
// readableBytes is decimal, and <hex> is the full packet payload lowercase hex.
//
// Every case is round-trip-decoded through the SAME codec and asserted bit-exact before
// emit. The C++ pkt_player_input_sb_parity rebuilds the flag byte from these booleans,
// re-encodes via PacketBuffer (writeByte), and must match <hex> byte-for-byte (+
// readableBytes); it also decodes <hex> and checks the recovered flag bits exactly.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPlayerInputPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.player.Input;

public class PktPlayerInputSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundPlayerInputPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPlayerInputPacket>)
                ServerboundPlayerInputPacket.STREAM_CODEC;

        // (A) Enumerate ALL 128 valid flag combinations (7 independent bits, 0..127).
        //     This exercises every reachable byte value the codec can produce/consume.
        for (int mask = 0; mask < 128; mask++) {
            boolean forward  = (mask & 1)  != 0;
            boolean backward = (mask & 2)  != 0;
            boolean left     = (mask & 4)  != 0;
            boolean right    = (mask & 8)  != 0;
            boolean jump     = (mask & 16) != 0;
            boolean shift    = (mask & 32) != 0;
            boolean sprint   = (mask & 64) != 0;
            emit(CODEC, "mask" + mask, forward, backward, left, right, jump, shift, sprint);
        }

        // (B) Hand-picked named vanilla states (sanity labels; bytes already covered by A).
        emit(CODEC, "empty",        false, false, false, false, false, false, false); // 0x00
        emit(CODEC, "walkforward",  true,  false, false, false, false, false, false); // 0x01
        emit(CODEC, "walkback",     false, true,  false, false, false, false, false); // 0x02
        emit(CODEC, "strafeleft",   false, false, true,  false, false, false, false); // 0x04
        emit(CODEC, "straferight",  false, false, false, true,  false, false, false); // 0x08
        emit(CODEC, "jump",         false, false, false, false, true,  false, false); // 0x10
        emit(CODEC, "sneak",        false, false, false, false, false, true,  false); // 0x20
        emit(CODEC, "sprint",       false, false, false, false, false, false, true);  // 0x40
        emit(CODEC, "sprintfwd",    true,  false, false, false, false, false, true);  // 0x41
        emit(CODEC, "sneakfwd",     true,  false, false, false, false, true,  false); // 0x21
        emit(CODEC, "jumpfwd",      true,  false, false, false, true,  false, false); // 0x11
        emit(CODEC, "all",          true,  true,  true,  true,  true,  true,  true);   // 0x7f
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundPlayerInputPacket> CODEC,
                     String name, boolean forward, boolean backward, boolean left,
                     boolean right, boolean jump, boolean shift, boolean sprint) {
        // Build the real Input + packet through public canonical record ctors.
        Input input = new Input(forward, backward, left, right, jump, shift, sprint);
        ServerboundPlayerInputPacket pkt = new ServerboundPlayerInputPacket(input);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec; assert exact equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundPlayerInputPacket dec = CODEC.decode(rbuf);
        Input di = dec.input();
        if (di.forward() != forward || di.backward() != backward || di.left() != left
            || di.right() != right || di.jump() != jump || di.shift() != shift
            || di.sprint() != sprint) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + forward + "," + backward + "," + left + ","
                + right + "," + jump + "," + shift + "," + sprint + ") out=("
                + di.forward() + "," + di.backward() + "," + di.left() + "," + di.right()
                + "," + di.jump() + "," + di.shift() + "," + di.sprint() + ")");
        }

        O.print("ENC\t");
        O.print(toHexStr(name));               // name as lowercase hex (label column)
        O.print('\t');
        O.print(forward  ? 1 : 0);
        O.print('\t');
        O.print(backward ? 1 : 0);
        O.print('\t');
        O.print(left     ? 1 : 0);
        O.print('\t');
        O.print(right    ? 1 : 0);
        O.print('\t');
        O.print(jump     ? 1 : 0);
        O.print('\t');
        O.print(shift    ? 1 : 0);
        O.print('\t');
        O.print(sprint   ? 1 : 0);
        O.print('\t');
        O.print(readable);
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

    static String toHexStr(String s) {
        StringBuilder sb = new StringBuilder();
        for (byte by : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            sb.append(String.format("%02x", by & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
