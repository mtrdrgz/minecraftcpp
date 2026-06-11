// Ground truth for net.minecraft.network.protocol.configuration.ClientboundUpdateEnabledFeaturesPacket.
//
// The packet is a record wrapping a Set<Identifier>. Its STREAM_CODEC
// (Packet.codec(write, new)) writes ONLY (configuration/
// ClientboundUpdateEnabledFeaturesPacket.java):
//     output.writeCollection(this.features, FriendlyByteBuf::writeIdentifier);
// read: input.readCollection(HashSet::new, FriendlyByteBuf::readIdentifier).
//
// writeCollection (FriendlyByteBuf.java:134-140):
//     writeVarInt(collection.size());
//     for each element: encoder.encode(this, element);
// writeIdentifier (FriendlyByteBuf.java:585-588):
//     writeUtf(identifier.toString());      // VarInt(byteLen)+UTF-8 bytes, maxLen 32767
// Identifier.toString() (Identifier.java:124-126) == namespace + ":" + path,
// ALWAYS including the namespace (even "minecraft:"). So the wire form is:
//     VarInt(count) then count * ( VarInt(byteLen) + UTF-8 bytes of "ns:path" ).
//
// Set ITERATION ORDER is what hits the wire. A HashSet's order is unspecified,
// so for a DETERMINISTIC, byte-exact gate we build the packet from a
// LinkedHashSet (insertion order). The C++ side writes the SAME ordered list,
// so both sides iterate identically. The codec FORMAT (count + each
// writeIdentifier) is exactly what this gate certifies.
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf,
// dump readableBytes() + raw hex, then decode back through the SAME codec and
// verify the resulting Set equals the input Set (order-independent, as a Set).
//
//   ENC <name>\t<count>\t<idsHex>\t<readableBytes>\t<hex>
//
// idsHex: each Identifier's toString() as UTF-8 lowercase hex, '|'-separated
// (ASCII-safe TSV transport; run_groundtruth.ps1 writes ASCII). Empty set -> "".
// The C++ side splits on '|', un-hexes each id back to its exact UTF-8 bytes,
// and replays writeVarInt(count)+writeString(id) in the same order.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.LinkedHashSet;
import java.util.Set;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.configuration.ClientboundUpdateEnabledFeaturesPacket;
import net.minecraft.resources.Identifier;

public class PktUpdateEnabledFeaturesParity {
    static final java.io.PrintStream O = System.out;

    static String utf8Hex(String s) {
        StringBuilder b = new StringBuilder();
        for (byte bb : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            b.append(String.format("%02x", bb));
        return b.toString();
    }

    static Set<Identifier> set(String... ids) {
        // LinkedHashSet preserves insertion order -> deterministic wire bytes.
        LinkedHashSet<Identifier> s = new LinkedHashSet<>();
        for (String id : ids) s.add(Identifier.parse(id));
        return s;
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundUpdateEnabledFeaturesPacket> codec =
            ClientboundUpdateEnabledFeaturesPacket.STREAM_CODEC;

        // A set whose size (130) crosses the 1->2 byte VarInt boundary for the
        // count prefix (>=128). Identifiers minecraft:f0 .. minecraft:f129.
        LinkedHashSet<Identifier> big = new LinkedHashSet<>();
        for (int i = 0; i < 130; i++) big.add(Identifier.parse("minecraft:f" + i));

        Object[][] cases = {
            // empty set (count VarInt 0, no elements)
            { "empty",        set() },
            // the real vanilla feature flag (FeatureFlags.VANILLA == minecraft:vanilla)
            { "vanilla",      set("minecraft:vanilla") },
            // multiple known feature-flag ids, fixed insertion order
            { "two",          set("minecraft:vanilla", "minecraft:bundle") },
            { "three",        set("minecraft:vanilla", "minecraft:trade_rebalance", "minecraft:bundle") },
            // custom (non-minecraft) namespace -> toString keeps the namespace
            { "custom_ns",    set("fabric:custom_feature") },
            // mixed namespaces + a path using the full valid path charset (a-z0-9/._-)
            { "mixed_ns",     set("minecraft:vanilla", "mymod:experimental/feature.v2-test") },
            // count crosses the 1->2 byte VarInt boundary (130 elements)
            { "big130",       big },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            @SuppressWarnings("unchecked")
            Set<Identifier> features = (Set<Identifier>) c[1];

            ClientboundUpdateEnabledFeaturesPacket pkt = new ClientboundUpdateEnabledFeaturesPacket(features);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec; a Set compares
            // order-independently, so HashSet (read) == LinkedHashSet (input).
            ClientboundUpdateEnabledFeaturesPacket back = codec.decode(buf);
            if (!back.features().equals(features)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // Emit each Identifier.toString() as UTF-8 hex, '|'-separated, in the
            // exact iteration order the codec used.
            StringBuilder idsHex = new StringBuilder();
            boolean first = true;
            for (Identifier id : features) {
                if (!first) idsHex.append('|');
                idsHex.append(utf8Hex(id.toString()));
                first = false;
            }

            O.print("ENC\t");
            O.print(name);            O.print('\t');
            O.print(features.size()); O.print('\t');
            O.print(idsHex);          O.print('\t');
            O.print(readable);        O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
