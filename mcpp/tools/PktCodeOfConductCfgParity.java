// Ground truth for net.minecraft.network.protocol.configuration.ClientboundCodeOfConductPacket.
//
// The packet is a record (String codeOfConduct). Its STREAM_CODEC is built via
//   StreamCodec.composite(ByteBufCodecs.STRING_UTF8, ClientboundCodeOfConductPacket::codeOfConduct,
//                          ClientboundCodeOfConductPacket::new)
// so the entire codec body is exactly ONE field:
//     ByteBufCodecs.STRING_UTF8  (== stringUtf8(32767))
// which calls net.minecraft.network.Utf8String.write(output, value, 32767):
//     VarInt(byteLength) then the UTF-8 bytes.  read() is the symmetric Utf8String.read.
// This is byte-identical to FriendlyByteBuf.writeUtf(s)/readUtf() with maxLength 32767.
//
// Source (26.1.2/src/net/minecraft/network/protocol/configuration/ClientboundCodeOfConductPacket.java):
//     public record ClientboundCodeOfConductPacket(String codeOfConduct) ...
//     STREAM_CODEC = StreamCodec.composite(ByteBufCodecs.STRING_UTF8,
//                        ClientboundCodeOfConductPacket::codeOfConduct, ClientboundCodeOfConductPacket::new);
// No packet-type id is part of the codec bytes (framing lives outside the StreamCodec).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf and
// dump readableBytes() + the raw hex; we also decode the bytes back through the SAME
// codec and re-check codeOfConduct so the C++ side proves read parity too.
//
//   ENC <name>\t<codeOfConduct UTF-8 hex>\t<readableBytes>\t<hex>
//
// codeOfConduct is emitted as LOWERCASE UTF-8 HEX so the exact bytes survive the TSV
// transport (run_groundtruth captures stdout as ASCII, which would mangle multi-byte
// UTF-8 and byte parity is this gate's whole point). The C++ side decodes this hex
// back to the exact byte string before writeString.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.configuration.ClientboundCodeOfConductPacket;

public class PktCodeOfConductCfgParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // STREAM_CODEC is StreamCodec<ByteBuf, ClientboundCodeOfConductPacket>; FriendlyByteBuf
        // IS a ByteBuf so we can encode/decode through it directly and read readableBytes().
        StreamCodec<ByteBuf, ClientboundCodeOfConductPacket> codec =
            ClientboundCodeOfConductPacket.STREAM_CODEC;

        // Finite/physical cases exercising the VarInt byte-length prefix boundaries
        // (127->1 byte, 128->2 byte prefix), empty, ascii, multi-line text, ascii+
        // multi-byte UTF-8 incl. a surrogate-pair emoji so the encoded byte count
        // exceeds the UTF-16 char count, and newlines (a code of conduct is body text).
        StringBuilder s127 = new StringBuilder();
        for (int i = 0; i < 127; i++) s127.append('a');   // 127 bytes -> 1-byte VarInt prefix
        StringBuilder s128 = new StringBuilder();
        for (int i = 0; i < 128; i++) s128.append('b');   // 128 bytes -> 2-byte VarInt prefix
        StringBuilder s300 = new StringBuilder();
        for (int i = 0; i < 300; i++) s300.append('c');   // 300 bytes -> 2-byte VarInt prefix

        Object[][] cases = {
            { "empty",         "" },
            { "short_ascii",   "Be excellent to each other." },
            { "multiline",     "Rule 1: Be kind.\nRule 2: No griefing.\n" },
            { "with_tab",      "Section\t1" },
            { "unicode_nino",  "Sé niño" },
            { "unicode_cjk",   "中文行为准则" },   // 中文行为准则
            { "unicode_emoji", "Welcome 😀" },                  // U+1F600 surrogate pair
            { "mixed",         "Be kind niño 中文 😀" },
            { "len127",        s127.toString() },
            { "len128",        s128.toString() },
            { "len300",        s300.toString() },
            { "crlf",          "line1\r\nline2" },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String text = (String) c[1];

            ClientboundCodeOfConductPacket pkt = new ClientboundCodeOfConductPacket(text);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundCodeOfConductPacket back = codec.decode(buf);
            if (!back.codeOfConduct().equals(text)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // Emit codeOfConduct as UTF-8 HEX (ASCII-safe TSV transport).
            StringBuilder txtHex = new StringBuilder();
            for (byte bb : text.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                txtHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(txtHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
