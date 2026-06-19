// Ground truth for net.minecraft.network.protocol.game.ClientboundPlayerPositionPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL STREAM_CODECs:
//   ClientboundPlayerPositionPacket.STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,             ::id,         -> VarInt teleport id
//       PositionMoveRotation.STREAM_CODEC, ::change,     -> nested record (below)
//       Relative.SET_STREAM_CODEC,         ::relatives,  -> int bitmask (below)
//       ::new)
//   PositionMoveRotation.STREAM_CODEC = StreamCodec.composite(
//       Vec3.STREAM_CODEC,   ::position,        -> double x, double y, double z    (BE)
//       Vec3.STREAM_CODEC,   ::deltaMovement,   -> double vx, double vy, double vz  (BE)
//       ByteBufCodecs.FLOAT, ::yRot,            -> float yRot (BE 4 bytes)
//       ByteBufCodecs.FLOAT, ::xRot,            -> float xRot (BE 4 bytes)
//       ::new)
//   Vec3.STREAM_CODEC.encode    = writeDouble(x); writeDouble(y); writeDouble(z)  (big-endian).
//   ByteBufCodecs.FLOAT.encode  = output.writeFloat(value)                        (big-endian 4B).
//   Relative.SET_STREAM_CODEC   = ByteBufCodecs.INT.map(Relative::unpack, Relative::pack)
//                                 -> output.writeInt(Relative.pack(set))          (big-endian 4B).
//     Relative.pack(set) ORs (1 << bit) for each member; bits:
//        X=0 Y=1 Z=2 Y_ROT=3 X_ROT=4 DELTA_X=5 DELTA_Y=6 DELTA_Z=7 ROTATE_DELTA=8.
//
// Full payload, in order:
//   VarInt(id)
//   double px, double py, double pz          (position, 8B each, BE)
//   double vx, double vy, double vz          (deltaMovement, 8B each, BE)
//   float  yRot, float xRot                  (4B each, BE)
//   int    relativesMask                     (4B, BE)
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <pxBits-016x> <pyBits-016x> <pzBits-016x>
//       <vxBits-016x> <vyBits-016x> <vzBits-016x> <yRotBits-08x> <xRotBits-08x>
//       <relativesMask-dec> <readableBytes-dec> <hexBytes>
// where pos/delta bits are Double.doubleToRawLongBits (lowercase 16-hex), yRot/xRot bits are
// Float.floatToRawIntBits (lowercase 8-hex), relativesMask is the decimal of Relative.pack(set).
// The C++ side reconstructs the exact doubles/floats + the int mask and replays the codec
// field-by-field through mc::net::PacketBuffer, requiring byte-for-byte parity + <readableBytes>,
// then decodes back and round-trips every field.
import io.netty.buffer.Unpooled;

