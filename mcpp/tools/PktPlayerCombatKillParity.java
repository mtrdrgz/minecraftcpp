// Ground truth for the ClientboundPlayerCombatKillPacket codec.
//
// net.minecraft.network.protocol.game.ClientboundPlayerCombatKillPacket
// (26.1.2/src/.../ClientboundPlayerCombatKillPacket.java lines 11-18):
//
//   public record ClientboundPlayerCombatKillPacket(int playerId, Component message)
//       implements Packet<ClientGamePacketListener> {
//      public static final StreamCodec<RegistryFriendlyByteBuf, ClientboundPlayerCombatKillPacket>
//          STREAM_CODEC = StreamCodec.composite(
//             ByteBufCodecs.VAR_INT,                          ::playerId
//             ComponentSerialization.TRUSTED_STREAM_CODEC,    ::message
//             ::new);
//   }
//
// So the on-wire body is EXACTLY: VarInt(playerId) ++ Component(message).
//   - ByteBufCodecs.VAR_INT encodes playerId with VarInt.write (ByteBufCodecs.java 102-110).
//   - ComponentSerialization.TRUSTED_STREAM_CODEC = ByteBufCodecs.fromCodecWithRegistriesTrusted(CODEC)
//     (ComponentSerialization.java line 40) — it codec-encodes the Component to an NBT Tag,
//     then FriendlyByteBuf.writeNbt(tag) emits it (NbtIo.writeAnyTag: type byte + UNNAMED
//     payload). For a PLAIN literal (no style, no siblings) the codec collapses to a bare
//     StringTag, so that span is a root StringTag: 08 <u16 MUTF8 len> <bytes>. This is the
//     NON-optional TRUSTED variant — there is NO Optional/boolean framing.
//
// The packet codec is StreamCodec<RegistryFriendlyByteBuf,...>, so we build a
// RegistryFriendlyByteBuf via RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY)
// and drive the REAL packet (encode + round-trip decode for sanity). We RESTRICT message to
// PLAIN-TEXT literal Components (Component.literal(text)) — the collapse-to-StringTag domain.
//
// Row: ENC <name> <playerId-dec> <msgTextHex(UTF-8 of the literal)> <readableBytes> <hexBytes>
//   msg text emitted as UTF-8 hex (ASCII TSV transport); the C++ side decodes it, builds a
//   StringTag, writes VarInt(playerId) + writeNbt(StringTag), and requires identical bytes.
//
//   tools/run_groundtruth.ps1 -Tool PktPlayerCombatKillParity -Out mcpp/build/pkt_player_combat_kill.tsv
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundPlayerCombatKillPacket;

public class PktPlayerCombatKillParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x & 0xff));
        return s.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The packet codec is StreamCodec<RegistryFriendlyByteBuf, ...>, so we need a
        // RegistryFriendlyByteBuf (it carries a RegistryAccess for registry-aware codecs).
        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);
        StreamCodec<RegistryFriendlyByteBuf, ClientboundPlayerCombatKillPacket> codec =
            ClientboundPlayerCombatKillPacket.STREAM_CODEC;

        // PLAIN literals only (no style/siblings) — the collapse-to-string domain. Includes
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

        // A handful of representative playerId values exercising the VarInt encoder (1..5 bytes).
        int[] ids = { 0, 1, 127, 128, 255, 300, 2097151, 268435455, -1 };

        int idx = 0;
        for (String text : all) {
            int playerId = ids[idx % ids.length];
            idx++;

            Component message = Component.literal(text);
            ClientboundPlayerCombatKillPacket pkt = new ClientboundPlayerCombatKillPacket(playerId, message);

            RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
            codec.encode(buf, pkt);

            int readable = buf.readableBytes();
            byte[] bytes = new byte[readable];
            buf.duplicate().readBytes(bytes);

            // Round-trip decode through the REAL codec for sanity.
            RegistryFriendlyByteBuf rbuf = new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(bytes), access);
            ClientboundPlayerCombatKillPacket back = codec.decode(rbuf);
            if (back.playerId() != playerId)
                throw new IllegalStateException("playerId round-trip mismatch: " + back.playerId() + " != " + playerId);
            if (!back.message().equals(message))
                throw new IllegalStateException("message round-trip mismatch for text=" + text);

            byte[] textUtf8 = text.getBytes(java.nio.charset.StandardCharsets.UTF_8);
            // ENC <name> <playerId> <msgTextHex> <readableBytes> <hexBytes>
            String name = "id" + playerId + "_len" + textUtf8.length;
            O.printf("ENC\t%s\t%d\t%s\t%d\t%s%n", name, playerId, hex(textUtf8), readable, hex(bytes));
        }
    }
}
