// Ground truth for net.minecraft.network.protocol.game.ClientboundSetCameraPacket.
//
// The packet body is exactly one FriendlyByteBuf.writeVarInt(cameraId) on encode
// and readVarInt() on decode (ClientboundSetCameraPacket lines 21-27 in 26.1.2/src):
//
//     private ClientboundSetCameraPacket(final FriendlyByteBuf input) {
//        this.cameraId = input.readVarInt();
//     }
//     private void write(final FriendlyByteBuf output) {
//        output.writeVarInt(this.cameraId);
//     }
//
// STREAM_CODEC = Packet.codec(write, ctor) -> StreamCodec.ofMember: body only, no
// packet-id prefix. cameraId is a raw entity id (a plain int): writeVarInt is plain
// LEB128 (NOT zig-zag), so negative ids encode to five bytes.
//
// The packet's only ctor that sets cameraId from an int is the private
// FriendlyByteBuf decode ctor; cameraId is also private. We therefore build each
// physical case by decoding a freshly-encoded VarInt(cameraId) through the REAL
// STREAM_CODEC.decode, then re-encode through the REAL STREAM_CODEC.encode and dump
// the exact wire bytes. We also assert the decode->encode round-trip is stable, and
// read cameraId back via reflection to sanity-check it equals the input.
//
// Row format (tab separated):
//   ENC <cameraId-dec> <hex>      encode the real packet -> body bytes
//   DEC <hex> <cameraId-dec>      decode the bytes -> recovered cameraId (== input)
//
// The C++ pkt_set_camera_parity rebuilds the body via PacketBuffer.writeVarInt and
// must match <hex> byte-for-byte (ENC), and PacketBuffer.readVarInt(<hex>) must
// recover cameraId (DEC).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Field;

import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundSetCameraPacket;

public class PktSetCameraParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundSetCameraPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundSetCameraPacket>)
                ClientboundSetCameraPacket.STREAM_CODEC;

        // cameraId is private; read it back for the round-trip sanity assert.
        Field idField = ClientboundSetCameraPacket.class.getDeclaredField("cameraId");
        idField.setAccessible(true);

        // Finite/physical battery: real entity ids (small), plus VarInt LEB128
        // 1->2->3->4->5 byte boundaries and the int extremes. Entity ids are plain
        // ints, so negatives are legal and encode to five bytes.
        int[] cameraIds = {
            0, 1, 2, 10, 127, 128,                 // 1 -> 2 byte boundary
            16383, 16384,                          // 2 -> 3 byte boundary
            2097151, 2097152,                      // 3 -> 4 byte boundary
            268435455, 268435456,                  // 4 -> 5 byte boundary
            -1, -2, -128, -16384,                  // negatives -> always 5 bytes
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        for (int cameraId : cameraIds) {
            // Build the REAL packet by decoding VarInt(cameraId) through STREAM_CODEC.
            FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
            seed.writeVarInt(cameraId);
            ClientboundSetCameraPacket pkt = CODEC.decode(seed);

            // Sanity: the packet's private cameraId equals the input.
            int recovered = idField.getInt(pkt);
            if (recovered != cameraId)
                throw new IllegalStateException("cameraId mismatch: " + recovered + " != " + cameraId);

            // ENC: encode through the real codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            CODEC.encode(buf, pkt);
            String hex = toHex(buf);
            O.print("ENC\t");
            O.print(cameraId);
            O.print('\t');
            O.print(hex);
            O.print('\n');

            // DEC: decode the same bytes through the real codec, dump cameraId.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ClientboundSetCameraPacket dec = CODEC.decode(rbuf);
            int decId = idField.getInt(dec);
            if (decId != cameraId)
                throw new IllegalStateException("round-trip cameraId mismatch: " + decId + " != " + cameraId);
            O.print("DEC\t");
            O.print(hex);
            O.print('\t');
            O.print(cameraId);
            O.print('\n');
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
