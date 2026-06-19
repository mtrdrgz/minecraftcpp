// Ground truth for net.minecraft.network.protocol.game.ClientboundSetSubtitleTextPacket.
//
//   public record ClientboundSetSubtitleTextPacket(Component text) ...
//   STREAM_CODEC = StreamCodec.composite(
//       ComponentSerialization.TRUSTED_STREAM_CODEC, ClientboundSetSubtitleTextPacket::text,
//       ClientboundSetSubtitleTextPacket::new);
//
// The packet is a single Component field encoded via ComponentSerialization.TRUSTED_STREAM_CODEC
// (NO Optional framing). TRUSTED_STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistries(CODEC): it
// codec-encodes the Component to an NBT Tag, then FriendlyByteBuf.writeNbt(tag) emits it
// (NbtIo.writeAnyTag: type byte + UNNAMED payload). For a PLAIN literal (no style, no siblings)
// the codec collapses to a bare StringTag, so the whole packet body is a root StringTag:
//   08 <u16 MUTF8 len> <bytes>.
// This tool ENCODES the REAL packet (Component.literal(text)) through the REAL packet STREAM_CODEC
// (StreamCodec<RegistryFriendlyByteBuf, ...>) and dumps the exact bytes so the C++ side
// (PacketBuffer.writeNbt(NbtTag::string_(text))) can byte-match. Round-trips for a sanity check.
//
//   tools/run_groundtruth.ps1 -Tool PktSetSubtitleTextParity -Out mcpp/build/pkt_set_subtitle_text.tsv
//
// Row: ENC \t <name> \t <textHex(UTF-8 of the literal)> \t <readableBytes> \t <hexBytes>
//   text emitted as UTF-8 hex (ASCII TSV transport); the C++ test decodes it, builds a StringTag,
//   writes it via the same codec order, and requires identical bytes + that the leading byte is 0x08.

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.protocol.game.ClientboundSetSubtitleTextPacket;

public class PktSetSubtitleTextParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x));
        return s.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The packet codec is StreamCodec<RegistryFriendlyByteBuf, ...>, so we need a
        // RegistryFriendlyByteBuf. A registry access is required to construct one even
        // though a TRUSTED plain-text Component touches no registries on the wire.
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // PLAIN literals only (no style/siblings) — the collapse-to-StringTag domain.
        // empty, ASCII, spaces, command-like, multibyte UTF-8, surrogate-pair emoji, long.
        String[][] cases = {
            {"empty",      ""},
            {"ascii",      "hi"},
            {"hello",      "Hello, world!"},
            {"spaces",     "a b c"},
            {"command",    "/say x"},
            {"nino",       "niño"},
            {"cjk",        "中文"},
            {"emoji",      "😀"},
            {"mixed",      "niño中文😀"},
            {"newline",    "line1\nline2"},
            {"tab",        "tab\there"},
        };
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 300; i++) big.append('x');

        java.util.List<String[]> all = new java.util.ArrayList<>(java.util.Arrays.asList(cases));
        all.add(new String[] {"long", big.toString()});

        for (String[] c : all) {
            String name = c[0];
            String text = c[1];
            ClientboundSetSubtitleTextPacket pkt = new ClientboundSetSubtitleTextPacket(Component.literal(text));

            RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
            ClientboundSetSubtitleTextPacket.STREAM_CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            byte[] bytes = new byte[readable];
            buf.duplicate().readBytes(bytes);

            // Round-trip decode for sanity: must reproduce the same literal text.
            RegistryFriendlyByteBuf rbuf = new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(bytes), access);
            ClientboundSetSubtitleTextPacket back = ClientboundSetSubtitleTextPacket.STREAM_CODEC.decode(rbuf);
            String got = back.text().getString();
            if (!got.equals(text)) {
                throw new IllegalStateException("round-trip mismatch for '" + name + "': '" + got + "' != '" + text + "'");
            }

            byte[] textUtf8 = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            O.println("ENC\t" + name + "\t" + hex(textUtf8) + "\t" + readable + "\t" + hex(bytes));
        }
    }
}
