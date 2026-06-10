// Ground truth for net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.PosRot's
// StreamCodec.
//
// ServerboundMovePlayerPacket.PosRot (ServerboundMovePlayerPacket.java:144-184) extends the
// abstract ServerboundMovePlayerPacket and carries: double x, double y, double z, float yRot,
// float xRot, plus two booleans (onGround, horizontalCollision) packed into ONE byte.
//
// Its STREAM_CODEC is Packet.codec(PosRot::write, PosRot::read) == StreamCodec.ofMember(...)
// (Packet.java:22-24), so there is NO packet-id prefix: the on-wire body is EXACTLY what
// PosRot.write(FriendlyByteBuf) emits, IN THIS EXACT ORDER (ServerboundMovePlayerPacket.java
// :171-178):
//   output.writeDouble(this.x);     // big-endian IEEE-754 8 bytes  (writeDouble)
//   output.writeDouble(this.y);     // big-endian IEEE-754 8 bytes  (writeDouble)
//   output.writeDouble(this.z);     // big-endian IEEE-754 8 bytes  (writeDouble)
//   output.writeFloat(this.yRot);   // big-endian IEEE-754 4 bytes  (writeFloat)
//   output.writeFloat(this.xRot);   // big-endian IEEE-754 4 bytes  (writeFloat)
//   output.writeByte(packFlags(onGround, horizontalCollision)); // 1 byte, bit0=onGround, bit1=horizCollision
// where packFlags (ServerboundMovePlayerPacket.java:22-33) = (onGround?1:0) | (horizontalCollision?2:0).
// So the body is exactly: double(x) ++ double(y) ++ double(z) ++ float(yRot) ++ float(xRot) ++
// byte(flags), 8+8+8+4+4+1 = 25 bytes total.
//
// All fields are plain primitives (double / float / packed byte) -- no registry/ItemStack/
// Component/Holder/NBT -- fully representable by the certified PacketBuffer (FriendlyByteBuf)
// port. We build each packet via the canonical public ctor
//   PosRot(double x, double y, double z, float yRot, float xRot, boolean onGround, boolean horizontalCollision)
// (ServerboundMovePlayerPacket.java:153-157) so we can pin arbitrary field values, then encode
// every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). x/y/z are emitted as %016x of their raw long bits
// (Double.doubleToRawLongBits) and yRot/xRot as %08x of their raw int bits
// (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly without parse rounding;
// onGround/horizontalCollision as decimal 0/1; hex columns are lowercase hex:
//   ENC <xBits> <yBits> <zBits> <yRotBits> <xRotBits> <onGround> <horizColl> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every body byte.
//   DEC <hexBytes> <xBits_in> <yBits_in> <zBits_in> <yRotBits_in> <xRotBits_in> <onGround_in> <horizColl_in> \
//       <xBits_dec> <yBits_dec> <zBits_dec> <yRotBits_dec> <xRotBits_dec> <onGround_dec> <horizColl_dec>
//        decode: STREAM_CODEC.decode(buf) -> field round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundMovePlayerPacket;

public class PktMovePlayerPosRotParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, PosRot).
        StreamCodec<FriendlyByteBuf, ServerboundMovePlayerPacket.PosRot> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundMovePlayerPacket.PosRot>)
                ServerboundMovePlayerPacket.PosRot.STREAM_CODEC;

        // Finite/physical battery of position doubles: zero/sign, unit, typical world
        // coordinates, world-border extremes, plus special IEEE-754 doubles to exercise
        // the full 8-byte encoding.
        double[] coords = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
            64.0, 70.0, 256.0, -64.0,
            123.456, -123.456, 1000000.5, -1000000.5,
            29999984.0, -29999984.0,            // world-border extent
            3.141592653589793, 2.718281828459045,
            Double.MIN_VALUE, Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.NaN,
        };
        // Finite/physical battery of rotation floats: -180..360 plus sign/zero/boundary/
        // special IEEE-754 floats.
        float[] rots = {
            0.0f, -0.0f, 1.0f, -1.0f, 45.0f, 90.0f, 180.0f, -180.0f, 270.0f, 360.0f,
            0.5f, 3.14159265f, 89.9f, -89.9f, 123.456f, -123.456f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };
        boolean[] bools = { false, true };

        // Pair the coordinate/rotation lists index-wise so every bit pattern is hit in
        // each slot, and cross all 4 boolean combos (every flags byte 0..3).
        int n = Math.max(coords.length, rots.length);
        for (int i = 0; i < n; i++) {
            double x = coords[i % coords.length];
            double y = coords[(i + 1) % coords.length];
            double z = coords[(coords.length - 1 - (i % coords.length)) % coords.length];
            float yRot = rots[i % rots.length];
            float xRot = rots[(rots.length - 1 - (i % rots.length)) % rots.length];
            for (boolean onGround : bools) {
                for (boolean horizontalCollision : bools) {
                    ServerboundMovePlayerPacket.PosRot pkt =
                        new ServerboundMovePlayerPacket.PosRot(x, y, z, yRot, xRot, onGround, horizontalCollision);

                    long xBits = Double.doubleToRawLongBits(x);
                    long yBits = Double.doubleToRawLongBits(y);
                    long zBits = Double.doubleToRawLongBits(z);
                    int yRotBits = Float.floatToRawIntBits(yRot);
                    int xRotBits = Float.floatToRawIntBits(xRot);

                    // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                    FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                    CODEC.encode(buf, pkt);
                    int rb = buf.readableBytes();
                    String hex = toHex(buf);
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
                    O.print(horizontalCollision ? 1 : 0);
                    O.print('\t');
                    O.print(rb);
                    O.print('\t');
                    O.print(hex);
                    O.print('\n');

                    // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                    FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ServerboundMovePlayerPacket.PosRot dec = CODEC.decode(rbuf);
                    long dxBits = Double.doubleToRawLongBits(dec.getX(Double.NaN));
                    long dyBits = Double.doubleToRawLongBits(dec.getY(Double.NaN));
                    long dzBits = Double.doubleToRawLongBits(dec.getZ(Double.NaN));
                    int dyRotBits = Float.floatToRawIntBits(dec.getYRot(Float.NaN));
                    int dxRotBits = Float.floatToRawIntBits(dec.getXRot(Float.NaN));
                    boolean dOn = dec.isOnGround();
                    boolean dHoriz = dec.horizontalCollision();
                    if (dxBits != xBits || dyBits != yBits || dzBits != zBits
                        || dyRotBits != yRotBits || dxRotBits != xRotBits
                        || dOn != onGround || dHoriz != horizontalCollision)
                        throw new IllegalStateException("round-trip mismatch for " + hex);
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
                    O.print(horizontalCollision ? 1 : 0);
                    O.print('\t');
                    O.print(String.format("%016x", dxBits));
                    O.print('\t');
                    O.print(String.format("%016x", dyBits));
                    O.print('\t');
                    O.print(String.format("%016x", dzBits));
                    O.print('\t');
                    O.print(String.format("%08x", dyRotBits));
                    O.print('\t');
                    O.print(String.format("%08x", dxRotBits));
                    O.print('\t');
                    O.print(dOn ? 1 : 0);
                    O.print('\t');
                    O.print(dHoriz ? 1 : 0);
                    O.print('\n');
                }
            }
        }
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
