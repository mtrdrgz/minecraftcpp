// Ground truth for net.minecraft.network.protocol.game.ServerboundMovePlayerPacket.Pos's
// StreamCodec.
//
// ServerboundMovePlayerPacket.Pos (ServerboundMovePlayerPacket.java:108-142) carries:
//   double x, double y, double z, boolean onGround, boolean horizontalCollision.
// Its STREAM_CODEC is Packet.codec(Pos::write, Pos::read). The write() method
// (ServerboundMovePlayerPacket.java:131-136) emits, IN THIS EXACT ORDER:
//   output.writeDouble(this.x)   // big-endian IEEE-754 8 bytes   (writeDouble)
//   output.writeDouble(this.y)   // big-endian IEEE-754 8 bytes   (writeDouble)
//   output.writeDouble(this.z)   // big-endian IEEE-754 8 bytes   (writeDouble)
//   output.writeByte(packFlags(onGround, horizontalCollision))  // 1 byte
// where packFlags (ServerboundMovePlayerPacket.java:22-33) is:
//   flags = 0; if (onGround) flags |= 1; if (horizontalCollision) flags |= 2;
// So the on-wire body is exactly: double(x) ++ double(y) ++ double(z) ++ byte(flags),
// 25 bytes total, NO packet-id prefix (Packet.codec body codec). Netty's
// ByteBuf.writeByte(int) writes only the low 8 bits, and flags is always 0..3 here.
//
// Every field is a plain primitive (double / boolean) -- no registry/ItemStack/
// Component/Holder/NBT -- fully representable by the certified PacketBuffer
// (FriendlyByteBuf) port. We build each packet via the public ctor
// Pos(double x, double y, double z, boolean onGround, boolean horizontalCollision)
// (ServerboundMovePlayerPacket.java:117-119) so we can pin arbitrary field values,
// then encode every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). x/y/z are emitted as %016x of their raw long bits
// (Double.doubleToRawLongBits) so NaN/Inf/-0.0 survive exactly; onGround/horizontalCollision
// as decimal 0/1; hex columns are lowercase hex:
//   ENC <xBitsHex> <yBitsHex> <zBitsHex> <onGround> <horizColl> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <xBits_in> <yBits_in> <zBits_in> <onGround_in> <hc_in> \
//       <xBits_dec> <yBits_dec> <zBits_dec> <onGround_dec> <hc_dec>
//        decode: STREAM_CODEC.decode(buf) -> field round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundMovePlayerPacket;

public class PktMovePlayerPosParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet subtype (FriendlyByteBuf, Pos).
        StreamCodec<FriendlyByteBuf, ServerboundMovePlayerPacket.Pos> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundMovePlayerPacket.Pos>)
                ServerboundMovePlayerPacket.Pos.STREAM_CODEC;

        // Finite/physical battery of doubles: world-coordinate range, sign / zero /
        // boundary / special IEEE-754 doubles to exercise the full 8-byte encoding.
        double[] coords = {
            0.0, -0.0, 1.0, -1.0, 0.5, -0.5,
            64.0, 128.5, 256.25, -256.25, 1000.0, -1000.0,
            3.141592653589793, 2.718281828459045,
            30000000.0, -30000000.0,              // world border extents
            12345.678901234, -98765.432109876,
            Double.MIN_VALUE, Double.MAX_VALUE, -Double.MAX_VALUE,
            Double.POSITIVE_INFINITY, Double.NEGATIVE_INFINITY, Double.NaN,
        };
        boolean[] bools = { false, true };

        // Pair the coord list index-wise across the x/y/z slots (distinct rotations) so
        // every double bit pattern lands in each slot, and cross all 4 boolean combos.
        int n = coords.length;
        for (int i = 0; i < n; i++) {
            double x = coords[i];
            double y = coords[(i + 1) % n];
            double z = coords[(n - 1 - i)];
            for (boolean onGround : bools) {
                for (boolean horizontalCollision : bools) {
                    ServerboundMovePlayerPacket.Pos pkt =
                        new ServerboundMovePlayerPacket.Pos(x, y, z, onGround, horizontalCollision);

                    // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                    FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                    CODEC.encode(buf, pkt);
                    int rn = buf.readableBytes();
                    String hex = toHex(buf);
                    long xBits = Double.doubleToRawLongBits(x);
                    long yBits = Double.doubleToRawLongBits(y);
                    long zBits = Double.doubleToRawLongBits(z);
                    O.print("ENC\t");
                    O.print(String.format("%016x", xBits));
                    O.print('\t');
                    O.print(String.format("%016x", yBits));
                    O.print('\t');
                    O.print(String.format("%016x", zBits));
                    O.print('\t');
                    O.print(onGround ? 1 : 0);
                    O.print('\t');
                    O.print(horizontalCollision ? 1 : 0);
                    O.print('\t');
                    O.print(rn);
                    O.print('\t');
                    O.print(hex);
                    O.print('\n');

                    // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                    FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ServerboundMovePlayerPacket.Pos dec = CODEC.decode(rbuf);
                    long dxBits = Double.doubleToRawLongBits(dec.getX(Double.NaN));
                    long dyBits = Double.doubleToRawLongBits(dec.getY(Double.NaN));
                    long dzBits = Double.doubleToRawLongBits(dec.getZ(Double.NaN));
                    boolean dOnGround = dec.isOnGround();
                    boolean dHc = dec.horizontalCollision();
                    if (dxBits != xBits || dyBits != yBits || dzBits != zBits
                        || dOnGround != onGround || dHc != horizontalCollision)
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
                    O.print(dOnGround ? 1 : 0);
                    O.print('\t');
                    O.print(dHc ? 1 : 0);
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
