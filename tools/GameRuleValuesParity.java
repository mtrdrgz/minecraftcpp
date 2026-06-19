// Ground truth for net.minecraft.network.protocol.game.ClientboundGameRuleValuesPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL source:
//   record ClientboundGameRuleValuesPacket(Map<ResourceKey<GameRule<?>>, String> values)
//   STREAM_CODEC = ByteBufCodecs.map(
//                      HashMap::new,
//                      ResourceKey.streamCodec(Registries.GAME_RULE),  // key
//                      ByteBufCodecs.STRING_UTF8                       // value, max 32767
//                  ).map(::new, ::values)
//        -> body only, NO packet-id prefix.
//
//   ResourceKey.streamCodec(reg) (ResourceKey.java:21-22):
//        = Identifier.STREAM_CODEC.map(name -> create(reg, name), ResourceKey::identifier)
//   Identifier.STREAM_CODEC (Identifier.java:19):
//        = ByteBufCodecs.STRING_UTF8.map(Identifier::parse, Identifier::toString)
//        -> encode writes Utf8String.write( identifier.toString() == "namespace:path" ).
//
//   ByteBufCodecs.map.encode (ByteBufCodecs.java:460-466):
//        writeCount(output, map.size(), maxSize);   // == VarInt.write(size)  (plain LEB128)
//        map.forEach((k, v) -> { keyCodec.encode(k); valueCodec.encode(v); });
//   Utf8String.write(output, value, n) (Utf8String.java:35-55):
//        VarInt.write(output, utf8ByteLen);  output.writeBytes(utf8Bytes);   == writeUtf
//
//   So the wire is exactly:
//        VarInt(size)
//        for each entry in MAP ITERATION ORDER:
//            VarInt(utf8ByteLen(keyIdentifierString))   key UTF-8 bytes   ("namespace:path")
//            VarInt(utf8ByteLen(value))                 value UTF-8 bytes
//
//   We construct the packet with a LinkedHashMap so map.forEach() (and our TSV dump)
//   both follow INSERTION ORDER deterministically; the encoded byte order then equals
//   the TSV entry order, which the C++ side replays through mc::net::PacketBuffer.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <size> <k0HexUtf8> <v0HexUtf8> ... <k{n-1}HexUtf8> <v{n-1}HexUtf8> <bytesN> <hexBytes>
// Keys/values are carried as raw-UTF-8 hex (lowercase). An empty string is impossible to
// carry as an empty TSV field, so empty strings are emitted as the literal token "_"
// (the C++ side maps "_" -> ""). Game-rule key identifiers are never empty.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import java.util.LinkedHashMap;
import java.util.Map;

import net.minecraft.SharedConstants;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.RegistryFriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundGameRuleValuesPacket;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.gamerules.GameRule;

