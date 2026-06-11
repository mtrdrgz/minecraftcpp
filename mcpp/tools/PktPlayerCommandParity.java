// Ground truth for net.minecraft.network.protocol.game.ServerboundPlayerCommandPacket's
// StreamCodec.
//
// The packet body is exactly (ServerboundPlayerCommandPacket.java lines 27-37):
//   write : FriendlyByteBuf.writeVarInt(this.id)
//           FriendlyByteBuf.writeEnum(this.action)  = writeVarInt(action.ordinal())
//           FriendlyByteBuf.writeVarInt(this.data)
//   read  : this.id     = input.readVarInt();
//           this.action = input.readEnum(Action.class)  = getEnumConstants()[readVarInt()]
//           this.data   = input.readVarInt();
// (FriendlyByteBuf.writeEnum 471-473 / readEnum 467-469.) STREAM_CODEC =
// Packet.codec(write, new(FriendlyByteBuf)) -> StreamCodec.ofMember, so NO packet-id
// prefix on the wire, just the body: VarInt(id), VarInt(action.ordinal()), VarInt(data).
//
// All three fields are fully representable by the certified PacketBuffer (FriendlyByteBuf)
// port; no registry/ItemStack/Component/Holder/NBT. The public ctor takes an Entity, so to
// pin arbitrary (id, ordinal, data) tuples we build a body and run it through the REAL
// decode codec (guaranteeing the packet is exactly what the wire would yield), then encode
// it back through the REAL STREAM_CODEC.
//
// Action declaration order (ServerboundPlayerCommandPacket.java 60-67):
//   STOP_SLEEPING=0, START_SPRINTING=1, STOP_SPRINTING=2, START_RIDING_JUMP=3,
//   STOP_RIDING_JUMP=4, OPEN_INVENTORY=5, START_FALL_FLYING=6
//
// Row formats (tab separated):
//   ENUM <ordinal> <name>                          per Action constant (enum gate)
//   ENC  <id> <ordinal> <data> <readableBytes> <hex>
//        encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + body bytes.
//   DEC  <hex> <id_in> <ordinal_in> <data_in> <id_out> <ordinal_out> <data_out>
//        decode: STREAM_CODEC.decode(buf) -> getId()/getAction().ordinal()/getData().
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPlayerCommandPacket;

public class PktPlayerCommandParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundPlayerCommandPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPlayerCommandPacket>)
                ServerboundPlayerCommandPacket.STREAM_CODEC;

        // Enum gate: dump ordinal()+name() for every Action constant in declaration order.
        ServerboundPlayerCommandPacket.Action[] actions =
            ServerboundPlayerCommandPacket.Action.values();
        for (ServerboundPlayerCommandPacket.Action a : actions) {
            O.print("ENUM\t");
            O.print(a.ordinal());
            O.print('\t');
            O.print(a.name());
            O.print('\n');
        }

        // VarInt battery for id and data: zero/sign/byte-boundaries/max-min.
        int[] ids = {
            0, 1, 2, 127, 128, 255, 256, 16383, 16384,
            2097151, 2097152, 268435455, 268435456,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };
        int[] datas = {
            0, 1, 42, 127, 128, 300, 16384,
            -1, Integer.MAX_VALUE, Integer.MIN_VALUE,
        };

        for (ServerboundPlayerCommandPacket.Action a : actions) {
            int ord = a.ordinal();
            for (int id : ids) {
                for (int data : datas) {
                    // Build the packet carrying exactly (id, ord, data) by decoding a
                    // hand-built body through the REAL codec.
                    ServerboundPlayerCommandPacket pkt = make(CODEC, id, ord, data);

                    // ENC: encode through the REAL codec; dump readableBytes + body bytes.
                    FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                    CODEC.encode(buf, pkt);
                    int n = buf.readableBytes();
                    String hex = toHex(buf);
                    O.print("ENC\t");
                    O.print(id);   O.print('\t');
                    O.print(ord);  O.print('\t');
                    O.print(data); O.print('\t');
                    O.print(n);    O.print('\t');
                    O.print(hex);  O.print('\n');

                    // DEC: decode the same bytes through the REAL codec; round-trip sanity.
                    FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ServerboundPlayerCommandPacket dec = CODEC.decode(rbuf);
                    if (dec.getId() != id)
                        throw new IllegalStateException("id round-trip " + id + " -> " + dec.getId());
                    if (dec.getAction().ordinal() != ord)
                        throw new IllegalStateException("action round-trip " + ord + " -> " + dec.getAction().ordinal());
                    if (dec.getData() != data)
                        throw new IllegalStateException("data round-trip " + data + " -> " + dec.getData());
                    O.print("DEC\t");
                    O.print(hex);  O.print('\t');
                    O.print(id);   O.print('\t');
                    O.print(ord);  O.print('\t');
                    O.print(data); O.print('\t');
                    O.print(dec.getId());               O.print('\t');
                    O.print(dec.getAction().ordinal()); O.print('\t');
                    O.print(dec.getData());             O.print('\n');
                }
            }
        }
    }

    // Build a ServerboundPlayerCommandPacket carrying exactly (id, ordinal, data) by
    // decoding a hand-built body through the REAL codec
    // (VarInt id + VarInt enum-ordinal + VarInt data). Guarantees the produced packet is
    // exactly what the codec would yield from the wire.
    static ServerboundPlayerCommandPacket make(
            StreamCodec<FriendlyByteBuf, ServerboundPlayerCommandPacket> codec,
            int id, int ordinal, int data) throws Exception {
        FriendlyByteBuf b = new FriendlyByteBuf(Unpooled.buffer());
        b.writeVarInt(id);
        b.writeVarInt(ordinal);
        b.writeVarInt(data);
        return codec.decode(b);
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
