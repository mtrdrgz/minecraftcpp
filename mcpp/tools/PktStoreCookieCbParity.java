// Ground truth for net.minecraft.network.protocol.common.ClientboundStoreCookiePacket.
//
// The packet is a record (Identifier key, byte[] payload). Its STREAM_CODEC is
// built via Packet.codec(write, new) and its write(FriendlyByteBuf) is EXACTLY:
//     output.writeIdentifier(this.key);                 // == writeUtf(key.toString())
//     PAYLOAD_STREAM_CODEC.encode(output, this.payload);// == writeByteArray(payload)
// where:
//   writeIdentifier(id)            -> writeUtf(id.toString())  (FriendlyByteBuf.java:585-587)
//        writeUtf -> VarInt(UTF-8 byte length) + UTF-8 bytes
//   PAYLOAD_STREAM_CODEC = ByteBufCodecs.byteArray(5120)
//        encode -> FriendlyByteBuf.writeByteArray(output, value)  (ByteBufCodecs.java:262)
//        writeByteArray -> VarInt(bytes.length) + raw bytes       (FriendlyByteBuf.java:289-292)
// The read side is: this(input.readIdentifier(), PAYLOAD_STREAM_CODEC.decode(input)).
// No packet-type id is part of the codec bytes (that framing lives outside the codec).
//
// Identifier.toString() is "namespace:path"; both namespace and path are ASCII-only
// (allowed chars [a-z0-9_.-/ ]), so key bytes are pure ASCII. We still emit the key as
// UTF-8 HEX (and payload as raw HEX) so the TSV survives the ASCII run_groundtruth.ps1
// transport; ints (readableBytes) are decimal.
//
//   ENC <name> <key_utf8_hex> <payload_hex> <readableBytes> <hex>
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.Arrays;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ClientboundStoreCookiePacket;
import net.minecraft.resources.Identifier;

public class PktStoreCookieCbParity {
    static final java.io.PrintStream O = System.out;

    // Build a payload of `n` bytes cycling 0x00..0xFF so every byte value is exercised.
    static byte[] ramp(int n) {
        byte[] b = new byte[n];
        for (int i = 0; i < n; i++) b[i] = (byte) (i & 0xFF);
        return b;
    }

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundStoreCookiePacket> codec =
            ClientboundStoreCookiePacket.STREAM_CODEC;

        // Finite/physical cases. The key sweeps namespaces/paths (incl. the longest
        // legal-ish ascii) and the payload sweeps VarInt-length boundaries
        // (0, 1, 127, 128 bytes) plus a full 0x00..0xFF byte-value ramp. MAX_PAYLOAD_SIZE
        // is 5120; all payloads stay <= that so encode/decode never throws.
        Object[][] cases = {
            // name              namespace     path                          payload
            { "empty_payload",   "minecraft",  "session",                    ramp(0)   },
            { "one_byte",        "minecraft",  "session",                    ramp(1)   },
            { "ramp256",         "minecraft",  "cookie",                     ramp(256) },
            { "vlen127",         "minecraft",  "x",                          ramp(127) },
            { "vlen128",         "minecraft",  "x",                          ramp(128) },
            { "custom_ns",       "mymod",      "auth/token",                 ramp(16)  },
            { "dotted",          "a.b-c_d",    "p.q-r_s",                    ramp(8)   },
            { "digits",          "ns123",      "path456",                    ramp(2)   },
            { "max_payload",     "minecraft",  "big",                        ramp(5120)},
            { "underscore_key",  "minecraft",  "store_cookie_session_token", ramp(4)   },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String ns = (String) c[1];
            String path = (String) c[2];
            byte[] payload = (byte[]) c[3];

            Identifier key = Identifier.fromNamespaceAndPath(ns, path);
            ClientboundStoreCookiePacket pkt = new ClientboundStoreCookiePacket(key, payload);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec (consumes buf) to confirm read side.
            ClientboundStoreCookiePacket back = codec.decode(buf);
            if (!back.key().equals(key) || !Arrays.equals(back.payload(), payload)) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // key.toString() as UTF-8 HEX, payload as raw HEX (ASCII-safe transport).
            StringBuilder keyHex = new StringBuilder();
            for (byte bb : key.toString().getBytes(java.nio.charset.StandardCharsets.UTF_8))
                keyHex.append(String.format("%02x", bb));
            StringBuilder payHex = new StringBuilder();
            for (byte bb : payload) payHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(keyHex);
            O.print('\t');
            O.print(payHex.length() == 0 ? "-" : payHex.toString());
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
