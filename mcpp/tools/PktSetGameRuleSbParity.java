// Ground truth for net.minecraft.network.protocol.game.ServerboundSetGameRulePacket.
//
// The packet is a record (List<Entry> entries) where
//   Entry(ResourceKey<GameRule<?>> gameRuleKey, String value).
// Its real STREAM_CODEC, VERBATIM (ServerboundSetGameRulePacket.java:15-17):
//     STREAM_CODEC = StreamCodec.composite(
//         ServerboundSetGameRulePacket.Entry.STREAM_CODEC.apply(ByteBufCodecs.list()),
//         ServerboundSetGameRulePacket::entries,
//         ServerboundSetGameRulePacket::new);
// and Entry.STREAM_CODEC, VERBATIM (ServerboundSetGameRulePacket.java:33-40):
//     STREAM_CODEC = StreamCodec.composite(
//         ResourceKey.streamCodec(Registries.GAME_RULE), Entry::gameRuleKey,
//         ByteBufCodecs.STRING_UTF8,                      Entry::value,
//         Entry::new);
//
// Wire format, field-by-field in codec order:
//   ByteBufCodecs.list()  -> writeCount(size) = VarInt(size)            (ByteBufCodecs.java:399-405)
//   per Entry:
//     ResourceKey.streamCodec(GAME_RULE) = Identifier.STREAM_CODEC      (ResourceKey.java:20-22)
//        = ByteBufCodecs.STRING_UTF8.map(parse, Identifier::toString)   (Identifier.java:19)
//        -> writeUtf(gameRuleKey.identifier().toString())               // "namespace:path"
//     ByteBufCodecs.STRING_UTF8 -> writeUtf(value)
//   writeUtf == Utf8String.write: VarInt(byteLength) + UTF-8 bytes, max 32767
//                                                                       (Utf8String.java:35-55)
//
// No Holder / registry-id / ItemStack / Component / NBT is on the wire: the
// "registry id" of a ResourceKey is implicit (only the Identifier location is
// written), so every field is a primitive (VarInt count + UTF-8 strings). The
// certified PacketBuffer (the FriendlyByteBuf port) rebuilds the body directly:
//   writeVarInt(count) + per entry [ writeString(keyId) + writeString(value) ].
//
// The codec's buffer type is the bare ByteBuf (StreamCodec<ByteBuf, ...>), so the
// GT encodes/decodes through a plain FriendlyByteBuf (a ByteBuf) — no
// RegistryFriendlyByteBuf needed.
//
// Row format (tab separated), TAG = ENC. Strings are hex-encoded UTF-8 so the
// ascii TSV transport (run_groundtruth.ps1) survives multi-byte bytes intact:
//   ENC <name> <count> <keyId0hex>:<value0hex>,<keyId1hex>:<value1hex>,... <readableBytes> <hex>
// where the entries field is a comma-separated list of "keyHex:valueHex" pairs
// (empty string => empty field; a list with zero entries emits a single literal
// "-" so the column is never blank).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.SharedConstants;
import net.minecraft.core.registries.Registries;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.server.Bootstrap;
import net.minecraft.network.protocol.game.ServerboundSetGameRulePacket;
import net.minecraft.world.level.gamerules.GameRule;

