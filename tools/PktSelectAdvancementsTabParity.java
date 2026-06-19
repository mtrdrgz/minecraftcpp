// Ground truth for net.minecraft.network.protocol.game.ClientboundSelectAdvancementsTabPacket.
//
// STREAM_CODEC = Packet.codec(ClientboundSelectAdvancementsTabPacket::write,
//                             ClientboundSelectAdvancementsTabPacket::new)
// (ClientboundSelectAdvancementsTabPacket.java lines 11-13) -> no packet-id prefix, just the body.
//
// write(FriendlyByteBuf output) (lines 24-26):
//   output.writeNullable(this.tab, FriendlyByteBuf::writeIdentifier);
//
// new ClientboundSelectAdvancementsTabPacket(FriendlyByteBuf input) (lines 20-22):
//   this.tab = input.readNullable(FriendlyByteBuf::readIdentifier);
//
// So the field is a @Nullable Identifier serialized as an Optional with a leading
// boolean present-flag:
//   writeNullable(value, enc):                                 (FriendlyByteBuf:267-274)
//     if value != null { writeBoolean(true);  enc.encode(buf, value); }
//     else             { writeBoolean(false); }
//   readNullable(dec):                                         (FriendlyByteBuf:259-261)
//     return readBoolean() ? dec.decode(buf) : null;
//   writeIdentifier(id) = writeUtf(id.toString())              (FriendlyByteBuf:585-588)
//   readIdentifier()    = Identifier.parse(readUtf(32767))     (FriendlyByteBuf:581-583)
//   id.toString()       = namespace + ":" + path               (Identifier:124-126)
//   writeUtf(s)         = VarInt(utf8 byte length) + utf8 bytes
//   writeBoolean(b)     = single byte 0x00 / 0x01
//
// So the wire is:
//   tab == null     -> 0x00                                    (1 byte)
//   tab != null     -> 0x01 + writeUtf(namespace:path)
//
// Row format (tab separated):
//   ENC <name> <present> <tabHex> <readableBytes> <hexBytes>
//        present = 1 if tab != null else 0
//        tabHex  = lowercase hex of the UTF-8 bytes of Identifier.toString() (empty when null)
//
// The packet is constructed via the public 1-arg ctor; encode/decode go through the
// REAL STREAM_CODEC. Decode is round-tripped for sanity (tab must match).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSelectAdvancementsTabPacket;
import net.minecraft.resources.Identifier;

public class PktSelectAdvancementsTabParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ClientboundSelectAdvancementsTabPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSelectAdvancementsTabPacket>)
                ClientboundSelectAdvancementsTabPacket.STREAM_CODEC;

        // null tab: not present. Wire = single boolean 0x00.
        emit(CODEC, "null", null);

        // Present tab: writeBoolean(true) + writeIdentifier(writeUtf(namespace:path)).
        // Cover: default minecraft namespace tabs; the shortest legal id; custom
        // namespace; the full [a-z0-9_.-] + '/' path char set; and a long id forcing
        // the writeUtf length VarInt across the 127/128 byte boundary (so the inner
        // VarInt grows from 1 to 2 bytes).
        String[] ids = {
            "minecraft:story/root",
            "minecraft:adventure/root",
            "minecraft:nether/root",
            "minecraft:end/root",
            "minecraft:husbandry/root",
            "minecraft:recipes/root",
            "minecraft:a",                       // shortest minecraft-namespaced path
            "a:b",                               // 1-char namespace + 1-char path
            "custom_pack:my.tab-1/sub_path.2",   // custom ns + full punctuation set
            "abc-def_0.9:some/deeper/path-9_8.7",
            // Identifier.toString() length 130 chars -> writeUtf len VarInt = 2 bytes
            // (namespace "n9" + ':' + path of 127 chars from the allowed set).
            "n9:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/0",
        };
        for (String s : ids) {
            Identifier id = Identifier.parse(s);
            if (id == null) throw new IllegalStateException("invalid test id: " + s);
            emit(CODEC, s, id);
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ClientboundSelectAdvancementsTabPacket> CODEC,
                     String name,
                     Identifier tab) {
        ClientboundSelectAdvancementsTabPacket pkt =
            new ClientboundSelectAdvancementsTabPacket(tab);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hexBytes = toHex(buf);

        boolean present = (tab != null);
        String tabStr = (tab == null) ? "" : tab.toString();
        String tabHex = asciiHex(tabStr);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(present ? 1 : 0);
        O.print('\t');
        O.print(tabHex);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hexBytes);
        O.print('\n');

        // Round-trip decode (sanity): tab must match.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hexBytes)));
        ClientboundSelectAdvancementsTabPacket dec = CODEC.decode(rbuf);
        Identifier decTab = dec.getTab();
        boolean tabEq = (tab == null) ? (decTab == null) : tab.equals(decTab);
        if (!tabEq)
            throw new IllegalStateException("decode tab mismatch for " + name
                + ": want=" + tab + " got=" + decTab);
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static String asciiHex(String s) {
        byte[] bytes = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder();
        for (byte x : bytes) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
