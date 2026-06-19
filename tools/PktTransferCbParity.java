// Ground truth for net.minecraft.network.protocol.common.ClientboundTransferPacket.
//
// The packet is a record (String host, int port). Its STREAM_CODEC (built via
// Packet.codec(write, new)) writes ONLY:
//     output.writeUtf(this.host);     // VarInt byte-length prefix + UTF-8 bytes
//     output.writeVarInt(this.port);  // LEB128 VarInt
// and reads back input.readUtf() then input.readVarInt(). No packet-type id is
// part of the codec bytes (that framing lives outside the StreamCodec).
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and re-emit host/port so the C++ side proves read parity too.
//
//   ENC <name> <host_raw>\t<port>\t<readableBytes>\t<hex>
//
// Note: host is emitted raw (it is the exact UTF-8 String the packet carries);
// the C++ test parses the leading three tab-separated fields then the hex.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ClientboundTransferPacket;

public class PktTransferCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ClientboundTransferPacket> codec =
            ClientboundTransferPacket.STREAM_CODEC;

        // Finite/physical cases: empty host, ascii host, dotted host, unicode host,
        // and a sweep of port values exercising every VarInt byte-length boundary
        // (incl. the negatives, since `port` is a plain signed int written verbatim
        // as a 5-byte VarInt).
        Object[][] cases = {
            { "empty_p0",        "",                 0 },
            { "host_p25565",     "localhost",        25565 },
            { "ip_p1",           "127.0.0.1",        1 },
            { "fqdn_p65535",     "play.example.com", 65535 },
            { "unicode_p443",    "niño.中文.😀", 443 },
            { "host_p127",       "a",                127 },
            { "host_p128",       "a",                128 },
            { "host_p16383",     "a",                16383 },
            { "host_p16384",     "a",                16384 },
            { "host_p2097151",   "a",                2097151 },
            { "host_p2097152",   "a",                2097152 },
            { "host_p268435455", "a",                268435455 },
            { "host_p268435456", "a",                268435456 },
            { "host_pmax",       "a",                Integer.MAX_VALUE },
            { "host_pm1",        "a",                -1 },
            { "host_pmin",       "a",                Integer.MIN_VALUE },
        };

        for (Object[] c : cases) {
            String name = (String) c[0];
            String host = (String) c[1];
            int port = (Integer) c[2];

            ClientboundTransferPacket pkt = new ClientboundTransferPacket(host, port);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundTransferPacket back = codec.decode(buf);
            if (!back.host().equals(host) || back.port() != port) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            // host as UTF-8 HEX so the exact bytes survive the ASCII TSV transport
            // (run_groundtruth.ps1 writes the TSV as ASCII, mangling raw multi-byte UTF-8;
            // byte parity is this gate's whole point). C++ decodes the hex before writeUtf.
            StringBuilder hostHex = new StringBuilder();
            for (byte bb : host.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                hostHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(hostHex);
            O.print('\t');
            O.print(port);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
