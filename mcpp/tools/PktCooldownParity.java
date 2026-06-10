// Ground truth for net.minecraft.network.protocol.game.ClientboundCooldownPacket.
//
// The packet is a record (Identifier cooldownGroup, int duration). Its
// STREAM_CODEC (StreamCodec.composite) is, VERBATIM
// (ClientboundCooldownPacket.java:11-17):
//     Identifier.STREAM_CODEC,      ClientboundCooldownPacket::cooldownGroup,
//     ByteBufCodecs.VAR_INT,        ClientboundCooldownPacket::duration,
//     ClientboundCooldownPacket::new
//
// Field-by-field wire encoding (in codec order):
//   (1) Identifier.STREAM_CODEC = ByteBufCodecs.STRING_UTF8.map(Identifier::parse,
//       Identifier::toString)  (Identifier.java:19). On encode it writes
//       Identifier.toString() (== "namespace:path") via STRING_UTF8 = stringUtf8(32767)
//       (ByteBufCodecs.java:168) -> Utf8String.write: VarInt(UTF-8 byte length) then
//       the UTF-8 bytes, maxLength 32767. This is exactly FriendlyByteBuf.writeUtf,
//       i.e. mc::net::PacketBuffer::writeString.
//   (2) ByteBufCodecs.VAR_INT (ByteBufCodecs.java:102-110) = VarInt.write(out, value)
//       -> LEB128 VarInt, i.e. mc::net::PacketBuffer::writeVarInt.
//
// No registry-held type / ItemStack / Component / Holder is on the wire (the
// Identifier travels purely as its toString() text), so the C++ PacketBuffer can
// rebuild the body from just (utf8-of-toString, duration-int).
//
// We construct the REAL packet via its public canonical record constructor and
// encode through the REAL STREAM_CODEC into a fresh RegistryFriendlyByteBuf (the
// codec's left type), dump readableBytes() + every byte %02x, then round-trip
// decode through the SAME codec (sanity).
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <cooldownGroup-toString-as-UTF8-HEX> <duration-dec> <readableBytes> <hex>
// cooldownGroup is emitted as the lowercase UTF-8 HEX of Identifier.toString() so the
// exact bytes survive the ASCII TSV transport (the C++ side decodes that hex back to
// the byte string before writeUtf). duration is a signed 32-bit decimal.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundCooldownPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.server.Bootstrap;

public class PktCooldownParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // A frozen RegistryAccess so RegistryFriendlyByteBuf is constructible; the
        // ClientboundCooldownPacket codec never touches any registry on the wire.
        RegistryAccess registryAccess = RegistryAccess.fromRegistryOfRegistries(BuiltInRegistries.REGISTRY);

        // Identifier battery: default-namespace (minecraft), custom namespaces,
        // dotted/underscored/dashed/numeric path chars (the full ALLOWED set), and a
        // long path so the toString crosses the 1->2 byte VarInt length-prefix boundary.
        // toString() is always "namespace:path"; parse() of a path-only string yields
        // the minecraft namespace (Identifier.bySeparator), so the wire text differs
        // from the input. We construct explicitly with parse() to mirror the codec's
        // map(Identifier::parse, ...) decode side and assert round-trip below.
        StringBuilder longPath = new StringBuilder();
        for (int i = 0; i < 200; i++) longPath.append('a'); // 200 chars -> 2-byte VarInt prefix on "minecraft:" + path

        Object[][] idCases = {
            { "bare",        "stone" },                  // -> minecraft:stone
            { "default_ns",  "minecraft:stone" },        // -> minecraft:stone
            { "custom_ns",   "mymod:enderpearl" },
            { "dotted",      "minecraft:item.cool.down" },
            { "underscore",  "minecraft:cooldown_group_1" },
            { "dashed",      "my-mod:some-path" },
            { "numeric",     "mod123:path456" },
            { "slash_path",  "minecraft:foo/bar/baz" },  // '/' is a valid path char
            { "realms",      "realms:cooldown" },
            { "long_path",   "minecraft:" + longPath },  // crosses 1->2 byte prefix
        };

        // duration battery: VarInt 1->2->3->4->5 byte boundaries, signs and extremes.
        // VAR_INT writes the raw 32-bit value as LEB128 (negatives -> 5 bytes).
        int[] durations = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };

        // (A) Sweep identifiers with a fixed small duration.
        for (Object[] ic : idCases) {
            emit(registryAccess, "id_" + ic[0], (String) ic[1], 20);
        }

        // (B) Sweep durations with a fixed simple identifier.
        for (int d : durations) {
            emit(registryAccess, "dur_" + d, "minecraft:stone", d);
        }

        // (C) A few fully-mixed cases.
        emit(registryAccess, "mix_max",  "mymod:big_cooldown", Integer.MAX_VALUE);
        emit(registryAccess, "mix_min",  "my-mod:neg",         Integer.MIN_VALUE);
        emit(registryAccess, "mix_long", "minecraft:" + longPath, 2097152);
    }

    @SuppressWarnings("deprecation")
    static void emit(RegistryAccess registryAccess, String name, String idText, int duration) {
        // Build via the codec's own decode-side parse so the wire text == toString().
        Identifier cooldownGroup = Identifier.parse(idText);

        ClientboundCooldownPacket pkt = new ClientboundCooldownPacket(cooldownGroup, duration);

        RegistryFriendlyByteBuf buf = new RegistryFriendlyByteBuf(Unpooled.buffer(), registryAccess);
        ClientboundCooldownPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        RegistryFriendlyByteBuf rbuf =
            new RegistryFriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())), registryAccess);
        ClientboundCooldownPacket back = ClientboundCooldownPacket.STREAM_CODEC.decode(rbuf);
        if (!back.cooldownGroup().equals(cooldownGroup) || back.duration() != duration) {
            throw new IllegalStateException("round-trip mismatch for " + name
                + " group=" + cooldownGroup + " dur=" + duration
                + " -> group=" + back.cooldownGroup() + " dur=" + back.duration());
        }

        // Emit the cooldownGroup as the UTF-8 HEX of its toString() (the exact bytes
        // the codec writes) so multi-byte text survives the ASCII TSV transport.
        String wireText = cooldownGroup.toString();
        StringBuilder idHex = new StringBuilder();
        for (byte bb : wireText.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            idHex.append(String.format("%02x", bb));

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(idHex);     // UTF-8 hex of Identifier.toString()
        O.print('\t');
        O.print(duration);  // signed 32-bit decimal
        O.print('\t');
        O.print(n);         // readableBytes
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
