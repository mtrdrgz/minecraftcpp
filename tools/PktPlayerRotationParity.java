// Ground truth for net.minecraft.network.protocol.game.ClientboundPlayerRotationPacket's
// StreamCodec.
//
// The packet is a record(float yRot, boolean relativeY, float xRot, boolean relativeX)
// (ClientboundPlayerRotationPacket.java:9-20). Its STREAM_CODEC is StreamCodec.composite
// of, IN THIS EXACT ORDER:
//   ByteBufCodecs.FLOAT -> yRot       // big-endian IEEE-754 4 bytes  (writeFloat)
//   ByteBufCodecs.BOOL  -> relativeY  // 1 byte 0/1                   (writeBoolean)
//   ByteBufCodecs.FLOAT -> xRot       // big-endian IEEE-754 4 bytes  (writeFloat)
//   ByteBufCodecs.BOOL  -> relativeX  // 1 byte 0/1                   (writeBoolean)
// (ByteBufCodecs.FLOAT == input.readFloat()/output.writeFloat(); ByteBufCodecs.BOOL ==
//  input.readBoolean()/output.writeBoolean() -- ByteBufCodecs.java:56-64,132-140.)
// So the on-wire body is exactly: float(yRot) ++ bool(relativeY) ++ float(xRot) ++
// bool(relativeX), 10 bytes total, NO packet-id prefix (StreamCodec.composite).
//
// All four fields are plain primitives (float / boolean) -- no registry/ItemStack/
// Component/Holder/NBT -- fully representable by the certified PacketBuffer
// (FriendlyByteBuf) port. We build each packet via the canonical public record ctor
// (float, boolean, float, boolean) so we can pin arbitrary field values, then encode
// every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). yRot/xRot are emitted as %08x of their raw int bits
// (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly; relativeY/relativeX as
// decimal 0/1; hex columns are lowercase hex:
//   ENC <yRotRawBitsHex> <relativeY> <xRotRawBitsHex> <relativeX> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <yRotBits_in> <relY_in> <xRotBits_in> <relX_in> \
//       <yRotBits_dec> <relY_dec> <xRotBits_dec> <relX_dec>
//        decode: STREAM_CODEC.decode(buf) -> field round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundPlayerRotationPacket;

public class PktPlayerRotationParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundPlayerRotationPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundPlayerRotationPacket>)
                ClientboundPlayerRotationPacket.STREAM_CODEC;

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

        // Cross yRot x relativeY x xRot x relativeX would be huge; pair the float lists
        // index-wise for yRot/xRot and cross all 4 boolean combos -- this still covers
        // every float bit pattern in both float slots and all 0/1 boolean combos.
        for (int i = 0; i < rots.length; i++) {
            float yRot = rots[i];
            float xRot = rots[rots.length - 1 - i]; // reversed pairing
            for (boolean relativeY : bools) {
                for (boolean relativeX : bools) {
                    ClientboundPlayerRotationPacket pkt =
                        new ClientboundPlayerRotationPacket(yRot, relativeY, xRot, relativeX);

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
                    O.print(relativeY ? 1 : 0);
                    O.print('\t');
                    O.print(String.format("%08x", xBits));
                    O.print('\t');
                    O.print(relativeX ? 1 : 0);
                    O.print('\t');
                    O.print(n);
                    O.print('\t');
                    O.print(hex);
                    O.print('\n');

                    // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                    FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ClientboundPlayerRotationPacket dec = CODEC.decode(rbuf);
                    int dyBits = Float.floatToRawIntBits(dec.yRot());
                    int dxBits = Float.floatToRawIntBits(dec.xRot());
                    if (dyBits != yBits || dxBits != xBits
                        || dec.relativeY() != relativeY || dec.relativeX() != relativeX)
                        throw new IllegalStateException("round-trip mismatch for " + hex);
                    O.print("DEC\t");
                    O.print(hex);
                    O.print('\t');
                    O.print(String.format("%08x", yBits));
                    O.print('\t');
                    O.print(relativeY ? 1 : 0);
                    O.print('\t');
                    O.print(String.format("%08x", xBits));
                    O.print('\t');
                    O.print(relativeX ? 1 : 0);
                    O.print('\t');
                    O.print(String.format("%08x", dyBits));
                    O.print('\t');
                    O.print(dec.relativeY() ? 1 : 0);
                    O.print('\t');
                    O.print(String.format("%08x", dxBits));
                    O.print('\t');
                    O.print(dec.relativeX() ? 1 : 0);
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
