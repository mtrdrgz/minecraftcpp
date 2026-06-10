// Ground truth for net.minecraft.network.protocol.game.ClientboundSetActionBarTextPacket.
//
// The packet is a single-field record (Component text). Its STREAM_CODEC is:
//     StreamCodec.composite(
//         ComponentSerialization.TRUSTED_STREAM_CODEC, ...::text, ...::new)
// (Source: 26.1.2/src/net/minecraft/network/protocol/game/ClientboundSetActionBarTextPacket.java)
//
// So the entire wire payload is ONE Component encoded via TRUSTED_STREAM_CODEC
// (= ByteBufCodecs.fromCodecWithRegistriesTrusted(ComponentSerialization.CODEC)).
// There is NO Optional framing here — the codec is the plain (non-optional) Component
// stream codec, so the bytes are exactly the Component's NBT root, nothing else.
//
// For a PLAIN literal Component (no style, no siblings) the CODEC collapses via
// Component.tryCollapseToString() to a bare StringTag, and FriendlyByteBuf.writeNbt(tag)
// (NbtIo.writeAnyTag: type byte + UNNAMED payload) emits a root StringTag:
//     08 <u16 MUTF8 byte-length> <MUTF8 bytes>
// That is the exact same form certified by component_nbt_parity, now exercised through
// the REAL packet codec so the framing (no extra prefix/suffix) is proven too.
//
// We RESTRICT this gate to plain-text literal Components (Component.literal(text)) — the
// collapse-to-StringTag domain. Styled/translatable/keybind Components use the CompoundTag
// form (not yet ported on the C++ side) and are intentionally NOT fed here.
//
// The packet codec is StreamCodec<RegistryFriendlyByteBuf, ...>, so we build a
// RegistryFriendlyByteBuf from RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY).
//
//   tools/run_groundtruth.ps1 -Tool PktSetActionBarTextParity -Out mcpp/build/pkt_set_actionbar_text.tsv
//
// Row: ENC \t <name> \t <textHex(UTF-8 of the literal)> \t <readableBytes> \t <wireHex>
//   The Component literal text is emitted as UTF-8 LOWERCASE HEX so the exact bytes
//   survive the ASCII TSV transport (the runner writes Java stdout as ASCII, which would
//   mangle multi-byte UTF-8; byte parity is this gate's whole point).

import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetActionBarTextPacket;

public class PktSetActionBarTextParity {
    static final java.io.PrintStream O = System.out;

    static String hexUtf8(String s) {
        StringBuilder sb = new StringBuilder();
        for (byte b : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            sb.append(String.format("%02x", b));
        return sb.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        StreamCodec<RegistryFriendlyByteBuf, ClientboundSetActionBarTextPacket> codec =
            ClientboundSetActionBarTextPacket.STREAM_CODEC;

        // Plain literals only (no style/siblings) — the collapse-to-string domain. Includes
        // empty, ASCII, spaces, slash-command-looking, multibyte UTF-8 (niño/中文),
        // a surrogate-pair emoji, control chars, and a long (300-byte) string.
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 300; i++) big.append('x');

        Object[][] cases = {
            { "empty",        "" },
            { "ascii",        "hi" },
            { "hello_world",  "Hello, world!" },
            { "spaces",       "a b c" },
            { "slash",        "/say x" },
            { "unicode",      "niño" },
            { "cjk",          "中文" },
            { "emoji",        "😀" },           // U+1F600 surrogate pair
            { "mixed",        "niño中文😀" },
            { "newline",      "line1\nline2" },
            { "tab",          "tab\there" },
            { "long_300",     big.toString() },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String text = (String) c[1];

            ClientboundSetActionBarTextPacket pkt =
                new ClientboundSetActionBarTextPacket(Component.literal(text));

            RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side. The
            // decoded Component must collapse back to the same literal string.
            ClientboundSetActionBarTextPacket back = codec.decode(buf);
            String backText = back.text().getString();
            if (!backText.equals(text)) {
                throw new IllegalStateException("round-trip mismatch for " + name
                    + " got=[" + backText + "] want=[" + text + "]");
            }

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(hexUtf8(text));
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
