// Ground truth for net.minecraft.network.protocol.game.ServerboundPlayerAbilitiesPacket.
//
// Real 26.1.2 source (ServerboundPlayerAbilitiesPacket.java lines 13-32):
//   private static final int FLAG_FLYING = 2;
//   private final boolean isFlying;
//   ctor(Abilities abilities) { this.isFlying = abilities.flying; }
//   write(FriendlyByteBuf output):
//     byte bitfield = 0;
//     if (this.isFlying) bitfield = (byte)(bitfield | 2);
//     output.writeByte(bitfield);                 // single raw byte
//   ctor(FriendlyByteBuf input):
//     byte bitfield = input.readByte();
//     this.isFlying = (bitfield & 2) != 0;
// STREAM_CODEC = Packet.codec(write, new) -> StreamCodec.ofMember: NO packet-id prefix,
// just the body. So the wire payload is exactly ONE byte:
//   isFlying==false -> 0x00,  isFlying==true -> 0x02.
//
// The ONLY public ctor takes net.minecraft.world.entity.player.Abilities; we build a
// real Abilities, set its public boolean field `flying`, then construct the packet
// through that public ctor. NOTE: write() ignores every Abilities flag EXCEPT flying,
// so the other Abilities booleans do not affect the encoded byte (we still vary them to
// prove that independence on the ENC side).
//
// Row formats (tab separated):
//   ENC <name-hex> <isFlying-dec> <readableBytes-dec> <hex>
//       encode the REAL packet (built via the public Abilities ctor) through STREAM_CODEC;
//       isFlying is 0/1 decimal, <hex> is the full payload (always 1 byte: 00 or 02).
//   DEC <inHex> <isFlying-out-dec>
//       feed an arbitrary input byte through the REAL decode ctor (via STREAM_CODEC.decode)
//       and report the recovered isFlying() (exercises the (bitfield & 2) != 0 mask over
//       every byte value 0..255, so the C++ side's bit test is gated exhaustively).
//
// Every ENC case is round-trip-decoded through the SAME codec and asserted bit-exact
// before emit. The C++ pkt_player_abilities_sb_parity rebuilds the byte from isFlying,
// re-encodes via PacketBuffer (writeByte), and must match <hex> byte-for-byte
// (+ readableBytes); for DEC it readByte()s and recomputes (byte & 2) != 0 and must
// match <isFlying-out>.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPlayerAbilitiesPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.player.Abilities;

public class PktPlayerAbilitiesSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundPlayerAbilitiesPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPlayerAbilitiesPacket>)
                ServerboundPlayerAbilitiesPacket.STREAM_CODEC;

        // (A) ENC: the only two physical packet states (isFlying false/true). We also
        //     toggle the OTHER Abilities flags to demonstrate write() ignores them: the
        //     encoded byte depends solely on `flying`.
        emit(CODEC, "not_flying",         false, false, false, false);
        emit(CODEC, "flying",             true,  false, false, false);
        emit(CODEC, "not_flying_allset",  false, true,  true,  true);   // still 0x00
        emit(CODEC, "flying_allset",      true,  true,  true,  true);    // still 0x02
        emit(CODEC, "not_flying_invuln",  false, true,  false, false);
        emit(CODEC, "flying_canfly",      true,  false, true,  false);

        // (B) DEC: exhaustive over every possible single input byte 0..255. The real
        //     decode ctor computes isFlying = (bitfield & 2) != 0; emit the recovered bool
        //     so the C++ side reproduces the mask exactly for ALL bytes (incl. bit-2 set
        //     amid other bits, sign byte 0x80, full 0xff, boundary 0x02/0x03/0xfd, ...).
        for (int b = 0; b <= 255; b++) {
            byte[] in = new byte[] { (byte) b };
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(in));
            ServerboundPlayerAbilitiesPacket dec = CODEC.decode(rbuf);
            O.print("DEC\t");
            O.print(String.format("%02x", b & 0xff));
            O.print('\t');
            O.print(dec.isFlying() ? 1 : 0);
            O.print('\n');
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundPlayerAbilitiesPacket> CODEC,
                     String name, boolean flying,
                     boolean invuln, boolean canFly, boolean insta) {
        // Build a real Abilities (public boolean fields) and construct the packet through
        // its only public ctor; only `flying` influences the encoded byte.
        Abilities ab = new Abilities();
        ab.flying = flying;
        ab.invulnerable = invuln;
        ab.mayfly = canFly;
        ab.instabuild = insta;

        ServerboundPlayerAbilitiesPacket pkt = new ServerboundPlayerAbilitiesPacket(ab);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec; assert isFlying bit-exact.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundPlayerAbilitiesPacket dec = CODEC.decode(rbuf);
        if (dec.isFlying() != flying) {
            throw new IllegalStateException(
                "round-trip mismatch: in flying=" + flying + " out flying=" + dec.isFlying());
        }

        O.print("ENC\t");
        O.print(toHexStr(name));     // name as lowercase hex (label column)
        O.print('\t');
        O.print(flying ? 1 : 0);
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
