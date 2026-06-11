// Ground truth for net.minecraft.network.protocol.cookie.ServerboundCookieResponsePacket.
//
// The packet is a record (Identifier key, byte @Nullable [] payload). Its
// STREAM_CODEC (built via Packet.codec(write, new)) writes EXACTLY two fields, in
// this order (ServerboundCookieResponsePacket.java:20-23):
//     output.writeIdentifier(this.key);
//     output.writeNullable(this.payload, ClientboundStoreCookiePacket.PAYLOAD_STREAM_CODEC);
// and reads them back with input.readIdentifier() / input.readNullable(...). No
// packet-type id is part of the codec bytes (that framing lives outside the codec).
//
// Wire layout, byte-for-byte:
//   1) writeIdentifier(key) -> writeUtf(key.toString())            (FriendlyByteBuf.java:585-586)
//        Utf8String.write -> VarInt(UTF-8 byteLength) then the UTF-8 bytes,
//        default maxLength 32767. Identifier.toString() = namespace+":"+path.
//   2) writeNullable(payload, codec)                                (FriendlyByteBuf.java:267-274)
//        payload == null -> writeBoolean(false)  (single 0x00)
//        payload != null -> writeBoolean(true) then codec.encode(payload), where
//        PAYLOAD_STREAM_CODEC = ByteBufCodecs.byteArray(5120) whose encode is
//        FriendlyByteBuf.writeByteArray (FriendlyByteBuf.java:289-292):
//            VarInt.write(output, bytes.length); output.writeBytes(bytes);
//      i.e. present-flag byte, then VarInt(len), then the raw payload bytes.
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and assert the key and payload round-trip so the C++ side proves
// read parity too.
//
//   ENC <name>\t<key.toString() UTF-8 HEX>\t<present 0|1>\t<payload HEX>\t<readableBytes>\t<wire hex>
//
// The key and payload columns are emitted as LOWERCASE HEX (run_groundtruth.ps1
// captures Java stdout and writes the TSV as ASCII, which would mangle raw bytes).
// An absent payload emits present=0 and an empty payload-hex column.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.nio.charset.StandardCharsets;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.cookie.ServerboundCookieResponsePacket;
import net.minecraft.resources.Identifier;

public class PktCookieResponseSbParity {
    static final java.io.PrintStream O = System.out;

    static String hex(byte[] bytes) {
        StringBuilder sb = new StringBuilder();
        for (byte b : bytes) sb.append(String.format("%02x", b));
        return sb.toString();
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundCookieResponsePacket> codec =
            ServerboundCookieResponsePacket.STREAM_CODEC;

        // A payload whose VarInt length prefix is exactly at the 1->2 byte boundary
        // (127 vs 128 bytes) to exercise the LEB128 length encoding.
        byte[] p127 = new byte[127];
        for (int i = 0; i < p127.length; i++) p127[i] = (byte)(i & 0x7f);
        byte[] p128 = new byte[128];
        for (int i = 0; i < p128.length; i++) p128[i] = (byte)(0xff - (i & 0xff));

        // {name, identifier, payload-or-null}.  null payload -> writeNullable(false).
        Object[][] cases = {
            { "null_payload",      Identifier.parse("minecraft:cookie"), null },
            { "empty_payload",     Identifier.parse("minecraft:cookie"), new byte[0] },
            { "default_ns",        Identifier.parse("stone"),            new byte[]{ 0x01, 0x02, 0x03 } },
            { "custom_ns",         Identifier.parse("mymod:session"),    new byte[]{ (byte)0xde, (byte)0xad, (byte)0xbe, (byte)0xef } },
            { "slashed_path",      Identifier.parse("minecraft:cookies/login"), new byte[]{ 0x00 } },
            { "all_byte_values",   Identifier.parse("mymod:blob"),       allBytes() },
            { "high_bytes",        Identifier.parse("a:b"),              new byte[]{ (byte)0x80, (byte)0xff, (byte)0x7f, (byte)0x00 } },
            { "payload_len127",    Identifier.parse("mymod:p127"),       p127 },
            { "payload_len128",    Identifier.parse("mymod:p128"),       p128 },
        };

        for (Object[] c : cases) {
            String name = (String)c[0];
            Identifier key = (Identifier)c[1];
            byte[] payload = (byte[])c[2];

            ServerboundCookieResponsePacket pkt = new ServerboundCookieResponsePacket(key, payload);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder wire = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) wire.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundCookieResponsePacket back = codec.decode(buf);
            if (!back.key().equals(key)) {
                throw new IllegalStateException("round-trip key mismatch for " + name
                    + ": " + back.key() + " != " + key);
            }
            if (!java.util.Arrays.equals(back.payload(), payload)) {
                throw new IllegalStateException("round-trip payload mismatch for " + name);
            }

            String keyHex = hex(key.toString().getBytes(StandardCharsets.UTF_8));
            int present = payload != null ? 1 : 0;
            String payloadHex = payload != null ? hex(payload) : "";

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(keyHex);
            O.print('\t');
            O.print(present);
            O.print('\t');
            O.print(payloadHex);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(wire);
            O.print('\n');
        }
    }

    static byte[] allBytes() {
        byte[] b = new byte[256];
        for (int i = 0; i < 256; i++) b[i] = (byte)i;
        return b;
    }
}
