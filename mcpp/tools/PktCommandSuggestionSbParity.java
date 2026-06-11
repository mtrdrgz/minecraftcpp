// Ground truth for net.minecraft.network.protocol.game.ServerboundCommandSuggestionPacket.
//
// The packet carries (id, command). Its STREAM_CODEC (built via
// Packet.codec(write, new)) writes EXACTLY:
//     output.writeVarInt(this.id);          // plain LEB128 VarInt (NOT zig-zag)
//     output.writeUtf(this.command, 32500); // VarInt byte-length prefix + UTF-8 bytes,
//                                            //   maxLength = 32500 (NOT the default 32767)
// and reads back input.readVarInt() / input.readUtf(32500). No packet-type id is part of
// the codec bytes (that framing lives outside the StreamCodec).
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ServerboundCommandSuggestionPacket.java):
//     public static final StreamCodec<FriendlyByteBuf, ServerboundCommandSuggestionPacket> STREAM_CODEC =
//         Packet.codec(ServerboundCommandSuggestionPacket::write, ServerboundCommandSuggestionPacket::new);
//     private void write(final FriendlyByteBuf output) {
//        output.writeVarInt(this.id);
//        output.writeUtf(this.command, 32500);
//     }
//     private ServerboundCommandSuggestionPacket(final FriendlyByteBuf input) {
//        this.id = input.readVarInt();
//        this.command = input.readUtf(32500);
//     }
//
// id is a raw transaction id (a plain int): writeVarInt is plain LEB128, so negative ids
// encode to five bytes. This is the command-suggestion request the client sends as the
// player types into a command (the tab-complete request).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf and dump
// readableBytes() + the raw hex; we also decode the bytes back through the SAME codec and
// re-emit id+command so the C++ side proves read parity too.
//
//   ENC <name>\t<id-dec>\t<command_utf8_hex>\t<readableBytes>\t<hex>
//
// The command column is emitted as UTF-8 HEX so the exact bytes survive the TSV transport
// (run_groundtruth.ps1 captures Java stdout as ASCII, which would mangle raw multi-byte
// UTF-8 and byte parity is this gate's whole point).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundCommandSuggestionPacket;

public class PktCommandSuggestionSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundCommandSuggestionPacket> codec =
            ServerboundCommandSuggestionPacket.STREAM_CODEC;

        // VarInt id boundaries (plain LEB128) exercise the 1->5 byte widths, and the
        // command's VarInt LENGTH PREFIX crosses byte boundaries (127->1, 128->2,
        // 16383->2, 16384->3). Single-byte ASCII 'x' so chars==bytes for the length cases.
        String len127   = repeat('x', 127);   // 1-byte VarInt length prefix
        String len128   = repeat('x', 128);   // 2-byte VarInt length prefix
        String len16383 = repeat('x', 16383); // 2-byte VarInt length prefix (max)
        String len16384 = repeat('x', 16384); // 3-byte VarInt length prefix

        Object[][] cases = {
            // name              id              command
            { "id0_empty",       0,             "" },
            { "id1_slash",       1,             "/" },
            { "basic",           7,             "/give @p stone" },
            { "id127",           127,           "/tp" },              // id: 1->2 byte boundary
            { "id128",           128,           "/tp" },              // id: 2-byte VarInt
            { "id16383",         16383,         "ab" },               // id: 2->3 byte boundary
            { "id16384",         16384,         "ab" },               // id: 3-byte VarInt
            { "id2097151",       2097151,       "x" },                // id: 3->4 byte boundary
            { "id2097152",       2097152,       "x" },                // id: 4-byte VarInt
            { "id268435455",     268435455,     "x" },                // id: 4->5 byte boundary
            { "id268435456",     268435456,     "x" },                // id: 5-byte VarInt
            { "id_neg1",         -1,            "/help" },            // negative -> 5-byte VarInt
            { "id_neg128",       -128,          "/help" },            // negative -> 5-byte VarInt
            { "id_max",          Integer.MAX_VALUE, "z" },
            { "id_min",          Integer.MIN_VALUE, "z" },            // negative -> 5-byte VarInt
            { "spaces",          3,             "/give @p minecraft:diamond_sword 1" },
            { "punct",           4,             "/say !@#$%^&*()_+-=[]{}" },
            { "newline",         5,             "/say line1\nline2" }, // \n is a valid UTF char
            { "tab",             6,             "/say a\tb" },
            { "unicode_nino",    8,             "/say niño" },        // ñ -> 2-byte UTF-8
            { "unicode_cjk",     9,             "/say 中文" },         // 中文 -> 3-byte each
            { "unicode_emoji",   10,            "/say 😀" },          // U+1F600 surrogate pair -> 4-byte
            { "mixed",           11,            "/say niño 中文 😀" },
            { "cmd_len127",      12,            len127 },              // 1-byte length prefix
            { "cmd_len128",      13,            len128 },              // 2-byte length prefix
            { "cmd_len16383",    14,            len16383 },            // 2-byte length prefix (boundary)
            { "cmd_len16384",    15,            len16384 },            // 3-byte length prefix (boundary)
        };

        for (Object[] c : cases) {
            String caseName = (String) c[0];
            int id          = (Integer) c[1];
            String command  = (String) c[2];

            ServerboundCommandSuggestionPacket pkt =
                new ServerboundCommandSuggestionPacket(id, command);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundCommandSuggestionPacket back = codec.decode(buf);
            if (back.getId() != id || !back.getCommand().equals(command)) {
                throw new IllegalStateException("round-trip mismatch for " + caseName);
            }

            // Emit the command as UTF-8 HEX so the exact bytes survive the TSV transport.
            StringBuilder cmdHex = new StringBuilder();
            for (byte bb : command.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                cmdHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(caseName);
            O.print('\t');
            O.print(id);
            O.print('\t');
            O.print(cmdHex);
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
