// Ground truth for net.minecraft.network.protocol.game.ClientboundSetDisplayObjectivePacket.
//
// The packet holds (DisplaySlot slot, String objectiveName). Its STREAM_CODEC is
// built via Packet.codec(write, new); the codec body is exactly:
//     output.writeById(DisplaySlot::id, this.slot);   // VarInt(slot.id())
//     output.writeUtf(this.objectiveName);            // VarInt(byteLen)+UTF-8
// and the read side is:
//     this.slot = input.readById(DisplaySlot.BY_ID);  // converter.apply(readVarInt())
//     this.objectiveName = input.readUtf();
//
// (Source: 26.1.2/src/net/minecraft/network/protocol/game/ClientboundSetDisplayObjectivePacket.java)
//
// writeById (FriendlyByteBuf.java:480) = writeVarInt(converter.applyAsInt(value)),
// so the slot is a plain VarInt of DisplaySlot.id() (0..18 — LIST..TEAM_WHITE; see
// net/minecraft/world/scores/DisplaySlot.java). writeUtf -> Utf8String.write:
// VarInt(byteLength) then the UTF-8 bytes, default maxLength 32767. No packet-type
// id is part of the codec bytes (that framing lives outside the StreamCodec).
//
// objectiveName is never null at the wire: the public ctor maps a null Objective to
// the empty string "", and getObjectiveName() only re-nulls "" on the read side. So
// the codec always writes a real (possibly empty) String.
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf and
// dump readableBytes() + the raw hex; we also decode the bytes back through the SAME
// codec and re-emit slot.id()/objectiveName so the C++ side proves read parity too.
//
//   ENC \t <name> \t <slotId> \t <objective_hex> \t <readableBytes> \t <hex>
//
// objectiveName is emitted as UTF-8 LOWERCASE HEX so the exact bytes survive the
// ASCII TSV transport (the runner writes Java stdout as ASCII, which would mangle
// multi-byte UTF-8; byte parity is this gate's whole point). slotId is decimal.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetDisplayObjectivePacket;
import net.minecraft.world.scores.DisplaySlot;
import net.minecraft.world.scores.Objective;
import net.minecraft.world.scores.Scoreboard;
import net.minecraft.world.scores.criteria.ObjectiveCriteria;

public class PktSetDisplayObjectiveParity {
    static final java.io.PrintStream O = System.out;

