// Ground truth for net.minecraft.network.protocol.game.ServerboundSetCommandMinecartPacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet body is exactly (ServerboundSetCommandMinecartPacket.java:33-37):
//   write(FriendlyByteBuf output):
//     output.writeVarInt(this.entity);     // LEB128 VarInt
//     output.writeUtf(this.command);       // VarInt byte-len + UTF-8 (default maxLen 32767)
//     output.writeBoolean(this.trackOutput);// single byte 0/1
//   read(FriendlyByteBuf input)  (ServerboundSetCommandMinecartPacket.java:27-31):
//     this.entity      = input.readVarInt();
//     this.command     = input.readUtf();   // default maxLen 32767
//     this.trackOutput = input.readBoolean();
//
// Packet.codec -> StreamCodec.ofMember (Packet.java): body only, NO packet-id or
// length prefix on the wire.
//
// Utf8String.write (FriendlyByteBuf.writeUtf): VarInt(byte length) + UTF-8 bytes.
//
// Row formats (tab separated). entity/readableBytes are decimal; trackOutput is
// decimal 0/1; the command string is emitted as lowercase UTF-8 HEX (ASCII-safe
// transport for the TSV); hexBytes is lowercase %02x of the encoded body.
//   ENC \t <name> \t <entity> \t <commandHex> \t <trackOutput> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeVarInt(entity) + writeString(command) + writeBool(trackOutput)) and must
// match byte-for-byte, then round-trips the bytes back to (entity, command, trackOutput).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSetCommandMinecartPacket;

public class PktSetCommandMinecartSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundSetCommandMinecartPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSetCommandMinecartPacket>)
                ServerboundSetCommandMinecartPacket.STREAM_CODEC;

        // Finite / physical battery. Columns: name, entity, command, trackOutput.
        // entity is a VarInt: exercise zero, ones at every VarInt byte boundary
        // (127/128/16383/16384/2097151/2097152/268435455), Integer.MAX/MIN (negative
        // encodes to a 5-byte VarInt). command exercises: empty, ASCII, multibyte
        // UTF-8 (2/3/4-byte: e accent, euro, emoji), VarInt length boundary (127/128
        // byte lengths), whitespace, special command chars. trackOutput is a bool.
        Object[][] cases = {
            // {entity, command, trackOutput}
            {0, "", false},
            {0, "", true},
            {1, "/say hi", true},
            {127, "/time set day", false},
            {128, "/setblock ~ ~ ~ stone", true},
            {16383, "/give @p minecraft:diamond 64", false},
            {16384, "/fill ~-1 ~ ~-1 ~1 ~ ~1 air", true},
            {2097151, "/tp @p 0 64 0", false},
            {2097152, "/effect give @s speed 10 1", true},
            {268435455, "/gamemode creative", false},
            {2147483647, "/execute as @e run say boundary", true},
            {-1, "café €uro 😀", false},
            {-2147483648, "mix é€😀 end", true},
            {42, repeat('A', 127), false},
            {43, repeat('B', 128), true},
            {12345, " leading and trailing ", false},
            {54321, "\ttab\nnewline\r", true},
            {99, "/data merge entity @s {NoGravity:1b}", false},
        };

        for (Object[] c : cases) {
            int entity = (Integer) c[0];
            String command = (String) c[1];
            boolean trackOutput = (Boolean) c[2];

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundSetCommandMinecartPacket pkt =
                new ServerboundSetCommandMinecartPacket(entity, command, trackOutput);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; sanity-assert field equality.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundSetCommandMinecartPacket dec = CODEC.decode(rbuf);
            if (!dec.getCommand().equals(command))
                throw new IllegalStateException("command roundtrip mismatch");
            if (dec.isTrackOutput() != trackOutput)
                throw new IllegalStateException("trackOutput roundtrip " + dec.isTrackOutput()
                    + " != " + trackOutput);
            // entity field is private with no accessor; verify via re-encode equality.
            FriendlyByteBuf rbuf2 = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(rbuf2, dec);
            if (!toHex(rbuf2).equals(hex))
                throw new IllegalStateException("entity roundtrip re-encode mismatch");

            String name = "case_e" + entity + "_t" + trackOutput;
            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(entity);
            O.print('\t');
            O.print(utf8Hex(command));
            O.print('\t');
            O.print(trackOutput ? 1 : 0);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static String repeat(char ch, int n) {
        StringBuilder sb = new StringBuilder(n);
        for (int i = 0; i < n; i++) sb.append(ch);
        return sb.toString();
    }

    static String utf8Hex(String s) {
        byte[] b = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
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
