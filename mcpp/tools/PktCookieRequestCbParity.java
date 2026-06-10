// Ground truth for net.minecraft.network.protocol.cookie.ClientboundCookieRequestPacket.
//
// The packet is a record (Identifier key). Its STREAM_CODEC (built via
// Packet.codec(write, new)) writes ONLY:
//     output.writeIdentifier(this.key);
// and reads back input.readIdentifier(). No packet-type id is part of the codec
// bytes (that framing lives outside the StreamCodec).
//
// Source (26.1.2/src/net/minecraft/network/protocol/cookie/ClientboundCookieRequestPacket.java):
//     private void write(final FriendlyByteBuf output) { output.writeIdentifier(this.key); }
//     private ClientboundCookieRequestPacket(final FriendlyByteBuf input) { this(input.readIdentifier()); }
//
// FriendlyByteBuf.writeIdentifier (26.1.2/src/.../FriendlyByteBuf.java:585):
//     public FriendlyByteBuf writeIdentifier(final Identifier identifier) {
//         this.writeUtf(identifier.toString());
//         return this;
//     }
//   readIdentifier(): return Identifier.parse(this.readUtf(32767));
// and Identifier.toString() (Identifier.java:124) = namespace + ":" + path.
//
// So the ENTIRE codec payload is a single writeUtf of the string "namespace:path":
//   Utf8String.write -> VarInt(UTF-8 byteLength) then the UTF-8 bytes; default
//   maxLength is 32767 (FriendlyByteBuf.MAX_STRING_LENGTH).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and re-emit key.toString() so the C++ side proves read parity.
//
//   ENC <name>\t<key.toString() as UTF-8 HEX>\t<readableBytes>\t<hex>
//
// The key column is emitted as LOWERCASE UTF-8 HEX (run_groundtruth.ps1 captures
// Java stdout and writes the TSV as ASCII, which would mangle raw multi-byte
// UTF-8). Identifier strings are constrained to [a-z0-9_.-] namespace and
// [a-z0-9/._-] path, so they are pure ASCII here, but we keep the hex transport
// for uniformity with the other packet gates and for the VarInt-boundary cases.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.cookie.ClientboundCookieRequestPacket;
import net.minecraft.resources.Identifier;

public class PktCookieRequestCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundCookieRequestPacket> codec =
            ClientboundCookieRequestPacket.STREAM_CODEC;

        // Finite/physical cases. Identifiers carry namespace:path. Cover:
        //  - the implicit default namespace ("minecraft") materialised by parse,
        //  - explicit minecraft namespace, custom namespaces, slashed paths,
        //  - all the [a-z0-9_.-] / [a-z0-9/._-] character classes,
        //  - VarInt length-prefix boundary at 127 vs 128 UTF-8 bytes.
        //
        // Each entry is built via Identifier.parse so what we encode is exactly an
        // Identifier (validated), and the wire bytes are writeUtf(key.toString()).
        //
        // For the boundary cases we craft an explicit "minecraft:<padding>" whose
        // toString() length is exactly 127 / 128 bytes.
        StringBuilder pad127 = new StringBuilder("minecraft:"); // 10 bytes prefix
        while (pad127.length() < 127) pad127.append('a');       // pad path to total 127
        StringBuilder pad128 = new StringBuilder("minecraft:");
        while (pad128.length() < 128) pad128.append('a');       // total 128

        String[][] cases = {
            { "default_ns_path",  "stone" },                       // -> minecraft:stone
            { "mc_stone",         "minecraft:stone" },
            { "mc_dirt",          "minecraft:dirt" },
            { "custom_ns",        "mymod:cookie" },
            { "slashed_path",     "minecraft:blocks/stone" },
            { "deep_path",        "mymod:foo/bar/baz" },
            { "all_path_chars",   "minecraft:a-b_c.d/e0/9" },
            { "ns_with_dot_dash", "my.mod-2:thing_1" },
            { "single_char",      "a:b" },
            { "digits",           "mod1:item9" },
            { "len127",           pad127.toString() },             // 127 bytes -> 1-byte VarInt prefix
            { "len128",           pad128.toString() },             // 128 bytes -> 2-byte VarInt prefix
        };

        for (String[] c : cases) {
            String name = c[0];
            Identifier key = Identifier.parse(c[1]);

            ClientboundCookieRequestPacket pkt = new ClientboundCookieRequestPacket(key);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundCookieRequestPacket back = codec.decode(buf);
            if (!back.key().equals(key)) {
                throw new IllegalStateException("round-trip mismatch for " + name
                    + ": " + back.key() + " != " + key);
            }

            // Emit key.toString() (the exact String the codec writeUtf's) as UTF-8 HEX.
            StringBuilder keyHex = new StringBuilder();
            for (byte bb : key.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8))
                keyHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(keyHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
