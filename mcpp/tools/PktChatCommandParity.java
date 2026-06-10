// Ground truth for net.minecraft.network.protocol.game.ServerboundChatCommandPacket.
//
// The packet is a record (String command). Its STREAM_CODEC (built via
// Packet.codec(write, new)) writes ONLY:
//     output.writeUtf(this.command);   // VarInt byte-length prefix + UTF-8 bytes
// and reads back input.readUtf(). No packet-type id is part of the codec bytes
// (that framing lives outside the StreamCodec).
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ServerboundChatCommandPacket.java):
//     private void write(final FriendlyByteBuf output) { output.writeUtf(this.command); }
//     private ServerboundChatCommandPacket(final FriendlyByteBuf input) { this(input.readUtf()); }
//
// writeUtf -> Utf8String.write: VarInt(byteLength) then the UTF-8 bytes; the
// default maxLength is 32767 (FriendlyByteBuf.MAX_STRING_LENGTH). This is the
// UNSIGNED chat-command packet; the SIGNED variant
// (ServerboundChatCommandSignedPacket) additionally carries timeStamp/salt/
// ArgumentSignatures/LastSeenMessages and is intentionally NOT covered here.
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and re-emit command so the C++ side proves read parity too.
//
//   ENC <name>\t<command_raw>\t<readableBytes>\t<hex>
//
// Note: command is emitted raw (it is the exact UTF-8 String the packet carries);
// the C++ test parses the leading two tab-separated fields then bytes/hex.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundChatCommandPacket;

public class PktChatCommandParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundChatCommandPacket> codec =
            ServerboundChatCommandPacket.STREAM_CODEC;

        // Finite/physical cases: empty, plain ascii commands, commands with args
        // and spaces, leading slash (server form), unicode (multi-byte UTF-8,
        // incl. a surrogate-pair emoji so the VarInt byte length exceeds the
        // UTF-16 char count), and a length that crosses the 1->2 byte VarInt
        // boundary for the length prefix (>=128 bytes).
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 200; i++) big.append('x');   // 200 bytes -> 2-byte VarInt prefix

        StringBuilder repeated = new StringBuilder();
        for (int i = 0; i < 64; i++) repeated.append("ab ");  // 192 bytes incl. spaces

        Object[][] cases = {
            { "empty",        "" },
            { "say",          "say hello" },
            { "gamemode",     "gamemode creative" },
            { "tp_coords",    "tp 0 64 0" },
            { "with_slash",   "/time set day" },
            { "tabs_in_args", "give @p stone 64" },
            { "trailing_sp",  "say trailing " },
            { "leading_sp",   " say leading" },
            { "unicode_nino", "say niño" },
            { "unicode_cjk",  "say 中文" },
            { "unicode_emoji","say 😀" },               // U+1F600, surrogate pair
            { "mixed",        "tellraw @a niño 中文 😀" },
            { "len127",       repeated.substring(0, 127) },        // 127 bytes -> 1-byte prefix
            { "len128",       repeated.substring(0, 128) },        // 128 bytes -> 2-byte prefix
            { "len200",       big.toString() },                    // 200 bytes -> 2-byte prefix
            { "newline",      "say line1\nline2" },                // \n is a valid UTF char in the codec
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String command = (String) c[1];

            ServerboundChatCommandPacket pkt = new ServerboundChatCommandPacket(command);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundChatCommandPacket back = codec.decode(buf);
            if (!back.command().equals(command)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // Emit the command as UTF-8 HEX so the exact bytes survive the TSV transport:
            // run_groundtruth.ps1 captures Java stdout and writes the TSV as ASCII, which
            // would mangle raw multi-byte UTF-8 (and byte parity is this gate's whole point).
            // The C++ side decodes this hex back to the exact byte string before writeUtf.
            StringBuilder cmdHex = new StringBuilder();
            for (byte bb : command.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                cmdHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(cmdHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
