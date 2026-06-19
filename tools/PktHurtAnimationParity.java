// Ground truth for net.minecraft.network.protocol.game.ClientboundHurtAnimationPacket's
// StreamCodec.
//
// The packet is a record(int id, float yaw) and its body is exactly
// (ClientboundHurtAnimationPacket.java:18-25):
//   write : FriendlyByteBuf.writeVarInt(this.id)     // entity id (VarInt / LEB128)
//           FriendlyByteBuf.writeFloat(this.yaw)     // big-endian IEEE-754 4 bytes
//   read  : this.id  = input.readVarInt();
//           this.yaw = input.readFloat();
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: VarInt(id) then 4-byte float(yaw).
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. The public ctor
// takes a LivingEntity (to read getId()/getHurtDir()); we instead use the canonical
// record ctor (int, float) directly so we can pin arbitrary (id, yaw) pairs, then
// encode every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). id is decimal; yaw is emitted as %08x of its raw
// int bits (Float.floatToRawIntBits) so NaN/Inf/-0.0 survive exactly; hex columns
// are lowercase hex:
//   ENC <id> <yawRawBitsHex> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <id_in> <yawRawBitsHex_in> <id_decoded> <yawRawBitsHex_decoded>
//        decode: STREAM_CODEC.decode(buf) -> id()/yaw() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundHurtAnimationPacket;

public class PktHurtAnimationParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundHurtAnimationPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundHurtAnimationPacket>)
                ClientboundHurtAnimationPacket.STREAM_CODEC;

        // Finite/physical battery.
        // entity ids: 0, small, VarInt 1->2->3->4->5-byte boundaries, max/min int.
        int[] ids = {
            0, 1, 2, 5, 100, 127, 128, 255, 256,
            16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };
        // yaw: the physical hurt-direction range is 0..360 degrees, plus sign / zero /
        // boundary / special IEEE-754 floats to exercise the full 4-byte encoding.
        float[] yaws = {
            0.0f, -0.0f, 1.0f, -1.0f, 45.0f, 90.0f, 180.0f, 270.0f, 360.0f,
            0.5f, 3.14159265f, 123.456f, -123.456f,
            Float.MIN_VALUE, Float.MAX_VALUE, -Float.MAX_VALUE,
            Float.POSITIVE_INFINITY, Float.NEGATIVE_INFINITY, Float.NaN,
        };

        for (int id : ids) {
            for (float yaw : yaws) {
                // Build a packet with this exact (id, yaw) via the canonical record ctor.
                ClientboundHurtAnimationPacket pkt = new ClientboundHurtAnimationPacket(id, yaw);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                int yawBits = Float.floatToRawIntBits(yaw);
                O.print("ENC\t");
                O.print(id);
                O.print('\t');
                O.print(String.format("%08x", yawBits));
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundHurtAnimationPacket dec = CODEC.decode(rbuf);
                if (dec.id() != id)
                    throw new IllegalStateException("id round-trip " + id + " -> " + dec.id());
                int decYawBits = Float.floatToRawIntBits(dec.yaw());
                if (decYawBits != yawBits)
                    throw new IllegalStateException("yaw round-trip " + yawBits + " -> " + decYawBits);
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(id);
                O.print('\t');
                O.print(String.format("%08x", yawBits));
                O.print('\t');
                O.print(dec.id());
                O.print('\t');
                O.print(String.format("%08x", decYawBits));
                O.print('\n');
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