import java.util.EnumSet;
import java.util.Set;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundPlayerPositionPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.PositionMoveRotation;
import net.minecraft.world.entity.Relative;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktPlayerPositionParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Teleport ids: exercise VarInt 1->5 byte boundaries + negatives + extrema.
        int[] ids = {
            0, 1, 127, 128, 16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, 42, 12345, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // Double coordinate samples: 0.0/-0.0/1.0/small/large/typical-coordinate/extrema/sign.
        double[] ds = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
            3.0517578125E-5, -2.5E-7,
            64.5, -123.0, 1234.5, -678.25,
            30000000.0, -29999999.5,                 // near world border
            1.0E10, -1.0E10,
            Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.MIN_VALUE, Double.MIN_NORMAL,
            123.456789, -987.654321
        };

        // Float rotation samples: 0/-0/1/typical yaw/pitch/extrema/sign.
        float[] fs = {
            0.0F, -0.0F, 1.0F, -1.0F,
            90.0F, -90.0F, 180.0F, -179.99F, 45.5F, -45.5F,
            359.9F, 0.001F, -0.001F,
            Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.MIN_VALUE, Float.MIN_NORMAL
        };

        // Relative subsets: empty, each singleton, classic combos, and ALL (mask 0..511).
        Set<Relative>[] relSets = new Set[] {
            EnumSet.noneOf(Relative.class),                                   // 0
            EnumSet.of(Relative.X),                                           // 1
            EnumSet.of(Relative.Y),                                           // 2
            EnumSet.of(Relative.Z),                                           // 4
            EnumSet.of(Relative.Y_ROT),                                       // 8
            EnumSet.of(Relative.X_ROT),                                       // 16
            EnumSet.of(Relative.DELTA_X),                                     // 32
            EnumSet.of(Relative.DELTA_Y),                                     // 64
            EnumSet.of(Relative.DELTA_Z),                                     // 128
            EnumSet.of(Relative.ROTATE_DELTA),                               // 256
            EnumSet.copyOf(Relative.ROTATION),                               // X_ROT|Y_ROT = 24
            EnumSet.copyOf(Relative.DELTA),                                  // delta x/y/z + rotate = 480
            EnumSet.of(Relative.X, Relative.Y, Relative.Z),                  // 7
            EnumSet.copyOf(Relative.ALL),                                    // 511
            EnumSet.of(Relative.X, Relative.Z, Relative.X_ROT, Relative.DELTA_Y, Relative.ROTATE_DELTA) // mixed
        };

        boolean[] grounds = { false, true };  // unused field-wise (no onGround here), kept for parity of harness shape

        int caseNo = 0;
        // Lockstep index walk with rotating offsets so every id/double/float/relative-set value is
        // exercised in several combinations, hitting all VarInt boundaries, every sign/zero/extreme
        // coordinate, and every relatives-bit. Deterministic, finite.
        int nd = ds.length, nf = fs.length, nr = relSets.length, ni = ids.length;
        int total = Math.max(Math.max(nd, nf), Math.max(nr, ni)) * 4;
        for (int k = 0; k < total; k++) {
            int id = ids[k % ni];
            double px = ds[k % nd];
            double py = ds[(k + 3) % nd];
            double pz = ds[(k + 7) % nd];
            double vx = ds[(k + 1) % nd];
            double vy = ds[(k + 5) % nd];
            double vz = ds[(k + 11) % nd];
            float yRot = fs[k % nf];
            float xRot = fs[(k + 2) % nf];
            Set<Relative> rel = relSets[k % nr];
            emit("c" + (caseNo++), id, px, py, pz, vx, vy, vz, yRot, xRot, rel);
        }

        // A few explicit extreme/edge rows to be certain the boundaries are covered.
        emit("zero", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0F, 0.0F, EnumSet.noneOf(Relative.class));
        emit("negzero", 1, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0F, -0.0F, EnumSet.of(Relative.X));
        emit("allrel", Integer.MAX_VALUE, 30000000.0, 320.0, -30000000.0,
             0.1, -0.2, 0.3, 180.0F, 90.0F, EnumSet.copyOf(Relative.ALL));
        emit("minid", Integer.MIN_VALUE, -30000000.0, -64.0, 30000000.0,
             -0.1, 0.2, -0.3, -180.0F, -90.0F, EnumSet.of(Relative.ROTATE_DELTA));
        emit("dblext", 268435455, Double.MAX_VALUE, -Double.MAX_VALUE, Double.MIN_VALUE,
             Double.MIN_NORMAL, -Double.MIN_VALUE, 0.0, Float.MAX_VALUE, -Float.MAX_VALUE,
             EnumSet.copyOf(Relative.DELTA));
    }

    static void emit(String name, int id, double px, double py, double pz,
                     double vx, double vy, double vz, float yRot, float xRot, Set<Relative> rel) {
        ClientboundPlayerPositionPacket pkt = new ClientboundPlayerPositionPacket(
            id,
            new PositionMoveRotation(new Vec3(px, py, pz), new Vec3(vx, vy, vz), yRot, xRot),
            rel
        );

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundPlayerPositionPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        int mask = Relative.pack(rel);

        // Round-trip decode through the SAME codec (sanity): every field must match exactly
        // (doubles/floats by RAW BITS so NaN/-0.0 are required to survive).
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundPlayerPositionPacket back = ClientboundPlayerPositionPacket.STREAM_CODEC.decode(rb);
        if (back.id() != id) {
            throw new IllegalStateException("round-trip id mismatch: " + back.id() + " != " + id);
        }
        PositionMoveRotation v = back.change();
        if (Double.doubleToRawLongBits(v.position().x) != Double.doubleToRawLongBits(px)
            || Double.doubleToRawLongBits(v.position().y) != Double.doubleToRawLongBits(py)
            || Double.doubleToRawLongBits(v.position().z) != Double.doubleToRawLongBits(pz)
            || Double.doubleToRawLongBits(v.deltaMovement().x) != Double.doubleToRawLongBits(vx)
            || Double.doubleToRawLongBits(v.deltaMovement().y) != Double.doubleToRawLongBits(vy)
            || Double.doubleToRawLongBits(v.deltaMovement().z) != Double.doubleToRawLongBits(vz)
            || Float.floatToRawIntBits(v.yRot()) != Float.floatToRawIntBits(yRot)
            || Float.floatToRawIntBits(v.xRot()) != Float.floatToRawIntBits(xRot)
            || Relative.pack(back.relatives()) != mask) {
            throw new IllegalStateException("round-trip field mismatch for " + name);
        }
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(id);
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(px)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(py)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(pz)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(vx)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(vy)));
        O.print('\t');
        O.print(String.format("%016x", Double.doubleToRawLongBits(vz)));
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(yRot)));
        O.print('\t');
        O.print(String.format("%08x", Float.floatToRawIntBits(xRot)));
        O.print('\t');
        O.print(mask);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
