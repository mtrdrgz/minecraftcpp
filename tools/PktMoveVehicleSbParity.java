// Ground truth for net.minecraft.network.protocol.game.ServerboundMoveVehiclePacket's
// StreamCodec.
//
// The packet is a record(Vec3 position, float yRot, float xRot, boolean onGround)
// (ServerboundMoveVehiclePacket.java:11-22). Its STREAM_CODEC is StreamCodec.composite
// over, in this exact wire order:
//   Vec3.STREAM_CODEC      -> position   (Vec3.java:25-35: writeDouble(x); writeDouble(y); writeDouble(z))
//   ByteBufCodecs.FLOAT    -> yRot       (ByteBufCodecs.java:132-140: writeFloat)
//   ByteBufCodecs.FLOAT    -> xRot       (ByteBufCodecs.java:132-140: writeFloat)
//   ByteBufCodecs.BOOL     -> onGround   (ByteBufCodecs.java:56-64:  writeBoolean)
// So the body is exactly: double x, double y, double z (each big-endian IEEE-754, 8B)
// then float yRot, float xRot (each big-endian IEEE-754, 4B) then 1 byte bool (00/01).
// Packet codec -> no packet-id prefix, just the body.
//
// Every field is a plain number/bool: no registry/ItemStack/Component/Holder/NBT, so
// the body is fully representable by the certified PacketBuffer (FriendlyByteBuf) port.
//
// Row formats (tab separated). Doubles are emitted as %016x of their raw long bits
// (Double.doubleToRawLongBits) and floats as %08x of their raw int bits
// (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly without parse rounding;
// onGround as a 0/1 decimal; hex columns are lowercase hex:
//   ENC <xBits> <yBits> <zBits> <yRotBits> <xRotBits> <onGround> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <xBits> <yBits> <zBits> <yRotBits> <xRotBits> <onGround> ...decoded...
//        decode: STREAM_CODEC.decode(buf) -> position()/yRot()/xRot()/onGround() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundMoveVehiclePacket;
import net.minecraft.world.phys.Vec3;

public class PktMoveVehicleSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundMoveVehiclePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundMoveVehiclePacket>)
                ServerboundMoveVehiclePacket.STREAM_CODEC;

        // Finite/physical position-coordinate battery (doubles): zero/sign, small,
        // fractional, world-extent magnitudes, and special IEEE-754 doubles to exercise
        // the full 8-byte encoding of each Vec3 component.
        double[] coords = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
            64.0, 256.5, -256.5, 1234.5678, -1234.5678,
            29999984.0, -29999984.0, 12345678.90123,
            Double.MIN_VALUE, Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.NaN,
        };
        // Rotation battery (floats): physical degree range plus sign/zero/special floats.
        float[] rots = {
            0.0f, -0.0f, 45.0f, 90.0f, 180.0f, -180.0f, 360.0f,
            22.5f, -22.5f, 89.99f, 179.99f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };
        boolean[] grounds = { false, true };

        // Full cross-product would be huge; build a focused-but-thorough set that pins
        // each (x,y,z) component independently against representative rot/ground values
        // while still covering every special coord/rot. We iterate coords as the primary
        // axis (varying x and reusing it for y,z via index rotation) and pair with a
        // rotating set of rotations + both ground flags.
        int ri = 0;
        for (int i = 0; i < coords.length; i++) {
            double x = coords[i];
            double y = coords[(i + 1) % coords.length];
            double z = coords[(i + 2) % coords.length];
            // Cover every rotation value at least once across the coord sweep, and pair
            // both ground flags at each step.
            float yRot = rots[ri % rots.length];
            float xRot = rots[(ri + 5) % rots.length];
            ri++;
            for (boolean g : grounds) {
                emit(CODEC, x, y, z, yRot, xRot, g);
            }
        }
        // Also sweep every rotation value explicitly (pinning yRot then xRot) at a fixed
        // physical position so all special floats are exercised in both rotation slots.
        for (int i = 0; i < rots.length; i++) {
            emit(CODEC, 100.5, 64.0, -200.25, rots[i], 0.0f, true);
            emit(CODEC, 100.5, 64.0, -200.25, 0.0f, rots[i], false);
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundMoveVehiclePacket> CODEC,
                     double x, double y, double z, float yRot, float xRot, boolean onGround) {
        // Build a packet with this exact (position, yRot, xRot, onGround) via the
        // canonical record ctor.
        ServerboundMoveVehiclePacket pkt =
            new ServerboundMoveVehiclePacket(new Vec3(x, y, z), yRot, xRot, onGround);

        // ENC: encode through the REAL codec, dump readableBytes + body bytes.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);
        int n = buf.readableBytes();
        String hex = toHex(buf);

        long xBits = Double.doubleToRawLongBits(x);
        long yBits = Double.doubleToRawLongBits(y);
        long zBits = Double.doubleToRawLongBits(z);
        int yRotBits = Float.floatToRawIntBits(yRot);
        int xRotBits = Float.floatToRawIntBits(xRot);

        O.print("ENC\t");
        O.print(String.format("%016x", xBits));
        O.print('\t');
        O.print(String.format("%016x", yBits));
        O.print('\t');
        O.print(String.format("%016x", zBits));
        O.print('\t');
        O.print(String.format("%08x", yRotBits));
        O.print('\t');
        O.print(String.format("%08x", xRotBits));
        O.print('\t');
        O.print(onGround ? 1 : 0);
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex);
        O.print('\n');

        // DEC: decode the same bytes through the REAL codec; round-trip sanity (abort on
        // any drift so no bad row is ever emitted).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundMoveVehiclePacket dec = CODEC.decode(rbuf);
        Vec3 dp = dec.position();
        long dxBits = Double.doubleToRawLongBits(dp.x());
        long dyBits = Double.doubleToRawLongBits(dp.y());
        long dzBits = Double.doubleToRawLongBits(dp.z());
        int dYRotBits = Float.floatToRawIntBits(dec.yRot());
        int dXRotBits = Float.floatToRawIntBits(dec.xRot());
        boolean dGround = dec.onGround();
        if (dxBits != xBits || dyBits != yBits || dzBits != zBits
            || dYRotBits != yRotBits || dXRotBits != xRotBits || dGround != onGround) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + String.format("%016x", xBits) + ","
                    + String.format("%016x", yBits) + "," + String.format("%016x", zBits) + ","
                    + String.format("%08x", yRotBits) + "," + String.format("%08x", xRotBits) + ","
                    + onGround + ") out=(" + String.format("%016x", dxBits) + ","
                    + String.format("%016x", dyBits) + "," + String.format("%016x", dzBits) + ","
                    + String.format("%08x", dYRotBits) + "," + String.format("%08x", dXRotBits) + ","
                    + dGround + ")");
        }
        O.print("DEC\t");
        O.print(hex);
        O.print('\t');
        O.print(String.format("%016x", xBits));
        O.print('\t');
        O.print(String.format("%016x", yBits));
        O.print('\t');
        O.print(String.format("%016x", zBits));
        O.print('\t');
        O.print(String.format("%08x", yRotBits));
        O.print('\t');
        O.print(String.format("%08x", xRotBits));
        O.print('\t');
        O.print(onGround ? 1 : 0);
        O.print('\t');
        O.print(String.format("%016x", dxBits));
        O.print('\t');
        O.print(String.format("%016x", dyBits));
        O.print('\t');
        O.print(String.format("%016x", dzBits));
        O.print('\t');
        O.print(String.format("%08x", dYRotBits));
        O.print('\t');
        O.print(String.format("%08x", dXRotBits));
        O.print('\t');
        O.print(dGround ? 1 : 0);
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