    static String hexUtf8(String s) {
        StringBuilder sb = new StringBuilder();
        for (byte b : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            sb.append(String.format("%02x", b));
        return sb.toString();
    }

    // Construct a real Objective whose getName() == the requested objectiveName.
    // We build it through reflection to avoid depending on the exact (changing)
    // Objective ctor signature/visibility; getName() returns the field we set.
    static Objective makeObjective(Scoreboard sb, String name) throws Exception {
        for (java.lang.reflect.Constructor<?> ctor : Objective.class.getDeclaredConstructors()) {
            Class<?>[] pt = ctor.getParameterTypes();
            // The Objective ctor is (Scoreboard, String name, ObjectiveCriteria,
            // Component displayName, RenderType, boolean, NumberFormat) in recent
            // versions; match by Scoreboard-first + a String 'name' second.
            if (pt.length >= 2 && pt[0] == Scoreboard.class && pt[1] == String.class) {
                Object[] argv = new Object[pt.length];
                argv[0] = sb;
                argv[1] = name;
                for (int i = 2; i < pt.length; i++) {
                    if (pt[i] == ObjectiveCriteria.class) argv[i] = ObjectiveCriteria.DUMMY;
                    else if (pt[i] == boolean.class) argv[i] = Boolean.FALSE;
                    // The ctor calls createFormattedDisplayName -> displayName.copy(), so a
                    // null Component NPEs. The packet only writes objective.getName() (the
                    // 'name' string), NOT the displayName, so ANY non-null Component yields
                    // identical wire bytes; use Component.literal(name).
                    else if (pt[i] == net.minecraft.network.chat.Component.class)
                        argv[i] = net.minecraft.network.chat.Component.literal(name);
                    else argv[i] = null;
                }
                ctor.setAccessible(true);
                return (Objective) ctor.newInstance(argv);
            }
        }
        throw new IllegalStateException("no matching Objective ctor found");
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundSetDisplayObjectivePacket> codec =
            ClientboundSetDisplayObjectivePacket.STREAM_CODEC;

        Scoreboard sb = new Scoreboard();

        // Finite/physical cases. slot ranges over the full DisplaySlot enum (id 0..18);
        // objectiveName is the Objective.getName() (or "" when the Objective is null).
        // We cover:
        // - null Objective (empty objectiveName) at slot 0
        // - empty + ASCII names across several slots
        // - the highest slot (TEAM_WHITE, id 18)
        // - VarInt length-prefix boundary cross (1->2 byte prefix at >=128 bytes)
        // - ASCII + multi-byte UTF-8 (incl. a surrogate-pair emoji so the VarInt
        //   byte length exceeds the UTF-16 char count)
        StringBuilder p127 = new StringBuilder();
        for (int i = 0; i < 127; i++) p127.append('a');  // 127 bytes -> 1-byte VarInt prefix
        StringBuilder p128 = new StringBuilder();
        for (int i = 0; i < 128; i++) p128.append('b');  // 128 bytes -> 2-byte VarInt prefix
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 300; i++) big.append('x');   // 300 bytes -> 2-byte VarInt prefix

        // Each case: { name, DisplaySlot, objectiveName(null -> uses null Objective) }
        Object[][] cases = {
            { "list_nullobj",      DisplaySlot.LIST,             null },
            { "list_empty",        DisplaySlot.LIST,             "" },
            { "sidebar_obj",       DisplaySlot.SIDEBAR,          "obj" },
            { "below_name_obj",    DisplaySlot.BELOW_NAME,       "deathCount" },
            { "team_black_obj",    DisplaySlot.TEAM_BLACK,       "health" },
            { "team_gold_obj",     DisplaySlot.TEAM_GOLD,        "score" },
            { "team_red_obj",      DisplaySlot.TEAM_RED,         "kills" },
            { "team_white_max",    DisplaySlot.TEAM_WHITE,       "x" },
            { "team_white_null",   DisplaySlot.TEAM_WHITE,       null },
            { "sidebar_unicode",   DisplaySlot.SIDEBAR,          "niño" },
            { "sidebar_cjk",       DisplaySlot.SIDEBAR,          "中文" },
            { "sidebar_emoji",     DisplaySlot.SIDEBAR,          "😀" },   // U+1F600 surrogate pair
            { "sidebar_mixed",     DisplaySlot.SIDEBAR,          "niño中文😀" },
            { "list_127",          DisplaySlot.LIST,             p127.toString() },
            { "list_128",          DisplaySlot.LIST,             p128.toString() },
            { "team_white_300",    DisplaySlot.TEAM_WHITE,       big.toString() },
            { "sidebar_spaces",    DisplaySlot.SIDEBAR,          "obj name" },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            DisplaySlot slot = (DisplaySlot) c[1];
            String objName = (String) c[2];   // null -> null Objective -> wire ""

            Objective objective = (objName == null) ? null : makeObjective(sb, objName);
            ClientboundSetDisplayObjectivePacket pkt =
                new ClientboundSetDisplayObjectivePacket(slot, objective);

            // The wire objectiveName: null Objective => "", else objective.getName().
            String wireName = (objName == null) ? "" : objName;

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundSetDisplayObjectivePacket back = codec.decode(buf);
            boolean slotOk = back.getSlot() == slot;
            // getObjectiveName() returns null for "" — compare to the wire value.
            String backName = back.getObjectiveName() == null ? "" : back.getObjectiveName();
            boolean nameOk = backName.equals(wireName);
            if (!slotOk || !nameOk) {
                throw new IllegalStateException("round-trip mismatch for " + name
                    + " slot=" + slotOk + " name=" + nameOk);
            }

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(slot.id());
            O.print('\t');
            O.print(hexUtf8(wireName));
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
