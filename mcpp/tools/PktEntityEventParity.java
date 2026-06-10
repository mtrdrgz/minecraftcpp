// Ground truth for net.minecraft.network.protocol.game.ClientboundEntityEventPacket's
// StreamCodec.
//
// The packet body is exactly (ClientboundEntityEventPacket.java:23-31):
//   write : FriendlyByteBuf.writeInt(this.entityId)  // entity id, BIG-ENDIAN 4 bytes
//           FriendlyByteBuf.writeByte(this.eventId)   // signed byte (low 8 bits)
//   read  : this.entityId = input.readInt();          // BE 4 bytes -> signed int
//           this.eventId  = input.readByte();         // signed byte -128..127
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: int(entityId) then 1 byte(eventId).
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. The public ctor
// takes an Entity (to read getId()); we instead drive the private FriendlyByteBuf decode
// ctor by reflection so we can pin arbitrary (entityId, eventId) pairs, then encode every
// packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). entityId/eventId are decimal; hex columns are lowercase hex:
//   ENC <entityId> <eventId> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <entityId_in> <eventId_in> <entityId_decoded> <eventId_decoded>
//        decode: STREAM_CODEC.decode(buf) -> entityId/getEventId() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundEntityEventPacket;

public class PktEntityEventParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundEntityEventPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundEntityEventPacket>)
                ClientboundEntityEventPacket.STREAM_CODEC;

        // The private decode ctor ClientboundEntityEventPacket(FriendlyByteBuf): we feed
        // it a buffer pre-loaded with int(entityId)+byte(eventId) to construct a packet
        // whose fields are exactly what we want, then re-encode through the codec.
        Constructor<ClientboundEntityEventPacket> dctor =
            ClientboundEntityEventPacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        dctor.setAccessible(true);

        // entityId field is private; read it back for round-trip sanity.
        Field fEntityId = ClientboundEntityEventPacket.class.getDeclaredField("entityId");
        fEntityId.setAccessible(true);

        // Finite/physical battery.
        // entity ids: 0, small, sign boundaries, BE 4-byte boundaries, max/min int.
        int[] entityIds = {
            0, 1, 2, 5, 100, 127, 128, 255, 256,
            16383, 16384, 65535, 65536, 16777215, 16777216,
            2097151, 2097152, 268435455, 268435456,
            -1, -128, -256, -65536, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };
        // eventId: a Java signed byte. Full signed-byte boundary battery + a few of the
        // documented EntityEvent constants live in this range anyway (e.g. 2=hurt, 3=death).
        int[] eventIds = {
            0, 1, 2, 3, 4, 7, 10, 60, 100, 127,
            -1, -2, -7, -128, 200, 255, // 200/255 exercise low-8-bits truncation to signed
        };

        for (int entityId : entityIds) {
            for (int ev : eventIds) {
                byte eventId = (byte) ev; // low 8 bits, interpreted signed (matches wire)

                // Build a packet with this exact (entityId, eventId) via the decode ctor.
                FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
                seed.writeInt(entityId);
                seed.writeByte(eventId);
                ClientboundEntityEventPacket pkt = dctor.newInstance(seed);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                O.print("ENC\t");
                O.print(entityId);
                O.print('\t');
                O.print((int) eventId); // emit the actual signed byte value used
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundEntityEventPacket dec = CODEC.decode(rbuf);
                int decEntityId = fEntityId.getInt(dec);
                byte decEventId = dec.getEventId();
                if (decEntityId != entityId)
                    throw new IllegalStateException("entityId round-trip " + entityId + " -> " + decEntityId);
                if (decEventId != eventId)
                    throw new IllegalStateException("eventId round-trip " + eventId + " -> " + decEventId);
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(entityId);
                O.print('\t');
                O.print((int) eventId);
                O.print('\t');
                O.print(decEntityId);
                O.print('\t');
                O.print((int) decEventId);
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
