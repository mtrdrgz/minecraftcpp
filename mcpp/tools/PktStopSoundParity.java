// Ground truth for net.minecraft.network.protocol.game.ClientboundStopSoundPacket's
// StreamCodec. Verbatim from 26.1.2/src ClientboundStopSoundPacket.java:
//
//   STREAM_CODEC = Packet.codec(write, ctor)   -> no packet-id prefix, just the body.
//
//   write(FriendlyByteBuf out):                         (lines 40-56)
//     if (source != null) {
//         if (name != null) { out.writeByte(3); out.writeEnum(source); out.writeIdentifier(name); }
//         else              { out.writeByte(1); out.writeEnum(source); }
//     } else if (name != null) { out.writeByte(2); out.writeIdentifier(name); }
//     else                     { out.writeByte(0); }
//
//   read(FriendlyByteBuf in):                            (lines 25-38)
//     int flags = in.readByte();
//     source = (flags & 1) > 0 ? in.readEnum(SoundSource.class) : null;
//     name   = (flags & 2) > 0 ? in.readIdentifier()            : null;
//
//   writeEnum(value)       = writeVarInt(value.ordinal())          (FriendlyByteBuf:471-473)
//   writeIdentifier(id)    = writeUtf(id.toString())               (FriendlyByteBuf:585-588)
//   id.toString()          = "namespace:path" (default namespace "minecraft").
//
// SoundSource ordinals (declaration order, SoundSource.java 4-14):
//   MASTER=0 MUSIC=1 RECORDS=2 WEATHER=3 BLOCKS=4 HOSTILE=5 NEUTRAL=6
//   PLAYERS=7 AMBIENT=8 VOICE=9 UI=10
//
// Rows (tab separated):
//   ENUM  <ordinal> <name>                       per SoundSource constant (enum gate)
//   ENC   <name> <hasSource> <srcOrdinal> <hasName> <nameHex> <readableBytes> <hexBytes>
//         hasSource/hasName are 0/1; srcOrdinal is -1 when absent; nameHex is the
//         lowercase-hex of the UTF-8 Identifier.toString() ("" when absent).
//
// The C++ side reconstructs the same packet from these columns, writes the SAME
// fields via PacketBuffer in the SAME codec order, and requires bytes-hex == expected
// AND readableBytes match, then round-trips the bytes back through PacketBuffer.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundStopSoundPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.sounds.SoundSource;

public class PktStopSoundParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ClientboundStopSoundPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundStopSoundPacket>)
                ClientboundStopSoundPacket.STREAM_CODEC;

        // Enum gate: dump ordinal()+name() for every SoundSource constant.
        for (SoundSource s : SoundSource.values()) {
            O.print("ENUM\t");
            O.print(s.ordinal());
            O.print('\t');
            O.print(s.name());
            O.print('\n');
        }

        // Battery of finite/physical cases covering the four flag branches and a
        // range of Identifier shapes. NOTE: a real ClientboundStopSoundPacket can only
        // carry a *valid* Identifier (path chars [a-z0-9._/-], namespace [a-z0-9_.-]) —
        // Identifier.parse rejects anything else and the packet could never transmit it.
        // So every name here is a legal Identifier; the namespace is "minecraft" by
        // default unless the string itself contains a ':'. All Identifier bytes are
        // ASCII, so the writeUtf VarInt-length boundary (1->2 bytes at 128) is the only
        // physical edge — exercised with a long path below.
        String longPath = repeat("a", 200);              // > 127 UTF-8 bytes -> 2-byte VarInt len
        String[] names = new String[] {
            "block.note_block.harp",
            "entity.lightning_bolt.thunder",
            "minecraft:music.menu",
            "namespace_x:custom/sound.event",
            "a",                              // 1-char path (minimal)
            "a.b-c_d/e",                     // every legal path-separator char
            "namespace_x:" + longPath        // long custom-namespace id (VarInt len boundary)
        };

        // Branch (0): neither source nor name.
        emit(CODEC, "none", null, null);

        // Branch (1): source only, every SoundSource.
        for (SoundSource s : SoundSource.values()) {
            emit(CODEC, "src_" + s.name(), null, s);
        }

        // Branch (2): name only, every name shape.
        for (String n : names) {
            emit(CODEC, "name_" + sanitize(n), Identifier.parse(n), null);
        }

        // Branch (3): both source and name (cross a couple of sources x name shapes).
        SoundSource[] both = new SoundSource[] {
            SoundSource.MASTER, SoundSource.BLOCKS, SoundSource.UI, SoundSource.MUSIC
        };
        int bi = 0;
        for (String n : names) {
            SoundSource s = both[bi % both.length];
            emit(CODEC, "both_" + s.name() + "_" + sanitize(n), Identifier.parse(n), s);
            bi++;
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundStopSoundPacket> CODEC,
                     String caseName, Identifier name, SoundSource source) throws Exception {
        ClientboundStopSoundPacket pkt = new ClientboundStopSoundPacket(name, source);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hexBytes = toHex(buf);

        int hasSource = source != null ? 1 : 0;
        int srcOrdinal = source != null ? source.ordinal() : -1;
        int hasName = name != null ? 1 : 0;
        String nameHex = name != null ? strHex(name.toString()) : "";

        // Round-trip decode sanity: rebuild from the wire and confirm fields match.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hexBytes)));
        ClientboundStopSoundPacket dec = CODEC.decode(rbuf);
        boolean ok =
            (dec.getSource() == null) == (source == null) &&
            (dec.getName() == null)   == (name == null);
        if (ok && source != null) ok = dec.getSource() == source;
        if (ok && name != null)   ok = dec.getName().toString().equals(name.toString());
        if (!ok) throw new IllegalStateException("round-trip mismatch for " + caseName);

        O.print("ENC\t");
        O.print(caseName);
        O.print('\t');
        O.print(hasSource);
        O.print('\t');
        O.print(srcOrdinal);
        O.print('\t');
        O.print(hasName);
        O.print('\t');
        O.print(nameHex.isEmpty() ? "-" : nameHex);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hexBytes.isEmpty() ? "-" : hexBytes);
        O.print('\n');
    }

    static String repeat(String unit, int n) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < n; i++) sb.append(unit);
        return sb.toString();
    }

    static String sanitize(String s) {
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < s.length(); i++) {
            char c = s.charAt(i);
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'))
                sb.append(c);
            else sb.append('_');
        }
        return sb.toString();
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static String strHex(String s) {
        byte[] u8 = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (byte x : u8) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        if (s.equals("-")) return new byte[0];
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
