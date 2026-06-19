// Ground truth for net.minecraft.network.protocol.game.ClientboundRotateHeadPacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet body is exactly (ClientboundRotateHeadPacket.java:29-32):
//   write(FriendlyByteBuf output):
//     output.writeVarInt(this.entityId);   // VarInt entity id
//     output.writeByte(this.yHeadRot);     // a single signed byte (the packed rotation)
//   read(FriendlyByteBuf input)  (ClientboundRotateHeadPacket.java:24-27):
//     this.entityId = input.readVarInt();
//     this.yHeadRot = input.readByte();    // SIGNED byte (-128..127), NOT readUnsignedByte
//
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember
// (Packet.java:22-24): body ONLY, NO packet-id or length prefix on the wire.
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. The public ctor
// takes an Entity (to read getId()); we instead drive the private FriendlyByteBuf
// decode ctor by reflection so we can pin arbitrary (entityId, yHeadRot) pairs, then
// encode every packet through the REAL STREAM_CODEC.
//
// FriendlyByteBuf.writeByte(int) writes the low 8 bits; the field is a Java `byte`
// (already -128..127), so the written byte equals (yHeadRot & 0xff). On read,
// readByte() yields the SIGNED byte back, so yHeadRot round-trips exactly.
//
// Row formats (tab separated). entityId and yHeadRot are decimal; the hex column is
// lowercase %02x of the encoded body (there are no String/binary fields here):
//   ENC \t <name> \t <entityId> \t <yHeadRot> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeVarInt(entityId) + writeByte(yHeadRot & 0xff)) and must match byte-for-byte,
// then round-trips the bytes back to (entityId, yHeadRot).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundRotateHeadPacket;

public class PktRotateHeadParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundRotateHeadPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundRotateHeadPacket>)
                ClientboundRotateHeadPacket.STREAM_CODEC;

        // The private decode ctor ClientboundRotateHeadPacket(FriendlyByteBuf): we feed
        // it a buffer pre-loaded with VarInt(entityId)+byte(yHeadRot) to construct a
        // packet whose fields are exactly what we want, then re-encode through the codec.
        Constructor<ClientboundRotateHeadPacket> dctor =
            ClientboundRotateHeadPacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        dctor.setAccessible(true);

        // Reflectively read the private final fields for round-trip sanity assertions.
        Field fId = ClientboundRotateHeadPacket.class.getDeclaredField("entityId");
        fId.setAccessible(true);
        Field fRot = ClientboundRotateHeadPacket.class.getDeclaredField("yHeadRot");
        fRot.setAccessible(true);

        // Finite/physical battery.
        // entity ids: 0, small, VarInt 1->2->3->4->5-byte boundaries, max/min int.
        int[] ids = {
            0, 1, 2, 5, 100, 127, 128, 255, 256,
            16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };
        // yHeadRot is a Java byte: full signed-byte boundary battery -128..127.
        int[] rots = {
            0, 1, -1, 63, 64, -64, 127, -128, 100, -100, 42, -42,
        };

        for (int id : ids) {
            for (int rotI : rots) {
                byte rot = (byte) rotI;  // physical: yHeadRot is a byte field (-128..127).

                // Build a packet with this exact (entityId, yHeadRot) via the private
                // decode ctor: seed buffer = VarInt(id) + byte(rot).
                FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
                seed.writeVarInt(id);
                seed.writeByte(rot);
                ClientboundRotateHeadPacket pkt = dctor.newInstance(seed);

                // Sanity: the constructed packet holds exactly what we seeded.
                int gotId = fId.getInt(pkt);
                byte gotRot = fRot.getByte(pkt);
                if (gotId != id)
                    throw new IllegalStateException("ctor entityId " + id + " -> " + gotId);
                if (gotRot != rot)
                    throw new IllegalStateException("ctor yHeadRot " + rot + " -> " + gotRot);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int readable = buf.readableBytes();
                String hex = toHex(buf);

                // Round-trip decode through the SAME codec; assert field equality.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundRotateHeadPacket dec = CODEC.decode(rbuf);
                int decId = fId.getInt(dec);
                byte decRot = fRot.getByte(dec);
                if (decId != id)
                    throw new IllegalStateException("entityId round-trip " + id + " -> " + decId);
                if (decRot != rot)
                    throw new IllegalStateException("yHeadRot round-trip " + rot + " -> " + decRot);

                String name = "case_id" + id + "_rot" + rotI;
                O.print("ENC\t");
                O.print(name);
                O.print('\t');
                O.print(id);
                O.print('\t');
                O.print(rotI);
                O.print('\t');
                O.print(readable);
                O.print('\t');
                O.print(hex);
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
