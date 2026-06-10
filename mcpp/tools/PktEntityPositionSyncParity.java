// Ground truth for net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket.
//
// 26.1.2 wire format — verified VERBATIM against the REAL STREAM_CODECs:
//   ClientboundEntityPositionSyncPacket.STREAM_CODEC = StreamCodec.composite(
//       ByteBufCodecs.VAR_INT,             ::id,        -> VarInt entity id
//       PositionMoveRotation.STREAM_CODEC, ::values,    -> nested record (below)
//       ByteBufCodecs.BOOL,                ::onGround,  -> 1 byte (0/1)
//       ::new)
//   PositionMoveRotation.STREAM_CODEC = StreamCodec.composite(
//       Vec3.STREAM_CODEC,   ::position,        -> double x, double y, double z   (BE)
//       Vec3.STREAM_CODEC,   ::deltaMovement,   -> double vx, double vy, double vz (BE)
//       ByteBufCodecs.FLOAT, ::yRot,            -> float yRot (BE)
//       ByteBufCodecs.FLOAT, ::xRot,            -> float xRot (BE)
//       ::new)
//   Vec3.STREAM_CODEC.encode = writeDouble(x); writeDouble(y); writeDouble(z)  (big-endian).
//
// So the full payload is, in order:
//   VarInt(id)
//   double px, double py, double pz          (position, 8B each, BE)
//   double vx, double vy, double vz          (deltaMovement, 8B each, BE)
//   float  yRot, float xRot                  (4B each, BE)
//   byte   onGround                          (0/1)
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <id-dec> <pxBits-016x> <pyBits-016x> <pzBits-016x>
//       <vxBits-016x> <vyBits-016x> <vzBits-016x> <yRotBits-08x> <xRotBits-08x>
//       <onGround-dec> <readableBytes-dec> <hexBytes>
// where pos/delta bits are Double.doubleToRawLongBits (lowercase 16-hex), yRot/xRot bits are
// Float.floatToRawIntBits (lowercase 8-hex), onGround is 0/1. The C++ side reconstructs the
// exact doubles/floats and replays the codec field-by-field through mc::net::PacketBuffer,
// requiring byte-for-byte parity + <readableBytes>, then decodes back and round-trips fields.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundEntityPositionSyncPacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.entity.PositionMoveRotation;
import net.minecraft.world.phys.Vec3;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktEntityPositionSyncParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // Entity ids: exercise VarInt 1->5 byte boundaries + negatives + extrema.
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

        boolean[] grounds = { false, true };

        int caseNo = 0;
        // Full cross product would explode; walk indices in lockstep with rotating offsets so
        // every double/float/bool/id value is exercised in several combinations, hitting all
        // VarInt boundaries and every sign/zero/extreme coordinate. Deterministic, finite, physical.
        int nd = ds.length, nf = fs.length, ng = grounds.length, ni = ids.length;
        int total = Math.max(Math.max(nd, nf), Math.max(ng, ni)) * 4;
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
            boolean onGround = grounds[k % ng];
            emit("c" + (caseNo++), id, px, py, pz, vx, vy, vz, yRot, xRot, onGround);
        }

        // A few explicit extreme/edge rows to be certain the boundaries are covered.
        emit("zero", 0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0F, 0.0F, false);
        emit("negzero", 1, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0, -0.0F, -0.0F, true);
        emit("maxid", Integer.MAX_VALUE, 30000000.0, 320.0, -30000000.0,
             0.1, -0.2, 0.3, 180.0F, 90.0F, true);
        emit("minid", Integer.MIN_VALUE, -30000000.0, -64.0, 30000000.0,
             -0.1, 0.2, -0.3, -180.0F, -90.0F, false);
        emit("dblext", 268435455, Double.MAX_VALUE, -Double.MAX_VALUE, Double.MIN_VALUE,
             Double.MIN_NORMAL, -Double.MIN_VALUE, 0.0, Float.MAX_VALUE, -Float.MAX_VALUE, true);
    }

    static void emit(String name, int id, double px, double py, double pz,
                     double vx, double vy, double vz, float yRot, float xRot, boolean onGround) {
        ClientboundEntityPositionSyncPacket pkt = new ClientboundEntityPositionSyncPacket(
            id,
            new PositionMoveRotation(new Vec3(px, py, pz), new Vec3(vx, vy, vz), yRot, xRot),
            onGround
        );

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundEntityPositionSyncPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): every field must match exactly.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundEntityPositionSyncPacket back = ClientboundEntityPositionSyncPacket.STREAM_CODEC.decode(rb);
        if (back.id() != id) {
            throw new IllegalStateException("round-trip id mismatch: " + back.id() + " != " + id);
        }
        PositionMoveRotation v = back.values();
        if (Double.doubleToRawLongBits(v.position().x) != Double.doubleToRawLongBits(px)
            || Double.doubleToRawLongBits(v.position().y) != Double.doubleToRawLongBits(py)
            || Double.doubleToRawLongBits(v.position().z) != Double.doubleToRawLongBits(pz)
            || Double.doubleToRawLongBits(v.deltaMovement().x) != Double.doubleToRawLongBits(vx)
            || Double.doubleToRawLongBits(v.deltaMovement().y) != Double.doubleToRawLongBits(vy)
            || Double.doubleToRawLongBits(v.deltaMovement().z) != Double.doubleToRawLongBits(vz)
            || Float.floatToRawIntBits(v.yRot()) != Float.floatToRawIntBits(yRot)
            || Float.floatToRawIntBits(v.xRot()) != Float.floatToRawIntBits(xRot)
            || back.onGround() != onGround) {
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
        O.print(onGround ? 1 : 0);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
