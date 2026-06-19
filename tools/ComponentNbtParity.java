// Ground truth for the PLAY-protocol Component wire form: ComponentSerialization's
// STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistries(CODEC) — it codec-encodes the
// Component to an NBT Tag, then FriendlyByteBuf.writeNbt(tag) emits it (NbtIo.writeAnyTag:
// type byte + UNNAMED payload). For a PLAIN literal (no style, no siblings) the codec
// collapses to a bare StringTag, so the wire is a root StringTag: 08 <u16 MUTF8 len> <bytes>.
// This tool ENCODES Component.literal(text) via the REAL TRUSTED_STREAM_CODEC and dumps the
// exact bytes so the C++ side (PacketBuffer.writeNbt(NbtTag::string_(text))) can byte-match.
//
//   tools/run_groundtruth.ps1 -Tool ComponentNbtParity -Out mcpp/build/component_nbt.tsv
//
// Row: NBT \t <textHex(UTF-8 of the literal)> \t <readableBytes> \t <wireHex>
//   text emitted as UTF-8 hex (ASCII TSV transport); the C++ test decodes it, builds a
//   StringTag, writes it, and requires identical bytes + that the leading byte is 0x08.

import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;

public class ComponentNbtParity {
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

        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // Plain literals only (no style/siblings) — the collapse-to-string domain. Includes
        // empty, ASCII, spaces, multibyte UTF-8, surrogate-pair emoji, and a long string.
        String[] texts = {
            "", "hi", "Hello, world!", "a b c", "/say x",
            "niño", "中文", "😀", "niño中文😀",
            "line1\nline2", "tab\there",
        };
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 300; i++) big.append('x');

        java.util.List<String> all = new java.util.ArrayList<>(java.util.Arrays.asList(texts));
        all.add(big.toString());

        for (String text : all) {
            Component comp = Component.literal(text);
            RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
            ComponentSerialization.TRUSTED_STREAM_CODEC.encode(buf, comp);
            int readable = buf.readableBytes();
            byte[] bytes = new byte[readable];
            buf.duplicate().readBytes(bytes);
            byte[] textUtf8 = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            O.println("NBT\t" + hex(textUtf8) + "\t" + readable + "\t" + hex(bytes));
        }
    }
}
