// Ground truth for net.minecraft.network.protocol.game.ClientboundResetScorePacket.
//
// The packet is a record (String owner, @Nullable String objectiveName). Its
// STREAM_CODEC is built via Packet.codec(write, new); the codec body is exactly:
//     output.writeUtf(this.owner);                              // VarInt(byteLen)+UTF-8
//     output.writeNullable(this.objectiveName, FriendlyByteBuf::writeUtf);
// and the read side is:
//     this(input.readUtf(), input.readNullable(FriendlyByteBuf::readUtf));
//
// writeNullable (FriendlyByteBuf.java:267) encodes: writeBoolean(present); if
// present, the value encoder runs -> writeUtf. So objectiveName is one boolean
// (0x00 absent / 0x01 present) optionally followed by a writeUtf string. No
// packet-type id is part of the codec bytes (that framing lives outside the
// StreamCodec).
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ClientboundResetScorePacket.java):
//     private void write(final FriendlyByteBuf output) {
//        output.writeUtf(this.owner);
//        output.writeNullable(this.objectiveName, FriendlyByteBuf::writeUtf);
//     }
//     private ClientboundResetScorePacket(final FriendlyByteBuf input) {
//        this(input.readUtf(), input.readNullable(FriendlyByteBuf::readUtf));
//     }
//
// writeUtf -> Utf8String.write: VarInt(byteLength) then the UTF-8 bytes; the
// default maxLength is 32767 (FriendlyByteBuf.MAX_STRING_LENGTH).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and re-emit owner/objectiveName so the C++ side proves read
// parity too.
//
//   ENC \t <name> \t <owner_hex> \t <hasObjective:0|1> \t <objective_hex> \t <readableBytes> \t <hex>
//
// owner and objectiveName are emitted as UTF-8 LOWERCASE HEX so the exact bytes
// survive the ASCII TSV transport (the runner writes Java stdout as ASCII, which
// would mangle multi-byte UTF-8; byte parity is this gate's whole point). When
// objectiveName is null, hasObjective=0 and the objective_hex column is empty.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundResetScorePacket;

public class PktResetScoreParity {
    static final java.io.PrintStream O = System.out;

    static String hexUtf8(String s) {
        StringBuilder sb = new StringBuilder();
        for (byte b : s.getBytes(java.nio.charset.StandardCharsets.UTF_8))
            sb.append(String.format("%02x", b));
        return sb.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundResetScorePacket> codec =
            ClientboundResetScorePacket.STREAM_CODEC;

        // Finite/physical cases. owner is a non-null String (scoreboard owner name,
        // typically a player name or '*'); objectiveName is @Nullable. We cover:
        // - objectiveName absent (null) and present
        // - empty strings on both fields
        // - VarInt length-prefix boundary cross (1->2 byte prefix at >=128 bytes)
        // - ASCII + multi-byte UTF-8 (incl. a surrogate-pair emoji so the VarInt
        //   byte length exceeds the UTF-16 char count)
        StringBuilder big = new StringBuilder();
        for (int i = 0; i < 200; i++) big.append('x');   // 200 bytes -> 2-byte VarInt prefix
        StringBuilder p127 = new StringBuilder();
        for (int i = 0; i < 127; i++) p127.append('a');  // 127 bytes -> 1-byte VarInt prefix
        StringBuilder p128 = new StringBuilder();
        for (int i = 0; i < 128; i++) p128.append('b');  // 128 bytes -> 2-byte VarInt prefix

        // Each case: { name, owner, objectiveName(may be null) }
        Object[][] cases = {
            { "empty_null",        "",                  null },
            { "empty_empty",       "",                  "" },
            { "player_null",       "Notch",             null },
            { "player_obj",        "Notch",             "deathCount" },
            { "star_obj",          "*",                 "health" },
            { "owner_empty_obj",   "",                  "obj" },
            { "owner_obj_empty",   "owner",             "" },
            { "uuidlike_null",     "Player_123",        null },
            { "unicode_owner",     "niño",              "中文" },
            { "emoji_obj",         "player",            "😀" },         // U+1F600, surrogate pair
            { "mixed",             "niño中文",          "health😀" },
            { "owner127_null",     p127.toString(),     null },
            { "owner128_obj",      p128.toString(),     "x" },
            { "obj200",            "o",                 big.toString() },
            { "spaces",            "a b c",             "obj name" },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String owner = (String) c[1];
            String objectiveName = (String) c[2];   // may be null

            ClientboundResetScorePacket pkt = new ClientboundResetScorePacket(owner, objectiveName);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundResetScorePacket back = codec.decode(buf);
            boolean ownerOk = back.owner().equals(owner);
            boolean objOk = (objectiveName == null)
                ? (back.objectiveName() == null)
                : objectiveName.equals(back.objectiveName());
            if (!ownerOk || !objOk) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            int hasObjective = (objectiveName == null) ? 0 : 1;
            String objHex = (objectiveName == null) ? "" : hexUtf8(objectiveName);

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(hexUtf8(owner));
            O.print('\t');
            O.print(hasObjective);
            O.print('\t');
            O.print(objHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