@SuppressWarnings({"unchecked", "deprecation"})
public class GameRuleValuesParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        int caseNo = 0;

        // empty map -> just VarInt(0)
        emit("empty" + (caseNo++), ordered());

        // single entry, default namespace key
        emit("single" + (caseNo++), ordered("minecraft:do_daylight_cycle", "true"));

        // a few real game-rule-like keys, insertion order preserved by LinkedHashMap
        emit("trio" + (caseNo++), ordered(
            "minecraft:keep_inventory", "false",
            "minecraft:random_tick_speed", "3",
            "minecraft:max_entity_cramming", "24"));

        // empty value (legal: VarInt(0) length prefix, no bytes); key never empty
        emit("emptyVal" + (caseNo++), ordered("minecraft:reduced_debug_info", ""));

        // non-default namespace + value crossing the 127-byte VarInt boundary
        emit("len128" + (caseNo++), ordered("modid:custom_rule", repeat('x', 128)));
        emit("len300" + (caseNo++), ordered("foo:bar_baz", repeat('z', 300)));

        // multi-byte UTF-8 in the VALUE: byteLen != charLen.
        //   "é"  U+00E9  -> 2 bytes (c3 a9)
        //   "€"  U+20AC  -> 3 bytes (e2 82 ac)
        //   "𝄞"  U+1D11E -> 4 bytes (f0 9d 84 9e), 2 UTF-16 units
        emit("utf8val" + (caseNo++), ordered("minecraft:spawn_radius", "abc𝄞 naïve €café"));

        // all 0x00..0x7f ASCII bytes as a value (exercises every single-byte code unit)
        StringBuilder ascii = new StringBuilder();
        for (int i = 0; i < 0x80; i++) ascii.append((char) i);
        emit("asciiVal" + (caseNo++), ordered("minecraft:ascii_rule", ascii.toString()));

        // many entries (size VarInt still 1 byte; exercises the count loop)
        LinkedHashMap<String, String> big = new LinkedHashMap<>();
        for (int i = 0; i < 40; i++) big.put("minecraft:rule_" + i, "val-" + (i * 7));
        emit("many40" + (caseNo++), big);
    }

    static String repeat(char c, int n) {
        StringBuilder sb = new StringBuilder(n);
        for (int i = 0; i < n; i++) sb.append(c);
        return sb.toString();
    }

    // Build a LinkedHashMap<keyIdentifierString,value> preserving the given k,v,k,v,...
    // insertion order. The TSV carries the key as its identifier STRING; both the GT
    // encoder and the C++ side encode that exact string via writeUtf.
    static LinkedHashMap<String, String> ordered(String... kv) {
        LinkedHashMap<String, String> m = new LinkedHashMap<>();
        for (int i = 0; i + 1 < kv.length; i += 2) m.put(kv[i], kv[i + 1]);
        return m;
    }

    static String hexUtf8(String s) {
        if (s.isEmpty()) return "_";
        byte[] b = s.getBytes(java.nio.charset.StandardCharsets.UTF_8);
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte by : b) sb.append(String.format("%02x", by));
        return sb.toString();
    }

    static void emit(String name, Map<String, String> keyStringToValue) {
        // Construct the REAL packet's Map<ResourceKey<GameRule<?>>, String> from our
        // identifier strings. ResourceKey.create just interns a key; no registry lookup,
        // and the encoder only reads key.identifier().toString().
        LinkedHashMap<ResourceKey<GameRule<?>>, String> values = new LinkedHashMap<>();
        for (Map.Entry<String, String> e : keyStringToValue.entrySet()) {
            ResourceKey<GameRule<?>> key =
                (ResourceKey<GameRule<?>>) (ResourceKey<?>)
                ResourceKey.create(Registries.GAME_RULE, Identifier.parse(e.getKey()));
            values.put(key, e.getValue());
        }

        ClientboundGameRuleValuesPacket pkt = new ClientboundGameRuleValuesPacket(values);

        // STREAM_CODEC is StreamCodec<FriendlyByteBuf,...>; a RegistryFriendlyByteBuf is a
        // FriendlyByteBuf and needs no registry access for this packet (Identifier+String).
        RegistryFriendlyByteBuf buf =
            new RegistryFriendlyByteBuf(Unpooled.buffer(), RegistryAccess.EMPTY);
        ClientboundGameRuleValuesPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): the map must survive byte-for-byte.
        RegistryFriendlyByteBuf rb =
            new RegistryFriendlyByteBuf(Unpooled.copiedBuffer(buf), RegistryAccess.EMPTY);
        ClientboundGameRuleValuesPacket back =
            ClientboundGameRuleValuesPacket.STREAM_CODEC.decode(rb);
        if (!back.values().equals(values)) {
            throw new IllegalStateException("round-trip map mismatch for " + name + ": "
                + back.values() + " != " + values);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(keyStringToValue.size());
        // entries in MAP ITERATION ORDER (== insertion order for our LinkedHashMaps).
        for (Map.Entry<String, String> e : keyStringToValue.entrySet()) {
            O.print('\t');
            O.print(hexUtf8(e.getKey()));   // key identifier STRING, raw-UTF-8 hex
            O.print('\t');
            O.print(hexUtf8(e.getValue()));
        }
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
