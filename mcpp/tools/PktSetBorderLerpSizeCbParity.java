// Ground truth for net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket.
//
// The packet has three fields (double oldSize, double newSize, long lerpTime). Its
// STREAM_CODEC (built via Packet.codec(write, new)) writes EXACTLY three fields, in
// this order (ClientboundSetBorderLerpSizePacket.java:29-33):
//     output.writeDouble(this.oldSize);
//     output.writeDouble(this.newSize);
//     output.writeVarLong(this.lerpTime);
// and reads them back with input.readDouble()/readDouble()/readVarLong(). No
// packet-type id is part of the codec bytes (that framing lives outside the codec).
//
// Wire layout, byte-for-byte:
//   1) writeDouble(oldSize) -> 8 big-endian bytes of Double.doubleToRawLongBits.
//   2) writeDouble(newSize) -> 8 big-endian bytes of Double.doubleToRawLongBits.
//   3) writeVarLong(lerpTime) -> VarLong.write: plain LEB128, NO zig-zag; every
//      negative long takes the full 10 bytes.
//
// The packet exposes no public all-args constructor (only the WorldBorder ctor and a
// private (FriendlyByteBuf) read ctor). We build each case by prefilling a buffer in
// the EXACT read order and invoking the private (FriendlyByteBuf) constructor via
// reflection -- so the packet's real fields are populated from our chosen values --
// then encode the packet through the REAL StreamCodec into a fresh FriendlyByteBuf
// and dump readableBytes() + the raw hex. We also decode the bytes back through the
// SAME codec and assert every field round-trips, so the C++ side proves read parity.
//
//   ENC <name>\t<oldSize rawLongBits, decimal>\t<newSize rawLongBits, decimal>\t<lerpTime decimal>\t<readableBytes>\t<wire hex>
//
// Doubles are carried as their raw long bits (Double.doubleToRawLongBits) so NaN /
// -0.0 / subnormals survive the ASCII TSV exactly; lerpTime is a plain decimal long.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Constructor;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetBorderLerpSizePacket;

public class PktSetBorderLerpSizeCbParity {
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

        StreamCodec<FriendlyByteBuf, ClientboundSetBorderLerpSizePacket> codec =
            ClientboundSetBorderLerpSizePacket.STREAM_CODEC;

        // Reflect the private (FriendlyByteBuf) read constructor.
        Constructor<ClientboundSetBorderLerpSizePacket> ctor =
            ClientboundSetBorderLerpSizePacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        ctor.setAccessible(true);

        // {name, oldSize, newSize, lerpTime}. Exercise normal values, signs, zeros,
        // -0.0, +/-Inf, NaN, subnormal, and VarLong boundaries (1->2 byte, max long,
        // negatives -> full 10 bytes, the 7-bit boundary 127/128).
        Object[][] cases = {
            { "zeros",          0.0,                    0.0,                    0L },
            { "size_world",     5.9999968E7,            5.9999968E7,            0L },
            { "shrinking",      1000.0,                 10.0,                   10000L },
            { "growing",        10.0,                   1000.0,                 200L },
            { "neg_zero_old",   -0.0,                   1.0,                    1L },
            { "pos_inf",        Double.POSITIVE_INFINITY, 1.0,                  127L },
            { "neg_inf",        Double.NEGATIVE_INFINITY, -1.0,                 128L },
            { "nan",            Double.NaN,             Double.NaN,             16383L },
            { "min_normal",     Double.MIN_VALUE,       Double.MIN_NORMAL,      16384L },
            { "max_double",     Double.MAX_VALUE,       1.0,                    Long.MAX_VALUE },
            { "neg_lerp",       2.0,                    3.0,                    -1L },
            { "min_long_lerp",  4.0,                    5.0,                    Long.MIN_VALUE },
            { "small_lerp",     1.5,                    2.5,                    255L },
            { "frac",           60000000.0,            29999984.0,            1234567890123L },
        };

        for (Object[] c : cases) {
            String name = (String)c[0];
            double oldSize = (Double)c[1];
            double newSize = (Double)c[2];
            long lerpTime = (Long)c[3];

            // Prefill a buffer in read order, then build the packet via the read ctor.
            FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
            seed.writeDouble(oldSize);
            seed.writeDouble(newSize);
            seed.writeVarLong(lerpTime);
            ClientboundSetBorderLerpSizePacket pkt = ctor.newInstance(seed);

            // Sanity: the read ctor populated the fields with our chosen values.
            if (Double.doubleToRawLongBits(pkt.getOldSize()) != Double.doubleToRawLongBits(oldSize)
                || Double.doubleToRawLongBits(pkt.getNewSize()) != Double.doubleToRawLongBits(newSize)
                || pkt.getLerpTime() != lerpTime) {
                throw new IllegalStateException("read-ctor field mismatch for " + name);
            }

            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            codec.encode(buf, pkt);
            int readable = buf.readableBytes();

            StringBuilder wire = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) wire.append(String.format("%02x", dup.readByte()));

            // Round-trip decode through the SAME codec to confirm the read side.
            ClientboundSetBorderLerpSizePacket back = codec.decode(buf);
            if (Double.doubleToRawLongBits(back.getOldSize()) != Double.doubleToRawLongBits(oldSize)
                || Double.doubleToRawLongBits(back.getNewSize()) != Double.doubleToRawLongBits(newSize)
                || back.getLerpTime() != lerpTime) {
                throw new IllegalStateException("round-trip mismatch for " + name);
            }

            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(Long.toUnsignedString(Double.doubleToRawLongBits(oldSize)));
            O.print('\t');
            O.print(Long.toUnsignedString(Double.doubleToRawLongBits(newSize)));
            O.print('\t');
            O.print(Long.toString(lerpTime));
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(wire);
            O.print('\n');
        }
    }
}
