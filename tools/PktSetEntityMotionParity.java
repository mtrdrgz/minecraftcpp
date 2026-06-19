// Ground truth for net.minecraft.network.protocol.game.ClientboundSetEntityMotionPacket.
//
// 26.1.2 wire format (verified against the REAL source, NOT the legacy short xa/ya/za):
//   ClientboundSetEntityMotionPacket.STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT, ::id,          -> VarInt entity id
//       Vec3.LP_STREAM_CODEC,  ::movement,    -> LpVec3-quantized Vec3
//       ::new)
// Vec3.LP_STREAM_CODEC = StreamCodec.of(LpVec3::write, LpVec3::read) (net.minecraft.network.LpVec3).
//
// LpVec3.write(out, v):
//   x=sanitize(v.x), y=sanitize(v.y), z=sanitize(v.z)        // NaN->0, clamp +-1.7179869183E10
//   cl = absMax(x, absMax(y, z)) = max(|x|, max(|y|,|z|))
//   if cl < 3.051944088384301E-5:  out.writeByte(0)
//   else:
//     scale  = ceilLong(cl) = (long)Math.ceil(cl)
//     partial= (scale & 3L) != scale
//     markers= partial ? (scale & 3L | 4L) : scale
//     xn=pack(x/scale)<<3, yn=pack(y/scale)<<18, zn=pack(z/scale)<<33
//     buffer = markers | xn | yn | zn
//     out.writeByte((byte)buffer); out.writeByte((byte)(buffer>>8)); out.writeInt((int)(buffer>>16))
//     if partial: VarInt.write(out, (int)(scale>>2))
//   pack(v) = Math.round((v*0.5+0.5)*32766.0)
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <xBits-016x> <yBits-016x> <zBits-016x> <readableBytes-dec> <hexBytes>
// where x/y/z bits are Double.doubleToRawLongBits of the INPUT Vec3 components (lowercase
// hex, big-endian-by-digits) so the C++ side reconstructs the exact same doubles and
// replays the exact LpVec3 quantization. <hexBytes> is the full packet payload, lowercase hex.
//
// The C++ pkt_set_entity_motion_parity reconstructs (id, x, y, z), replays the LpVec3.write
// algorithm writing through mc::net::PacketBuffer (writeVarInt/writeByte/writeInt/writeVarInt),
// and must match <hexBytes> byte-for-byte AND <readableBytes>; it then decodes <hexBytes> back
// and checks id round-trips (the quantized Vec3 cannot be compared exactly, so id + full byte
// parity is the gate).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSetEntityMotionPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktSetEntityMotionParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Finite / physical battery.
        // Entity ids exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // Vec3 cases chosen to hit every LpVec3 branch:
        //  - the zero / sub-ABS_MIN fast path (single 0 byte)
        //  - scale==1 (non-partial, markers=1)
        //  - scale 2,3 (markers fit in 2 bits, non-partial)
        //  - scale 4+ (partial: continuation flag + VarInt(scale>>2))
        //  - mixed signs, sub-unit magnitudes, large magnitudes, clamp + NaN sanitize
        double[][] vecs = {
            {0.0, 0.0, 0.0},                       // exact zero -> byte 0
            {1e-6, -1e-6, 5e-7},                    // below ABS_MIN (3.05e-5) -> byte 0
            {3.051944088384301E-5, 0.0, 0.0},       // exactly ABS_MIN -> NOT < min, scale 1
            {0.5, -0.5, 0.25},                      // scale 1, mixed sign sub-unit
            {0.999, -0.999, 0.999},                 // scale 1, near edge
            {1.0, 0.0, 0.0},                        // scale 1, axis
            {0.0, -1.0, 0.0},
            {0.0, 0.0, 1.0},
            {1.5, -1.2, 0.3},                       // scale 2, partial? scale=2 fits 2 bits -> non-partial
            {2.0, -2.0, 1.0},                       // scale 2
            {2.5, 0.1, -2.4},                       // scale 3
            {3.0, -3.0, 3.0},                       // scale 3
            {3.5, 0.0, 0.0},                        // scale 4 -> partial (continuation)
            {4.0, -4.0, 2.0},                       // scale 4 -> partial
            {7.0, -6.5, 3.2},                       // scale 7 -> partial, varint scale>>2
            {100.0, -50.0, 25.0},                   // scale 100 -> partial
            {1234.5, -678.25, 90.125},              // scale 1235 -> partial
            {1.0E6, -1.0E6, 5.0E5},                 // large -> multi-byte varint scale
            {1.7179869183E10, -1.7179869183E10, 1.0E5}, // ABS_MAX (no clamp)
            {1.0E11, -1.0E11, 1.0E11},              // above ABS_MAX -> clamps to +-ABS_MAX
            {Double.NaN, 1.0, -1.0},                // NaN sanitizes to 0
            {Double.POSITIVE_INFINITY, 0.0, 0.0},   // +Inf clamps to ABS_MAX
            {Double.NEGATIVE_INFINITY, 0.0, 0.0},   // -Inf clamps to -ABS_MAX
            {-0.0, 0.0, 0.0},                       // negative zero
            {0.1, 0.2, 0.3},                        // generic sub-unit
            {-9.81, 0.0, 12.5},                     // gravity-ish y
        };

        int caseNo = 0;
        for (int id : ids) {
            for (double[] v : vecs) {
                emit("c" + (caseNo++), id, v[0], v[1], v[2]);
            }
        }
    }

    static void emit(String name, int id, double x, double y, double z) {
        ClientboundSetEntityMotionPacket pkt =
            new ClientboundSetEntityMotionPacket(id, new Vec3(x, y, z));

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSetEntityMotionPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): id must match exactly.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundSetEntityMotionPacket back = ClientboundSetEntityMotionPacket.STREAM_CODEC.decode(rb);
        if (back.id() != id) {
            throw new IllegalStateException("round-trip id mismatch: " + back.id() + " != " + id);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(id);
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(x)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(y)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(z)));
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
