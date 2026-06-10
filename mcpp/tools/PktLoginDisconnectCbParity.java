// Ground truth for net.minecraft.network.protocol.login.ClientboundLoginDisconnectPacket.
//
// The packet is a record (Component reason). Its STREAM_CODEC is:
//     StreamCodec.composite(
//        ByteBufCodecs.lenientJson(262144)
//           .apply(ByteBufCodecs.fromCodec(OPS, ComponentSerialization.CODEC)),
//        ClientboundLoginDisconnectPacket::reason,
//        ClientboundLoginDisconnectPacket::new)
//   where OPS = RegistryAccess.EMPTY.createSerializationContext(JsonOps.INSTANCE).
//
// On encode the chain is, verbatim (ByteBufCodecs.fromCodec(DynamicOps,Codec) +
// ByteBufCodecs.lenientJson):
//     JsonElement json = ComponentSerialization.CODEC.encodeStart(OPS, reason)...;
//     String payload  = new GsonBuilder().disableHtmlEscaping().create().toJson(json);
//     Utf8String.write(buf, payload, 262144);    // == FriendlyByteBuf.writeUtf
// i.e. the ON-WIRE form is purely a JSON STRING written via writeUtf
// (VarInt UTF-8 byte length + UTF-8 bytes). No registry-held / NBT / Holder /
// ItemStack data is on the wire — RegistryAccess.EMPTY + JsonOps guarantees the
// JsonElement is plain JSON text. So the C++ side only needs writeUtf(jsonString),
// where jsonString is exactly the canonical string this tool emits (we cannot
// re-derive Gson's canonical JSON in C++, so we transport the EXACT json bytes).
//
// We encode each case through the REAL STREAM_CODEC into a fresh FriendlyByteBuf,
// dump readableBytes() + the raw hex, and decode back through the SAME codec to
// confirm the read side. Emits:
//     ENC \t <name> \t <jsonHex> \t <readableBytes> \t <hex>
// jsonHex is the canonical JSON string (the exact UTF-8 bytes writeUtf consumed)
// emitted as lowercase UTF-8 HEX so it survives the ASCII TSV transport intact.
import com.google.gson.GsonBuilder;
import com.google.gson.JsonElement;
import com.mojang.serialization.JsonOps;
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.ChatFormatting;
import net.minecraft.core.RegistryAccess;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.chat.Component;
import net.minecraft.network.chat.ComponentSerialization;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.login.ClientboundLoginDisconnectPacket;
import net.minecraft.resources.RegistryOps;

public class PktLoginDisconnectCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<ByteBuf, ClientboundLoginDisconnectPacket> codec =
            ClientboundLoginDisconnectPacket.STREAM_CODEC;

        // Reproduce the exact OPS + Gson the codec uses so we can derive the
        // canonical JSON string independently (sanity cross-check only).
        RegistryOps<JsonElement> ops = RegistryAccess.EMPTY.createSerializationContext(JsonOps.INSTANCE);
        com.google.gson.Gson gson = new GsonBuilder().disableHtmlEscaping().create();

        // Finite/physical cases: empty, plain ascii literal, dotted/multiline text,
        // styled (color+bold+italic), translatable, nested siblings, unicode +
        // emoji (exercises multi-byte UTF-8 in the JSON string), a string that
        // forces a 2-byte VarInt length (>127 bytes), and content that exercises
        // Gson's escaping (quotes/backslash/control chars/non-ASCII passthrough).
        Object[][] cases = {
            { "empty",            Component.empty() },
            { "literal_hello",    Component.literal("You have been disconnected") },
            { "literal_quote",    Component.literal("she said \"hi\" \\ end") },
            { "literal_newline",  Component.literal("line1\nline2\tend") },
            { "styled_red_bold",  Component.literal("Banned")
                                     .withStyle(ChatFormatting.RED)
                                     .withStyle(ChatFormatting.BOLD) },
            { "styled_italic",    Component.literal("kicked")
                                     .withStyle(ChatFormatting.ITALIC) },
            { "translatable",     Component.translatable("multiplayer.disconnect.kicked") },
            { "siblings",         Component.literal("Server closed: ")
                                     .append(Component.literal("bye").withStyle(ChatFormatting.YELLOW)) },
            { "unicode",          Component.literal("niño 中文 § done") },
            { "emoji",            Component.literal("server down 😀 sorry") },
            { "long_2byte_len",   Component.literal(repeat("disconnected ", 20)) },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            Component reason = (Component) c[1];

            ClientboundLoginDisconnectPacket pkt = new ClientboundLoginDisconnectPacket(reason);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm read parity.
            ClientboundLoginDisconnectPacket back = codec.decode(buf);
            JsonElement aJson = ComponentSerialization.CODEC.encodeStart(ops, reason)
                .getOrThrow(msg -> new IllegalStateException("encode: " + msg));
            JsonElement bJson = ComponentSerialization.CODEC.encodeStart(ops, back.reason())
                .getOrThrow(msg -> new IllegalStateException("encode-back: " + msg));
            if (!aJson.equals(bJson)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // The canonical JSON string is exactly the bytes writeUtf consumed.
            String json = gson.toJson(aJson);

            // jsonHex = the EXACT UTF-8 bytes of the JSON string (what writeUtf wrote
            // after the VarInt length prefix), as lowercase hex for ASCII-safe transport.
            StringBuilder jsonHex = new StringBuilder();
            for (byte bb : json.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                jsonHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(jsonHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    private static String repeat(String s, int n) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < n; i++) b.append(s);
        return b.toString();
    }
}
