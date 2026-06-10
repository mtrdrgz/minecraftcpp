// Ground truth for net.minecraft.network.protocol.handshake.ClientIntentionPacket.
//
// The packet is a record (int protocolVersion, String hostName, int port,
// ClientIntent intention). Its STREAM_CODEC (built via Packet.codec(write, new))
// writes EXACTLY (read straight from 26.1.2/src ClientIntentionPacket.write):
//     output.writeVarInt(this.protocolVersion);   // LEB128 VarInt
//     output.writeUtf(this.hostName);             // VarInt UTF-8 byte-length + bytes (maxLen 32767 on write)
//     output.writeShort(this.port);               // netty writeShort: LOW 16 BITS, big-endian
//     output.writeVarInt(this.intention.id());    // NOTE: id() is 1/2/3, NOT the ordinal 0/1/2
// and reads back: readVarInt(), readUtf(255), readUnsignedShort(), ClientIntent.byId(readVarInt()).
//
// CRITICAL: the intention on the wire is intention.id() (STATUS=1, LOGIN=2,
// TRANSFER=3) — NOT the enum ordinal. See ClientIntent.id()/byId() in 26.1.2/src.
// readShort() vs readUnsignedShort(): the wire is always the low-16-bits big-endian
// short; the read just interprets it unsigned (0..65535).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf and
// dump readableBytes() + raw hex; we also decode the bytes back through the SAME
// codec and assert equality so the C++ side proves read parity too.
//
//   ENC <name> <protocolVersion> <hostNameHex> <port> <intentId> <readableBytes> <hex>
//
// hostName is emitted as UTF-8 HEX so the exact bytes survive the ASCII TSV transport
// (run_groundtruth.ps1 writes the TSV as ASCII, which would mangle raw multi-byte
// UTF-8; byte parity is this gate's whole point). The C++ decodes the hex before
// writeUtf. protocolVersion/port/intentId/readableBytes are decimal.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.handshake.ClientIntent;
import net.minecraft.network.protocol.handshake.ClientIntentionPacket;

public class PktClientIntentionParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientIntentionPacket> codec =
            ClientIntentionPacket.STREAM_CODEC;

        // All three intents (id 1/2/3 on the wire) used across cases.
        ClientIntent STATUS = ClientIntent.STATUS;
        ClientIntent LOGIN = ClientIntent.LOGIN;
        ClientIntent TRANSFER = ClientIntent.TRANSFER;

        // Finite/physical battery:
        //  * protocolVersion sweeps every VarInt byte-length boundary (and negatives).
        //  * hostName: empty / single ascii / dotted ip / fqdn / unicode (multi-byte).
        //  * port: full unsigned-16 range incl. boundaries; the codec writes the low
        //    16 bits, so 65536 wraps to 0, 65535 -> 0xffff, -1 -> 0xffff, etc.
        //  * intention: all three, exercising id() 1/2/3 (NOT ordinal).
        // Each row is an independent finite packet — no infinite/registry types.
        Object[][] cases = {
            // name                    protoVer            host                  port    intent
            { "empty_login",           0,                  "",                   0,      LOGIN },
            { "localhost_status",      770,                "localhost",          25565,  STATUS },
            { "ip_login",              770,                "127.0.0.1",          25565,  LOGIN },
            { "fqdn_transfer",         770,                "play.example.com",   25565,  TRANSFER },
            { "unicode_login",         770,                "niño.中文.😀", 443, LOGIN },
            // protocolVersion VarInt boundaries
            { "pv_p1",                 1,                  "a",                  25565,  LOGIN },
            { "pv_127",                127,                "a",                  25565,  LOGIN },
            { "pv_128",                128,                "a",                  25565,  LOGIN },
            { "pv_16383",              16383,              "a",                  25565,  LOGIN },
            { "pv_16384",              16384,              "a",                  25565,  LOGIN },
            { "pv_2097151",            2097151,            "a",                  25565,  LOGIN },
            { "pv_2097152",            2097152,            "a",                  25565,  LOGIN },
            { "pv_268435455",          268435455,          "a",                  25565,  LOGIN },
            { "pv_268435456",          268435456,          "a",                  25565,  LOGIN },
            { "pv_max",                Integer.MAX_VALUE,  "a",                  25565,  LOGIN },
            { "pv_m1",                 -1,                 "a",                  25565,  LOGIN },
            { "pv_min",                Integer.MIN_VALUE,  "a",                  25565,  LOGIN },
            // port boundaries (low 16 bits, big-endian short)
            { "port_0",                770,                "a",                  0,      LOGIN },
            { "port_1",                770,                "a",                  1,      LOGIN },
            { "port_255",              770,                "a",                  255,    LOGIN },
            { "port_256",              770,                "a",                  256,    LOGIN },
            { "port_32767",            770,                "a",                  32767,  LOGIN },
            { "port_32768",            770,                "a",                  32768,  LOGIN },
            { "port_65535",            770,                "a",                  65535,  LOGIN },
            { "port_19132",            770,                "a",                  19132,  LOGIN },
            // intents on a common host/port
            { "intent_status",         770,                "host",               25565,  STATUS },
            { "intent_login",          770,                "host",               25565,  LOGIN },
            { "intent_transfer",       770,                "host",               25565,  TRANSFER },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            int protocolVersion = (Integer) c[1];
            String hostName = (String) c[2];
            int port = (Integer) c[3];
            ClientIntent intention = (ClientIntent) c[4];

            ClientIntentionPacket pkt =
                new ClientIntentionPacket(protocolVersion, hostName, port, intention);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte() & 0xff));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientIntentionPacket back = codec.decode(buf);
            // Note: readUnsignedShort() makes the decoded port the unsigned low-16 of port.
            int expectedPort = port & 0xffff;
            if (back.protocolVersion() != protocolVersion
                || !back.hostName().equals(hostName)
                || back.port() != expectedPort
                || back.intention() != intention) {
                throw new IllegalStateException("round-trip mismatch for " + name
                    + ": pv=" + back.protocolVersion()
                    + " host=" + back.hostName()
                    + " port=" + back.port() + " (exp " + expectedPort + ")"
                    + " intent=" + back.intention());
            }

            // hostName as UTF-8 HEX so exact bytes survive ASCII TSV transport.
            StringBuilder hostHex = new StringBuilder();
            for (byte bb : hostName.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                hostHex.append(String.format("%02x", bb & 0xff));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(protocolVersion);
            O.print('\t');
            O.print(hostHex.length() == 0 ? "-" : hostHex.toString());
            O.print('\t');
            O.print(port);
            O.print('\t');
            O.print(intention.id());   // wire value: 1/2/3 (NOT ordinal)
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
