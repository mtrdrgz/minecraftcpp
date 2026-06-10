// Ground truth for net.minecraft.network.protocol.game.ClientboundServerDataPacket's
// StreamCodec. Verbatim from 26.1.2/src ClientboundServerDataPacket.java:
//
//   public record ClientboundServerDataPacket(Component motd, Optional<byte[]> iconBytes)
//   STREAM_CODEC = StreamCodec.composite(
//       ComponentSerialization.TRUSTED_CONTEXT_FREE_STREAM_CODEC, ::motd,
//       ByteBufCodecs.BYTE_ARRAY.apply(ByteBufCodecs::optional),  ::iconBytes,
//       ::new);
//
// Wire (codec field order):
//   1) motd  : TRUSTED_CONTEXT_FREE_STREAM_CODEC = fromCodecTrusted(CODEC)
//              (ByteBufCodecs.java:322-323 -> fromCodec, line 326+, NbtOps + tagCodec).
//              For a PLAIN literal (no style/siblings) ComponentSerialization.CODEC
//              collapses to a bare StringTag, so the wire is a root StringTag emitted
//              by FriendlyByteBuf.writeNbt(Tag) -> NbtIo.writeAnyTag: 08 <u16 MUTF8 len>
//              <bytes>.  (The registries-context variant TRUSTED_STREAM_CODEC produces
//              IDENTICAL bytes for a plain literal — verified by component_nbt_parity.)
//   2) iconBytes : Optional<byte[]> = optional(BYTE_ARRAY)
//              (ByteBufCodecs.java optional() 373-388):
//                writeBoolean(present);
//                if present: BYTE_ARRAY.encode = FriendlyByteBuf.writeByteArray
//                            (FriendlyByteBuf.java:289-292) = VarInt(len) + raw bytes.
//
// This tool ENCODES the REAL packet through its STREAM_CODEC and dumps the exact bytes
// so the C++ side (PacketBuffer.writeNbt(NbtTag::string_) + writeBool + writeVarInt +
// writeBytes) can byte-match.  Only PLAIN-TEXT literal Components are fed (the
// collapse-to-StringTag domain); styled/translatable Components use the CompoundTag form
// which is not yet ported and is intentionally not exercised here.
//
// Rows (tab separated):
//   ENC <name> <motdTextHex(UTF-8)> <hasIcon> <iconHex> <readableBytes> <hexBytes>
//     motdTextHex : lowercase hex of the literal's UTF-8 bytes ("" -> "-")
//     hasIcon     : 0/1 (Optional present)
//     iconHex     : lowercase hex of the raw icon bytes ("" / absent -> "-")
//
// The packet codec is StreamCodec<ByteBuf, ...>, but we build a RegistryFriendlyByteBuf
// (RegistryAccess.fromRegistryOfRegistries) so the codec runs in the canonical ctx; the
// composite tolerates the wider buffer type.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.Optional;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundServerDataPacket;

public class PktServerDataParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] b) {
        if (b == null || b.length == 0) return "-";
        StringBuilder s = new StringBuilder();
        for (byte x : b) s.append(String.format("%02x", x & 0xff));
        return s.toString();
    }

    static String toHex(ByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.length() == 0 ? "-" : sb.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        RegistryAccess access = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        @SuppressWarnings("unchecked")
        StreamCodec<ByteBuf, ClientboundServerDataPacket> CODEC =
            (StreamCodec<ByteBuf, ClientboundServerDataPacket>)
                ClientboundServerDataPacket.STREAM_CODEC;

        // MOTD literals: plain text only (collapse-to-StringTag domain). Empty, ASCII,
        // spaces, command-like, multibyte UTF-8, surrogate-pair emoji, mixed, control
        // chars, and a long string crossing the VarInt/MUTF8 length boundary.
        String[] motds = {
            "", "A Minecraft Server", "hi", "a b c", "/say x",
            "niño", "中文", "😀", "niño中文😀",
            "line1\nline2", "tab\there",
        };
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 300; i++) big.append('x');
        java.util.List<String> motdList = new java.util.ArrayList<>(java.util.Arrays.asList(motds));
        motdList.add(big.toString());

        // Icon byte payloads, including the Optional.empty() branch (hasIcon=0).
        // A real server icon is a PNG byte[]; we exercise: absent, empty present,
        // tiny binary, full 0..255 byte range, and a >127-byte array (2-byte VarInt len).
        byte[] iconAbsent = null;          // -> Optional.empty()
        byte[] iconEmpty  = new byte[0];
        byte[] iconTiny   = new byte[]{(byte)0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a};
        byte[] iconAll256 = new byte[256];
        for (int i = 0; i < 256; i++) iconAll256[i] = (byte) i;
        byte[] iconBig = new byte[200];
        for (int i = 0; i < iconBig.length; i++) iconBig[i] = (byte)(i * 7 + 3);

        byte[][] icons = { iconAbsent, iconEmpty, iconTiny, iconAll256, iconBig };
        String[] iconNames = { "noicon", "emptyicon", "tinyicon", "all256", "bigicon" };

        // Cross every motd with every icon variant.
        for (int mi = 0; mi < motdList.size(); mi++) {
            String motd = motdList.get(mi);
            for (int ii = 0; ii < icons.length; ii++) {
                emit(CODEC, access, "m" + mi + "_" + iconNames[ii], motd, icons[ii]);
            }
        }
    }

    static void emit(StreamCodec<ByteBuf, ClientboundServerDataPacket> CODEC,
                     RegistryAccess access, String name, String motdText, byte[] icon)
            throws Exception {
        Component motd = Component.literal(motdText);
        Optional<byte[]> iconOpt = Optional.ofNullable(icon);
        ClientboundServerDataPacket pkt = new ClientboundServerDataPacket(motd, iconOpt);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), access);
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hexBytes = toHex(buf);

        // Round-trip decode sanity: rebuild from the wire and confirm fields match.
        RegistryFriendlyByteBuf rbuf =
            new RegistryFriendlyByteBuf(buf.duplicate(), access);
        ClientboundServerDataPacket dec = CODEC.decode(rbuf);
        boolean ok = dec.iconBytes().isPresent() == iconOpt.isPresent();
        if (ok && iconOpt.isPresent())
            ok = java.util.Arrays.equals(dec.iconBytes().get(), icon == null ? new byte[0] : icon);
        // motd round-trips to a plain literal whose collapsed string equals motdText.
        if (ok) {
            String decText = dec.motd().getString();
            ok = decText.equals(motdText);
        }
        if (!ok) throw new IllegalStateException("round-trip mismatch for " + name);

        int hasIcon = iconOpt.isPresent() ? 1 : 0;
        String iconHex = iconOpt.isPresent()
            ? hex(icon == null ? new byte[0] : icon) : "-";
        byte[] motdUtf8 = motdText.getBytes(java.nio.charset.StandardCharsets.UTF_8);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(hex(motdUtf8));   // "" -> "-"
        O.print('\t');
        O.print(hasIcon);
        O.print('\t');
        O.print(iconHex);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hexBytes);
        O.print('\n');
    }
}