public class PktSetGameRuleSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        StreamCodec<ByteBuf, ServerboundSetGameRulePacket> codec =
            ServerboundSetGameRulePacket.STREAM_CODEC;

        // A 64-byte filler value to push a value's UTF length prefix across the
        // 1->2 byte VarInt boundary, and a long key path likewise.
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 200; i++) big.append('v');     // 200 bytes -> 2-byte VarInt prefix
        String bigVal = big.toString();
        String longPath = "a_very_long_gamerule_identifier_name_that_exceeds_one_hundred_and_twenty_eight_utf8_bytes_zzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzzz";

        // Cases: empty list; single entries spanning real vanilla gamerule keys,
        // namespaced keys, numeric/bool/empty values, unicode values (multi-byte
        // UTF-8 incl. a surrogate-pair emoji), VarInt-boundary value lengths; and
        // multi-entry lists that cross the list-count and per-string boundaries.
        List<List<String[]>> cases = new ArrayList<>();
        String[] names = {
            "empty", "single_bool", "single_int", "namespaced", "empty_value",
            "unicode_value", "emoji_value", "len127", "len128",
            "len200", "long_key", "two", "three", "many",
        };

        cases.add(list());                                                        // empty
        cases.add(list(e("minecraft:do_daylight_cycle", "true")));                // single_bool
        cases.add(list(e("minecraft:random_tick_speed", "3")));                   // single_int
        cases.add(list(e("mymod:custom_rule", "value")));                         // namespaced
        cases.add(list(e("minecraft:do_fire_tick", "")));                         // empty_value
        cases.add(list(e("minecraft:announce_advancements", "niño 中文")));        // unicode_value (in value)
        cases.add(list(e("minecraft:do_mob_spawning", "ok 😀")));                  // emoji_value (U+1F600 in value)
        cases.add(list(e("minecraft:keep_inventory", repeat('a', 127))));         // len127 -> 1-byte prefix
        cases.add(list(e("minecraft:keep_inventory", repeat('a', 128))));         // len128 -> 2-byte prefix
        cases.add(list(e("minecraft:keep_inventory", bigVal)));                   // len200 -> 2-byte prefix
        cases.add(list(e("minecraft:" + longPath, "1")));                         // long_key (>128-byte key)
        cases.add(list(e("minecraft:mob_griefing", "false"),
                       e("minecraft:do_weather_cycle", "true")));                 // two
        cases.add(list(e("minecraft:spawn_radius", "10"),
                       e("minecraft:max_entity_cramming", "24"),
                       e("minecraft:fall_damage", "true")));                      // three
        List<String[]> many = new ArrayList<>();
        for (int i = 0; i < 200; i++) many.add(e("mymod:rule_" + i, "v" + i));    // count crosses 1->2 byte VarInt
        cases.add(many);

        for (int ci = 0; ci < cases.size(); ci++) {
            String name = names[ci];
            List<String[]> raw = cases.get(ci);

            List<ServerboundSetGameRulePacket.Entry> entries = new ArrayList<>();
            for (String[] pair : raw) {
                ResourceKey<GameRule<?>> key = ResourceKey.create(Registries.GAME_RULE, Identifier.parse(pair[0]));
                entries.add(new ServerboundSetGameRulePacket.Entry(key, pair[1]));
            }
            ServerboundSetGameRulePacket pkt = new ServerboundSetGameRulePacket(entries);

            // Encode through the REAL STREAM_CODEC into a plain FriendlyByteBuf (a ByteBuf).
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int n = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte() & 0xff));

            // Round-trip decode through the SAME codec and assert field equality.
            ServerboundSetGameRulePacket back = codec.decode(buf);
            if (back.entries().size() != entries.size()) {
                throw new IllegalStateException("round-trip size mismatch for " + name);
            }
            for (int i = 0; i < entries.size(); i++) {
                ServerboundSetGameRulePacket.Entry a = entries.get(i);
                ServerboundSetGameRulePacket.Entry b = back.entries().get(i);
                if (!a.gameRuleKey().identifier().toString().equals(b.gameRuleKey().identifier().toString())
                    || !a.value().equals(b.value())) {
                    throw new IllegalStateException("round-trip entry mismatch for " + name + " idx " + i);
                }
            }

            // entries column: comma-separated "keyHex:valueHex"; "-" if the list is empty.
            StringBuilder ent = new StringBuilder();
            if (raw.isEmpty()) {
                ent.append('-');
            } else {
                for (int i = 0; i < raw.size(); i++) {
                    if (i > 0) ent.append(',');
                    ent.append(toHex(raw.get(i)[0])).append(':').append(toHex(raw.get(i)[1]));
                }
            }

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(raw.size());
            O.print('\t');
            O.print(ent);
            O.print('\t');
            O.print(n);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }

    static String[] e(String key, String value) { return new String[]{ key, value }; }

    @SafeVarargs
    static List<String[]> list(String[]... es) {
        List<String[]> l = new ArrayList<>();
        for (String[] x : es) l.add(x);
        return l;
    }

    static String repeat(char c, int n) {
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < n; i++) b.append(c);
        return b.toString();
    }

    static String toHex(String s) {
        StringBuilder b = new StringBuilder();
        for (byte bb : s.getBytes(StandardCharsets.UTF_8)) b.append(String.format("%02x", bb & 0xff));
        return b.toString();
    }
}
