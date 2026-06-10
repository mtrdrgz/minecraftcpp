// Ground truth for net.minecraft.network.protocol.game.ClientboundAnimatePacket's
// StreamCodec.
//
// The packet body is exactly (ClientboundAnimatePacket.java:31-34):
//   write : FriendlyByteBuf.writeVarInt(this.id)    // entity id
//           FriendlyByteBuf.writeByte(this.action)  // low 8 bits of the int action
//   read  (ClientboundAnimatePacket.java:26-29):
//           this.id     = input.readVarInt();
//           this.action = input.readUnsignedByte();  // 0..255
// STREAM_CODEC = Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember,
// so NO packet-id prefix on the wire, just the body: VarInt(id) then 1 byte(action).
//
// Both fields are plain numbers; no registry/ItemStack/Component/Holder/NBT — fully
// representable by the certified PacketBuffer (FriendlyByteBuf) port. The public ctor
// takes an Entity (to read getId()); we instead drive the private FriendlyByteBuf
// decode ctor by reflection so we can pin arbitrary (id, action) pairs, then encode
// every packet through the REAL STREAM_CODEC.
//
// Row formats (tab separated). id/action are decimal; hex columns are lowercase hex:
//   ENC <id> <action_in> <readableBytes> <hexBytes>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + every byte.
//   DEC <hexBytes> <id_in> <action_in> <id_decoded> <action_decoded>
//        decode: STREAM_CODEC.decode(buf) -> getId()/getAction() round-trip sanity.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.lang.reflect.Constructor;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundAnimatePacket;

public class PktAnimateCbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundAnimatePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundAnimatePacket>)
                ClientboundAnimatePacket.STREAM_CODEC;

        // The private decode ctor ClientboundAnimatePacket(FriendlyByteBuf): we feed it
        // a buffer pre-loaded with VarInt(id)+byte(action) to construct a packet whose
        // id/action are exactly what we want, then re-encode through the codec.
        Constructor<ClientboundAnimatePacket> dctor =
            ClientboundAnimatePacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        dctor.setAccessible(true);

        // Finite/physical battery.
        // entity ids: 0, small, VarInt 1->2->3->4->5-byte boundaries, max/min int.
        int[] ids = {
            0, 1, 2, 5, 100, 127, 128, 255, 256,
            16383, 16384, 2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };
        // actions: the documented constants + full unsigned-byte boundary battery.
        // (write low 8 bits of action; read as unsigned byte 0..255.)
        int[] actions = {
            ClientboundAnimatePacket.SWING_MAIN_HAND,   // 0
            1,
            ClientboundAnimatePacket.WAKE_UP,           // 2
            ClientboundAnimatePacket.SWING_OFF_HAND,    // 3
            ClientboundAnimatePacket.CRITICAL_HIT,      // 4
            ClientboundAnimatePacket.MAGIC_CRITICAL_HIT,// 5
            127, 128, 200, 255,
        };

        for (int id : ids) {
            for (int action : actions) {
                // Build a packet with this exact (id, action) via the private decode ctor.
                FriendlyByteBuf seed = new FriendlyByteBuf(Unpooled.buffer());
                seed.writeVarInt(id);
                seed.writeByte(action);
                ClientboundAnimatePacket pkt = dctor.newInstance(seed);

                // ENC: encode through the REAL codec, dump readableBytes + body bytes.
                FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                CODEC.encode(buf, pkt);
                int n = buf.readableBytes();
                String hex = toHex(buf);
                O.print("ENC\t");
                O.print(id);
                O.print('\t');
                O.print(action);
                O.print('\t');
                O.print(n);
                O.print('\t');
                O.print(hex);
                O.print('\n');

                // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                ClientboundAnimatePacket dec = CODEC.decode(rbuf);
                if (dec.getId() != id) throw new IllegalStateException("id round-trip " + id + " -> " + dec.getId());
                // action is masked to its low 8 bits on the wire then read unsigned.
                int expAction = action & 0xff;
                if (dec.getAction() != expAction)
                    throw new IllegalStateException("action round-trip " + action + " -> " + dec.getAction());
                O.print("DEC\t");
                O.print(hex);
                O.print('\t');
                O.print(id);
                O.print('\t');
                O.print(action);
                O.print('\t');
                O.print(dec.getId());
                O.print('\t');
                O.print(dec.getAction());
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
