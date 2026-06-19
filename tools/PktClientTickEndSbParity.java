// Ground truth for net.minecraft.network.protocol.game.ServerboundClientTickEndPacket.
//
// This is a UNIT / EMPTY packet. The real class is a parameterless record whose codec is:
//   public static final StreamCodec<ByteBuf, ServerboundClientTickEndPacket> STREAM_CODEC =
//       StreamCodec.unit(INSTANCE);
// (ServerboundClientTickEndPacket.java:8-10.)
//
// net.minecraft.network.codec.StreamCodec.unit (StreamCodec.java:49-61):
//   encode(output, value): asserts value.equals(instance) then writes NOTHING to output.
//   decode(input):         reads NOTHING, returns the singleton instance.
// So the wire payload is exactly ZERO bytes — no packet-id prefix (that is added by the
// protocol bundler, not the body codec), no fields. readableBytes() == 0, hex == "".
//
// Row format (tab separated):
//   ENC <name> <readableBytes-dec> <hex>     encode: STREAM_CODEC.encode(buf, INSTANCE)
//
// We round-trip-decode the empty buffer through the SAME codec and assert it yields the
// singleton INSTANCE as a sanity check before emitting. The C++ pkt_client_tick_end_sb_parity
// re-encodes via PacketBuffer (writes nothing) and must match <hex> (empty) byte-for-byte
// and readableBytes (0); it also "decodes" the empty payload (consumes nothing) and checks
// that the buffer was fully consumed.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundClientTickEndPacket;
import net.minecraft.server.Bootstrap;

public class PktClientTickEndSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet. Declared over ByteBuf; FriendlyByteBuf is a
        // ByteBuf, so it encodes/decodes against our FriendlyByteBuf fine.
        StreamCodec<ByteBuf, ServerboundClientTickEndPacket> CODEC =
            (StreamCodec<ByteBuf, ServerboundClientTickEndPacket>)
                ServerboundClientTickEndPacket.STREAM_CODEC;

        // The only physical input: the singleton INSTANCE. A unit packet has no fields and no
        // value-space to vary, so there is exactly one finite case. Emit it once.
        emit(CODEC, "instance", ServerboundClientTickEndPacket.INSTANCE);
    }

    @SuppressWarnings("deprecation")
    static void emit(StreamCodec<ByteBuf, ServerboundClientTickEndPacket> codec,
                     String name, ServerboundClientTickEndPacket pkt) throws Exception {
        // ENC: encode through the real codec, dump bytes (expected: empty).
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        codec.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: decode the (empty) payload through the SAME codec; must return INSTANCE and
        // consume nothing.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundClientTickEndPacket dec = codec.decode(rbuf);
        if (dec != ServerboundClientTickEndPacket.INSTANCE
                || !dec.equals(ServerboundClientTickEndPacket.INSTANCE)) {
            throw new IllegalStateException("round-trip mismatch: decode did not yield INSTANCE");
        }
        if (rbuf.readableBytes() != 0) {
            throw new IllegalStateException(
                "decode left " + rbuf.readableBytes() + " trailing byte(s)");
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
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
