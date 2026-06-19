// Ground truth for net.minecraft.network.protocol.game.ServerboundRenameItemPacket.
//
// The packet carries a single String name. Its STREAM_CODEC (built via
// Packet.codec(write, new)) writes ONLY:
//     output.writeUtf(this.name);   // VarInt byte-length prefix + UTF-8 bytes
// and reads back input.readUtf(). No packet-type id is part of the codec bytes
// (that framing lives outside the StreamCodec).
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ServerboundRenameItemPacket.java):
//     public static final StreamCodec<FriendlyByteBuf, ServerboundRenameItemPacket> STREAM_CODEC =
//         Packet.codec(ServerboundRenameItemPacket::write, ServerboundRenameItemPacket::new);
//     private void write(final FriendlyByteBuf output) { output.writeUtf(this.name); }
//     private ServerboundRenameItemPacket(final FriendlyByteBuf input) { this.name = input.readUtf(); }
//
// writeUtf -> Utf8String.write: VarInt(byteLength) then the UTF-8 bytes; the
// default maxLength is 32767 (FriendlyByteBuf.MAX_STRING_LENGTH). This is the
// anvil "rename item" packet sent client->server when the player types a name.
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and re-emit name so the C++ side proves read parity too.
//
//   ENC <name>\t<name_utf8_hex>\t<readableBytes>\t<hex>
//
// The name column is emitted as UTF-8 HEX so the exact bytes survive the TSV
// transport (run_groundtruth.ps1 captures Java stdout as ASCII, which would
// mangle raw multi-byte UTF-8 and byte parity is this gate's whole point).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundRenameItemPacket;

public class PktRenameItemSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundRenameItemPacket> codec =
            ServerboundRenameItemPacket.STREAM_CODEC;

        // Finite/physical cases. The VarInt LENGTH PREFIX is what crosses byte
        // boundaries here: a byte length of 127 -> 1-byte VarInt prefix, 128 ->
        // 2-byte prefix, 16383 -> 2-byte, 16384 -> 3-byte. We build strings of
        // those exact UTF-8 byte lengths (single-byte ASCII 'x' so chars==bytes).
        String len127  = repeat('x', 127);    // 1-byte VarInt prefix
        String len128  = repeat('x', 128);    // 2-byte VarInt prefix
        String len16383 = repeat('x', 16383); // 2-byte VarInt prefix (max)
        String len16384 = repeat('x', 16384); // 3-byte VarInt prefix

        Object[][] cases = {
            { "empty",         "" },
            { "zero_char",     "0" },
            { "space",         " " },
            { "plain",         "Excalibur" },
            { "with_spaces",   "My Cool Sword" },
            { "leading_sp",    " leading" },
            { "trailing_sp",   "trailing " },
            { "punct",         "!@#$%^&*()_+-=[]{}" },
            { "digits",        "1234567890" },
            { "newline",       "line1\nline2" },             // \n is a valid UTF char
            { "tab",           "a\tb" },
            { "unicode_nino",  "niño" },                // ñ -> 2-byte UTF-8
            { "unicode_cjk",   "中文" },             // 中文 -> 3-byte each
            { "unicode_emoji", "Sword 😀" },       // U+1F600 surrogate pair -> 4-byte
            { "mixed",         "niño 中文 😀" },
            { "len127",        len127 },                     // 1-byte prefix
            { "len128",        len128 },                     // 2-byte prefix
            { "len16383",      len16383 },                   // 2-byte prefix (boundary)
            { "len16384",      len16384 },                   // 3-byte prefix (boundary)
        };

        for (Object[] c : cases) {
            String caseName = (String) c[0];
            String name = (String) c[1];

            ServerboundRenameItemPacket pkt = new ServerboundRenameItemPacket(name);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundRenameItemPacket back = codec.decode(buf);
            if (!back.getName().equals(name)) {
                throw new IllegalStateException("round-trip mismatch for " + caseName);
            }

            // Emit the name as UTF-8 HEX so the exact bytes survive the TSV transport.
            StringBuilder nameHex = new StringBuilder();
            for (byte bb : name.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                nameHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(caseName);
            O.print('\t');
            O.print(nameHex);
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
}
