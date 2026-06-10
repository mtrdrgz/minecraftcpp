// Ground truth for net.minecraft.network.protocol.login.ServerboundHelloPacket.
//
// The packet is a record (String name, UUID profileId). Its STREAM_CODEC is
// built via Packet.codec(write, new), and write(FriendlyByteBuf) is EXACTLY:
//     output.writeUtf(this.name, 16);   // VarInt(UTF-8 byte length) + UTF-8 bytes
//     output.writeUUID(this.profileId); // two big-endian longs: MSB then LSB
// and read is: new ServerboundHelloPacket(input.readUtf(16), input.readUUID()).
// No packet-type id is part of the codec bytes (that framing lives outside the
// StreamCodec). The maxLength=16 is a UTF-16 code-unit cap (String.length()),
// with a byte cap of 16*3=48; all cases below stay within that bound.
//
// We encode each case through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex; we also decode the bytes back through
// the SAME codec and re-emit name/profileId so the C++ side proves read parity.
//
//   ENC \t <name> \t <nameUtf8Hex> \t <uuidHi> \t <uuidLo> \t <readableBytes> \t <hex>
//
// `name` is emitted as LOWERCASE UTF-8 HEX so the exact bytes survive the ASCII
// TSV transport (run_groundtruth.ps1 writes the TSV as ASCII, which would mangle
// raw multi-byte UTF-8). The UUID is emitted as its two signed longs (decimal).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.login.ServerboundHelloPacket;

public class PktHelloSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundHelloPacket> codec =
            ServerboundHelloPacket.STREAM_CODEC;

        // Finite/physical cases. The String exercises: empty (VarInt 0 length),
        // single ASCII, a typical 16-char name (the max-length boundary), and
        // a multi-byte UTF-8 name kept under 16 UTF-16 code units. The UUID
        // exercises zero, all-ones, sign boundaries on both longs, and a real
        // random-looking value (constructed verbatim, never made up downstream).
        Object[][] cases = {
            // { caseName, name, uuidMostSigBits, uuidLeastSigBits }
            { "empty_zero",     "",                 0L,                  0L                  },
            { "a_one",          "a",                0L,                  1L                  },
            { "ascii_typical",  "Notch",            0x1234567890abcdefL, 0x0fedcba098765432L },
            { "name16_max",     "Steve_The_Player",  -1L,                 -1L                 },
            { "unicode_short",  "niño_中_😀",       0x8000000000000000L, 0x7fffffffffffffffL },
            { "hi_min",         "x",                Long.MIN_VALUE,      0L                  },
            { "hi_max",         "x",                Long.MAX_VALUE,      0L                  },
            { "lo_min",         "x",                0L,                  Long.MIN_VALUE      },
            { "lo_neg1",        "x",                -1L,                 -1L                 },
            { "real_uuid",      "Dinnerbone",       0L,                  0L                  }, // hi/lo overwritten below
        };

        // Use a concrete parsed UUID for the "real_uuid" case so it is verbatim,
        // not a random/made-up value.
        UUID realUuid = UUID.fromString("069a79f4-44e9-4726-a5be-fca90e38aaf5");

        for (Object[] c : cases) {
            String cname = (String) c[0];
            String name = (String) c[1];
            long hi = (Long) c[2];
            long lo = (Long) c[3];
            UUID id = cname.equals("real_uuid")
                ? realUuid
                : new UUID(hi, lo);
            // Re-read the actual bits we will encode (so the emitted longs match
            // exactly what the packet carries, including for realUuid).
            hi = id.getMostSignificantBits();
            lo = id.getLeastSignificantBits();

            ServerboundHelloPacket pkt = new ServerboundHelloPacket(name, id);

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ServerboundHelloPacket back = codec.decode(buf);
            if (!back.name().equals(name) || !back.profileId().equals(id)) {
                throw new IllegalStateException("round-trip mismatch for " + cname);
            }

            // name as UTF-8 HEX so the exact bytes survive the ASCII TSV transport.
            StringBuilder nameHex = new StringBuilder();
            for (byte bb : name.getBytes(java.nio.charset.StandardCharsets.UTF_8))
                nameHex.append(String.format("%02x", bb));

            O.print("ENC\t");
            O.print(cname);
            O.print('\t');
            O.print(nameHex);
            O.print('\t');
            O.print(hi);
            O.print('\t');
            O.print(lo);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
    }
}
