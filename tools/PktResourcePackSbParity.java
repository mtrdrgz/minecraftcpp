// Ground truth for net.minecraft.network.protocol.common.ServerboundResourcePackPacket.
//
// The packet is a record (UUID id, ServerboundResourcePackPacket.Action action).
// Its STREAM_CODEC is built via Packet.codec(write, new), and write(FriendlyByteBuf)
// is EXACTLY (ServerboundResourcePackPacket.java lines 18-21):
//     output.writeUUID(this.id);       // two big-endian longs: MSB then LSB
//     output.writeEnum(this.action);   // == writeVarInt(action.ordinal())
// and read is:
//     new ServerboundResourcePackPacket(input.readUUID(),
//                                       input.readEnum(Action.class))
//   writeUUID(out,uuid) = out.writeLong(getMostSignificantBits());
//                         out.writeLong(getLeastSignificantBits());  (FriendlyByteBuf:498-501)
//   writeEnum(value)    = writeVarInt(value.ordinal())               (FriendlyByteBuf:471-473)
//   readEnum(clazz)     = clazz.getEnumConstants()[readVarInt()]     (FriendlyByteBuf:467-469)
// So the wire form is: 8 UUID bytes (hi BE) + 8 UUID bytes (lo BE) + VarInt(ordinal).
// No packet-type id is part of the codec bytes (that framing lives outside the
// StreamCodec). The STREAM_CODEC parameterises over plain FriendlyByteBuf, so a
// bare FriendlyByteBuf(Unpooled.buffer()) is sufficient (no registry access).
//
// Action declaration order (== ordinal, the writeEnum/readEnum index):
//   SUCCESSFULLY_LOADED=0, DECLINED=1, FAILED_DOWNLOAD=2, ACCEPTED=3,
//   DOWNLOADED=4, INVALID_URL=5, FAILED_RELOAD=6, DISCARDED=7
//   (ServerboundResourcePackPacket.Action, lines 32-40).
//
// Row format (tab separated):
//   ENUM  <ordinal> <name>                              per Action constant (enum gate)
//   ENC   <case> <uuidHi> <uuidLo> <ordinal> <readableBytes> <hex>   encode round-trip
//   DEC   <hex>  <uuidHi> <uuidLo> <ordinal>            decode: re-emit decoded fields
//
// UUIDs are emitted as their two signed longs (decimal). The C++ side encodes
// writeLong(hi)+writeLong(lo)+writeVarInt(ordinal) and must match byte-for-byte.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.UUID;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.common.ServerboundResourcePackPacket;

public class PktResourcePackSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        StreamCodec<FriendlyByteBuf, ServerboundResourcePackPacket> codec =
            ServerboundResourcePackPacket.STREAM_CODEC;

        // Enum gate: dump ordinal()+name() for every Action constant in declaration order.
        ServerboundResourcePackPacket.Action[] actions =
            ServerboundResourcePackPacket.Action.values();
        for (ServerboundResourcePackPacket.Action a : actions) {
            O.print("ENUM\t");
            O.print(a.ordinal());
            O.print('\t');
            O.print(a.name());
            O.print('\n');
        }

        // ENC/DEC cases: cross a set of UUIDs (zero, all-ones, sign boundaries on
        // each long, a real parsed UUID) with a representative set of actions,
        // plus one full sweep over every action with a fixed UUID so every ordinal
        // (including the multi-byte-VarInt-free 0..7 range) is exercised on the wire.
        // The UUID values are verbatim (sign-boundary longs, never made-up randoms).
        UUID realUuid = UUID.fromString("069a79f4-44e9-4726-a5be-fca90e38aaf5");
        Object[][] uuidCases = {
            // { caseName, hi, lo }
            { "zero",       0L,                  0L                  },
            { "lo_one",     0L,                  1L                  },
            { "all_ones",   -1L,                 -1L                 },
            { "hi_min",     Long.MIN_VALUE,      0L                  },
            { "hi_max",     Long.MAX_VALUE,      0L                  },
            { "lo_min",     0L,                  Long.MIN_VALUE      },
            { "lo_max",     0L,                  Long.MAX_VALUE      },
            { "mixed",      0x1234567890abcdefL, 0x0fedcba098765432L },
            { "real_uuid",  0L,                  0L                  }, // hi/lo overwritten below
        };

        // (uuidCase x action) cross product.
        for (Object[] uc : uuidCases) {
            String cname = (String) uc[0];
            UUID id = cname.equals("real_uuid")
                ? realUuid
                : new UUID((Long) uc[1], (Long) uc[2]);
            long hi = id.getMostSignificantBits();
            long lo = id.getLeastSignificantBits();
            for (ServerboundResourcePackPacket.Action a : actions) {
                emit(codec, cname, id, hi, lo, a);
            }
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundResourcePackPacket> codec,
                     String cname, UUID id, long hi, long lo,
                     ServerboundResourcePackPacket.Action a) {
        ServerboundResourcePackPacket pkt = new ServerboundResourcePackPacket(id, a);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Round-trip decode through the SAME codec to confirm the read side.
        ServerboundResourcePackPacket back = codec.decode(buf);
        if (!back.id().equals(id) || back.action() != a) {
            throw new IllegalStateException("round-trip mismatch for " + cname + "/" + a);
        }

        O.print("ENC\t");
        O.print(cname + "_" + a.name());
        O.print('\t');
        O.print(hi);
        O.print('\t');
        O.print(lo);
        O.print('\t');
        O.print(a.ordinal());
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');

        // DEC: feed the same bytes back and re-emit decoded fields so the C++ read
        // side can prove value parity independently of the encode hex.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundResourcePackPacket dec = codec.decode(rbuf);
        O.print("DEC\t");
        O.print(hex);
        O.print('\t');
        O.print(dec.id().getMostSignificantBits());
        O.print('\t');
        O.print(dec.id().getLeastSignificantBits());
        O.print('\t');
        O.print(dec.action().ordinal());
        O.print('\n');
    }

    static String toHex(FriendlyByteBuf b) {
        StringBuilder sb = new StringBuilder();
        ByteBuf dup = b.duplicate();
        while (dup.isReadable()) sb.append(String.format("%02x", dup.readByte() & 0xff));
        return sb.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
