// Ground truth for the ServerboundAcceptCodeOfConductPacket codec.
//
// net.minecraft.network.protocol.configuration.ServerboundAcceptCodeOfConductPacket:
//
//   public record ServerboundAcceptCodeOfConductPacket() implements Packet<...> {
//      public static final ServerboundAcceptCodeOfConductPacket INSTANCE =
//          new ServerboundAcceptCodeOfConductPacket();
//      public static final StreamCodec<ByteBuf, ServerboundAcceptCodeOfConductPacket>
//          STREAM_CODEC = StreamCodec.unit(INSTANCE);
//   }
//
// StreamCodec.unit(instance).encode(output, value) writes NOTHING to the buffer —
// it only asserts value.equals(instance) (the singleton) and otherwise throws. Its
// decode(input) returns the singleton without consuming any bytes. Therefore the
// encoded body is EXACTLY zero bytes (readableBytes()==0), and decode yields the
// INSTANCE. There are no fields of any kind on the wire (no registry-held types,
// no ItemStack, no Component, no NBT, no Holder) — nothing for PacketBuffer to build.
//
// We drive the REAL StreamCodec (encode + round-trip decode) and dump:
//   ENC <name> <readableBytes> <hexBytes(empty)> <isInstance>
//
// where <hexBytes> is the empty string for a zero-length body. The C++
// pkt_accept_code_sb_parity asserts the same: an empty PacketBuffer (nothing
// written) with size()==0 and hex=="" matching the expected zero-byte encoding.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.configuration.ServerboundAcceptCodeOfConductPacket;

public class PktAcceptCodeSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // STREAM_CODEC is typed StreamCodec<ByteBuf, ...>; FriendlyByteBuf is a ByteBuf,
        // so we encode into a FriendlyByteBuf to mirror the on-the-wire framing path.
        StreamCodec<ByteBuf, ServerboundAcceptCodeOfConductPacket> codec =
            ServerboundAcceptCodeOfConductPacket.STREAM_CODEC;

        // The packet is a singleton record with no components. The only physical degrees
        // of freedom are: encode the INSTANCE, and confirm the body is always zero bytes
        // regardless of how many times we encode into the same buffer (it stays empty).
        // We emit a few finite cases that all must yield zero readable bytes.
        String[] names = { "instance", "encode_twice_same_buf", "fresh_buffer" };

        for (String name : names) {
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());

            if (name.equals("encode_twice_same_buf")) {
                // Encoding the unit value repeatedly still writes nothing.
                codec.encode(buf, ServerboundAcceptCodeOfConductPacket.INSTANCE);
                codec.encode(buf, ServerboundAcceptCodeOfConductPacket.INSTANCE);
            } else {
                codec.encode(buf, ServerboundAcceptCodeOfConductPacket.INSTANCE);
            }

            // Snapshot the bytes (do not advance the original reader index).
            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte() & 0xff));

            // Round-trip decode through the REAL codec — must yield the singleton and
            // consume no bytes.
            ServerboundAcceptCodeOfConductPacket back = codec.decode(buf);
            int isInstance = (back == ServerboundAcceptCodeOfConductPacket.INSTANCE) ? 1 : 0;

            // hex column is "" for a zero-byte body; we print it explicitly so the TSV
            // column is always present.
            O.printf("ENC\t%s\t%d\t%s\t%d%n", name, n, hex.toString(), isInstance);
        }
    }
}
