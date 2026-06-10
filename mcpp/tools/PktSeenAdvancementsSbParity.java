// Ground truth for net.minecraft.network.protocol.game.ServerboundSeenAdvancementsPacket.
//
// STREAM_CODEC = Packet.codec(ServerboundSeenAdvancementsPacket::write,
//                             ServerboundSeenAdvancementsPacket::new)
// (ServerboundSeenAdvancementsPacket.java lines 12-14) -> no packet-id prefix, just the body.
//
// write(FriendlyByteBuf output) (lines 40-45):
//   output.writeEnum(this.action);
//   if (this.action == Action.OPENED_TAB) output.writeIdentifier(this.tab);
//
// new ServerboundSeenAdvancementsPacket(FriendlyByteBuf input) (lines 31-38):
//   this.action = input.readEnum(Action.class);
//   this.tab = (action == OPENED_TAB) ? input.readIdentifier() : null;
//
// So the tab is NOT an Optional<ResourceLocation> with a boolean present flag:
// it is written/read ONLY when action == OPENED_TAB, with the action enum acting
// as the discriminator. There is NO leading boolean.
//
//   writeEnum(value)        = writeVarInt(value.ordinal())     (FriendlyByteBuf:471-473)
//   writeIdentifier(id)     = writeUtf(id.toString())          (FriendlyByteBuf:585-588)
//   id.toString()           = namespace + ":" + path           (Identifier:124-126)
//   writeUtf(s)             = VarInt(utf8 byte length) + utf8 bytes
//
// Action declaration order (ServerboundSeenAdvancementsPacket.java 64-67):
//   OPENED_TAB=0, CLOSED_SCREEN=1
//
// Row format (tab separated):
//   ENUM <ordinal> <name>                                  per Action constant (enum gate)
//   ENC  <name> <ordinal> <hasTab> <tabHex> <readableBytes> <hexBytes>
//        tabHex = lowercase hex of the UTF-8 bytes of Identifier.toString() (empty when no tab)
//
// The packet is constructed via the public 2-arg ctor; encode/decode go through the
// REAL STREAM_CODEC. Decode is round-tripped for sanity (action + tab must match).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundSeenAdvancementsPacket;
import net.minecraft.resources.Identifier;

public class PktSeenAdvancementsSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        @SuppressWarnings("unchecked")
        StreamCodec<FriendlyByteBuf, ServerboundSeenAdvancementsPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundSeenAdvancementsPacket>)
                ServerboundSeenAdvancementsPacket.STREAM_CODEC;

        // Enum gate: dump ordinal()+name() for every Action constant in declaration order.
        ServerboundSeenAdvancementsPacket.Action[] actions =
            ServerboundSeenAdvancementsPacket.Action.values();
        for (ServerboundSeenAdvancementsPacket.Action a : actions) {
            O.print("ENUM\t");
            O.print(a.ordinal());
            O.print('\t');
            O.print(a.name());
            O.print('\n');
        }

        // CLOSED_SCREEN: tab is null and NOT written. Wire = single VarInt(ordinal=1).
        emit(CODEC, ServerboundSeenAdvancementsPacket.Action.CLOSED_SCREEN, null);

        // OPENED_TAB: tab written via writeIdentifier(writeUtf(namespace:path)).
        // Cover: default minecraft namespace; custom namespace; full [a-z0-9_.-] + '/'
        // path char set; and a long path forcing the UTF length VarInt across the
        // 127/128 byte boundary (so the VarInt grows from 1 to 2 bytes).
        String[] ids = {
            "minecraft:story/root",
            "minecraft:adventure/root",
            "minecraft:nether/root",
            "minecraft:end/root",
            "minecraft:husbandry/root",
            "minecraft:recipes/root",
            "minecraft:a",
            "custom_pack:my.tab-1/sub_path.2",
            "abc-def_0.9:some/deeper/path-9_8.7",
            // Identifier.toString() length 130 chars -> writeUtf len VarInt = 2 bytes
            // (namespace "n9" + ':' + path of 127 chars from the allowed set).
            "n9:aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"
                + "aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa/0",
        };
        for (String s : ids) {
            Identifier id = Identifier.parse(s);
            if (id == null) throw new IllegalStateException("invalid test id: " + s);
            emit(CODEC, ServerboundSeenAdvancementsPacket.Action.OPENED_TAB, id);
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundSeenAdvancementsPacket> CODEC,
                     ServerboundSeenAdvancementsPacket.Action action,
                     Identifier tab) {
        ServerboundSeenAdvancementsPacket pkt =
            new ServerboundSeenAdvancementsPacket(action, tab);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hexBytes = toHex(buf);

        boolean hasTab = (action == ServerboundSeenAdvancementsPacket.Action.OPENED_TAB);
        String tabStr = (tab == null) ? "" : tab.toString();
        String tabHex = asciiHex(tabStr);
        String name = action.name();

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(action.ordinal());
        O.print('\t');
        O.print(hasTab ? 1 : 0);
        O.print('\t');
        O.print(tabHex);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hexBytes);
        O.print('\n');

        // Round-trip decode (sanity): action ordinal + tab must match.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hexBytes)));
        ServerboundSeenAdvancementsPacket dec = CODEC.decode(rbuf);
        if (dec.getAction() != action)
            throw new IllegalStateException("decode action mismatch for " + name);
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
