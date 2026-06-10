// Ground truth for net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.Rot's
// StreamCodec (the rotation-only serverbound move-player subtype).
//
// ServerboundMovePlayerPacket is abstract with four concrete nested subtypes (Pos / Rot /
// PosRot / StatusOnly). This GT targets the Rot subtype
// (ServerboundMovePlayerPacket.java:186-214). Its STREAM_CODEC is
//   Packet.codec(Rot::write, Rot::read)
// and Rot.write(FriendlyByteBuf output) is, IN THIS EXACT ORDER
// (ServerboundMovePlayerPacket.java:204-208):
//   output.writeFloat(this.yRot);   // big-endian IEEE-754 4 bytes  (writeFloat)
//   output.writeFloat(this.xRot);   // big-endian IEEE-754 4 bytes  (writeFloat)
//   output.writeByte(packFlags(this.onGround, this.horizontalCollision)); // 1 byte
// where packFlags (ServerboundMovePlayerPacket.java:22-33) packs the two booleans into a
// single byte:  flags = (onGround ? 1 : 0) | (horizontalCollision ? 2 : 0).
// So the on-wire body is exactly: float(yRot) ++ float(xRot) ++ byte(flags), 9 bytes
// total, NO packet-id prefix (Packet.codec encodes the body only).
//
// NOTE on the field hint ("float yRot + float xRot + bool flags"): the trailing byte is NOT
// a single boolean -- it is a packed bitfield of TWO booleans (onGround bit 0,
// horizontalCollision bit 1). This GT models BOTH booleans and emits the exact packed byte
// the real codec writes.
//
// All fields are plain primitives (float / boolean -> packed byte) -- no registry /
// ItemStack / Component / Holder / NBT -- fully representable by the certified PacketBuffer
// (FriendlyByteBuf) port. We build each packet via the canonical public ctor
// Rot(float yRot, float xRot, boolean onGround, boolean horizontalCollision) so we can pin
// arbitrary field values, then encode every packet through the REAL STREAM_CODEC.
//
// Row format (tab separated). yRot/xRot are emitted as %08x of their raw int bits
// (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly; onGround/horizontalCollision
// as decimal 0/1; readableBytes decimal; hexBytes lowercase hex:
//   ENC <yRotRawBitsHex> <xRotRawBitsHex> <onGround> <horizontalCollision>
//       <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//        Then decode the same bytes through the SAME codec and assert field round-trip
//        (sanity, not emitted as a separate row).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundMovePlayerPacket;

public class PktMovePlayerRotParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for the Rot subtype (FriendlyByteBuf, Rot).
        StreamCodec<FriendlyByteBuf, ServerboundMovePlayerPacket.Rot> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundMovePlayerPacket.Rot>)
                ServerboundMovePlayerPacket.Rot.STREAM_CODEC;

        // Finite/physical battery of rotation floats: the physical rotation range is
        // -180..180 / 0..360 degrees, plus sign / zero / boundary / special IEEE-754
        // floats to exercise the full 4-byte encoding.
        float[] rots = {
            0.0f, -0.0f, 1.0f, -1.0f, 45.0f, 90.0f, 180.0f, -180.0f, 270.0f, 360.0f,
            0.5f, 3.14159265f, 89.9f, -89.9f, 123.456f, -123.456f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };
        boolean[] bools = { false, true };

        // Pair the float list index-wise for yRot/xRot (covers every float bit pattern in
        // both float slots) and cross all 4 (onGround x horizontalCollision) combos so the
        // packed flags byte takes every value 0..3.
        for (int i = 0; i < rots.length; i++) {
            float yRot = rots[i];
            float xRot = rots[rots.length - 1 - i]; // reversed pairing
            for (boolean onGround : bools) {
                for (boolean horizontalCollision : bools) {
                    ServerboundMovePlayerPacket.Rot pkt =
                        new ServerboundMovePlayerPacket.Rot(yRot, xRot, onGround, horizontalCollision);

                    // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                    FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                    CODEC.encode(buf, pkt);
                    int n = buf.readableBytes();
                    String hex = toHex(buf);
                    int yBits = Float.floatToRawIntBits(yRot);
                    int xBits = Float.floatToRawIntBits(xRot);

                    O.print("ENC\t");
                    O.print(String.format("%08x", yBits));
                    O.print('\t');
                    O.print(String.format("%08x", xBits));
                    O.print('\t');
                    O.print(onGround ? 1 : 0);
                    O.print('\t');
                    O.print(horizontalCollision ? 1 : 0);
                    O.print('\t');
                    O.print(n);
                    O.print('\t');
                    O.print(hex);
                    O.print('\n');

                    // Round-trip decode through the SAME codec; field-level sanity.
                    FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ServerboundMovePlayerPacket.Rot dec = CODEC.decode(rbuf);
                    int dyBits = Float.floatToRawIntBits(dec.getYRot(Float.NaN));
                    int dxBits = Float.floatToRawIntBits(dec.getXRot(Float.NaN));
                    if (dyBits != yBits || dxBits != xBits
                        || dec.isOnGround() != onGround
                        || dec.horizontalCollision() != horizontalCollision)
                        throw new IllegalStateException("round-trip mismatch for " + hex);
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
