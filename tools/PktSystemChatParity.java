// Ground truth for net.minecraft.network.protocol.game.ClientboundSystemChatPacket.
//
// STREAM_CODEC (ClientboundSystemChatPacket lines 12-18) =
//   StreamCodec.composite(
//     ComponentSerialization.TRUSTED_STREAM_CODEC, ::content,   // Component content
//     ByteBufCodecs.BOOL,                          ::overlay,   // boolean overlay
//     ::new)
// -> StreamCodec<RegistryFriendlyByteBuf, ClientboundSystemChatPacket>.
//
// composite() encodes fields in DECLARATION ORDER, so the body is exactly:
//   ComponentSerialization.TRUSTED_STREAM_CODEC.encode(buf, content)   // root NBT tag
//   ByteBufCodecs.BOOL.encode(buf, overlay)                            // 1 byte (writeBoolean)
//
// content uses TRUSTED_STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistriesTrusted(CODEC):
// it codec-encodes the Component to an NBT Tag, then FriendlyByteBuf.writeNbt(tag) emits it
// (NbtIo.writeAnyTag: type byte + UNNAMED payload). It is NOT optional (no leading present-
// bool) — the field is a bare Component, not Optional<Component>. For a PLAIN literal (no
// style, no siblings) the codec collapses to a bare StringTag, so content is a root StringTag:
//   08 <u16 MUTF8 len> <bytes>
// followed by the overlay bool byte (00/01). overlay=BOOL (ByteBufCodecs:56-64) writes a
// single boolean byte.
//
// This tool builds the REAL packet with Component.literal(text) and encodes it through the
// genuine packet STREAM_CODEC into a RegistryFriendlyByteBuf wrapping a bootstrapped
// RegistryAccess. We round-trip decode through the same codec for sanity.
//
//   tools/run_groundtruth.ps1 -Tool PktSystemChatParity -Out mcpp/build/pkt_system_chat.tsv
//
// Row format (tab separated):
//   ENC <name> <contentTextHex(UTF-8 of the literal)> <overlay-dec 0/1>
//       <readableBytes-dec> <hexBytes>
//   contentText is emitted as UTF-8 hex (ASCII TSV transport); the C++ test decodes it,
//   builds a StringTag, writes it via PacketBuffer.writeNbt(NbtTag::string_(text)) then the
//   overlay bool, and requires identical bytes + readableBytes.

import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSystemChatPacket;
import net.minecraft.server.Bootstrap;

public class PktSystemChatParity {
    static final java.io.PrintStream O = System.out;

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

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The packet codec is StreamCodec<RegistryFriendlyByteBuf,...>, so we need a real
        // RegistryAccess (built-in registries) to build the RegistryFriendlyByteBuf.
        RegistryAccess registries =
            RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        StreamCodec<RegistryFriendlyByteBuf, ClientboundSystemChatPacket> CODEC =
            (StreamCodec<RegistryFriendlyByteBuf, ClientboundSystemChatPacket>)
                ClientboundSystemChatPacket.STREAM_CODEC;

        // PLAIN-TEXT literal Components only (Component.literal) — the collapse-to-StringTag
        // domain. Includes empty, ASCII, spaces/slash, multibyte UTF-8 (niño/中文),
        // surrogate-pair emoji, and a long string. Each crossed with overlay=false/true.
        String[] texts = {
            "", "hi", "Hello, world!", "a b c", "/say x",
            "niño", "中文", "😀", "niño中文😀",
            "line1\nline2", "tab\there",
        };
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 300; i++) big.append('x');

        java.util.List<String> all = new java.util.ArrayList<>(java.util.Arrays.asList(texts));
        all.add(big.toString());

        int idx = 0;
        for (String text : all) {
            for (boolean overlay : new boolean[] { false, true }) {
                String name = "c" + (idx++);

                ClientboundSystemChatPacket pkt =
                    new ClientboundSystemChatPacket(Component.literal(text), overlay);

                // ENC through the REAL packet codec into a RegistryFriendlyByteBuf.
                RegistryFriendlyByteBuf buf =
                    new RegistryFriendlyByteBuf(Unpooled.buffer(), registries);
                CODEC.encode(buf, pkt);

                int readable = buf.readableBytes();
                String hexBytes = toHex(buf);

                // Sanity: round-trip decode through the SAME codec; assert fields recover.
                RegistryFriendlyByteBuf rbuf =
                    new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hexBytes)), registries);
                ClientboundSystemChatPacket dec = CODEC.decode(rbuf);
                String decText = dec.content().getString();
                if (!decText.equals(text) || dec.overlay() != overlay) {
                    throw new IllegalStateException(
                        "round-trip mismatch for " + name
                        + " in=(text=" + text + ",overlay=" + overlay + ")"
                        + " out=(text=" + decText + ",overlay=" + dec.overlay() + ")");
                }

                byte[] textUtf8 = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
                O.print("ENC\t");
                O.print(name);
                O.print('\t');
                O.print(hex(textUtf8));
                O.print('\t');
                O.print(overlay ? 1 : 0);
                O.print('\t');
                O.print(readable);
                O.print('\t');
                O.print(hexBytes);
                O.print('\n');
            }
        }
    }
}
