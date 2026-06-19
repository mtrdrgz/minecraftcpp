// Ground truth for net.minecraft.network.protocol.game.ClientboundSetTitleTextPacket.
//
// The packet is `record ClientboundSetTitleTextPacket(Component text)` and its codec is:
//   STREAM_CODEC = StreamCodec.composite(
//       ComponentSerialization.TRUSTED_STREAM_CODEC, ClientboundSetTitleTextPacket::text,
//       ClientboundSetTitleTextPacket::new)
// (net.minecraft.network.protocol.game.ClientboundSetTitleTextPacket lines 10-13).
// So the entire wire payload is a single Component encoded via TRUSTED_STREAM_CODEC =
// ByteBufCodecs.fromCodecWithRegistriesTrusted(ComponentSerialization.CODEC). For a PLAIN
// literal Component (no style, no siblings) the CODEC collapses to a bare StringTag
// (Component.tryCollapseToString -> Either.left(Either.left(text)) -> Codec.STRING under
// NBT ops), and FriendlyByteBuf.writeNbt(tag) emits it as a root StringTag:
//   08 <u16 MUTF8 len> <bytes>
// There is NO Optional framing and NO other field — the packet IS exactly one Component.
// The codec is StreamCodec<RegistryFriendlyByteBuf, ...>, so we build a
// RegistryFriendlyByteBuf via RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY).
//
//   tools/run_groundtruth.ps1 -Tool PktSetTitleTextParity -Out mcpp/build/pkt_set_title_text.tsv
//
// Row format (tab separated):
//   ENC <textHex(UTF-8 of the literal)> <readableBytes-dec> <wireHex>
//   text emitted as UTF-8 hex (ASCII TSV transport); the C++ test decodes it, builds a
//   StringTag, writes it via PacketBuffer.writeNbt(NbtTag), and requires identical bytes +
//   readableBytes. We round-trip-decode every case through the SAME packet codec and assert
//   the recovered text equals the input as a sanity check before emitting.

import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetTitleTextPacket;
import net.minecraft.server.Bootstrap;

public class PktSetTitleTextParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The packet codec is StreamCodec<RegistryFriendlyByteBuf, ...>; build a registry
        // access so the RegistryFriendlyByteBuf the codec needs can be constructed.
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // The real StreamCodec for this packet (RegistryFriendlyByteBuf, packet).
        StreamCodec<RegistryFriendlyByteBuf, ClientboundSetTitleTextPacket> CODEC =
            (StreamCodec<RegistryFriendlyByteBuf, ClientboundSetTitleTextPacket>)
                ClientboundSetTitleTextPacket.STREAM_CODEC;

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
            // ENC: construct the real packet with a plain literal, encode through the real
            // packet codec, dump bytes.
            ClientboundSetTitleTextPacket pkt =
                new ClientboundSetTitleTextPacket(Component.literal(text));
            RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
            CODEC.encode(buf, pkt);

            int readable = buf.readableBytes();
            String wireHex = toHex(buf);

            // Sanity: round-trip decode through the SAME codec and assert the recovered
            // literal text equals the input.
            RegistryFriendlyByteBuf rbuf =
                new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(wireHex)), access);
            ClientboundSetTitleTextPacket dec = CODEC.decode(rbuf);
            String decText = dec.text().getString();
            if (!decText.equals(text)) {
                throw new IllegalStateException(
                    "round-trip mismatch: in=[" + text + "] out=[" + decText + "]");
            }

            byte[] textUtf8 = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            O.print("ENC\t");
            O.print(hex(textUtf8));
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(wireHex);
            O.print('\n');
        }
    }

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x & 0xff));
        return s.toString();
    }

    static String toHex(ByteBuf b) {
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
