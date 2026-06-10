// Ground truth for the ServerboundPlayerLoadedPacket codec.
//
// net.minecraft.network.protocol.game.ServerboundPlayerLoadedPacket:
//
//   public record ServerboundPlayerLoadedPacket() implements Packet<ServerGamePacketListener> {
//      public static final StreamCodec<ByteBuf, ServerboundPlayerLoadedPacket> STREAM_CODEC =
//          StreamCodec.unit(new ServerboundPlayerLoadedPacket());
//      ...
//   }
//
// StreamCodec.unit(instance).encode(output, value) writes NOTHING to the buffer —
// it only asserts value.equals(instance) (the unit value) and otherwise throws. Its
// decode(input) returns that unit instance without consuming any bytes. Therefore the
// encoded body is EXACTLY zero bytes (readableBytes()==0), and decode yields the
// unit instance. The record has no components, so there are no fields of any kind on
// the wire (no registry-held types, no ItemStack, no Component, no NBT, no Holder) —
// nothing for PacketBuffer to build.
//
// We drive the REAL StreamCodec (encode + round-trip decode) and dump:
//   PLAYER_LOADED_SB <name> <readableBytes> <hexBytes(empty)> <isUnit>
//
// where <hexBytes> is the empty string for a zero-length body. The C++
// pkt_player_loaded_sb_parity asserts the same: an empty PacketBuffer (nothing
// written) with size()==0 and hex=="" matching the expected zero-byte encoding.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPlayerLoadedPacket;

public class PktPlayerLoadedSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // STREAM_CODEC is typed StreamCodec<ByteBuf, ...>; FriendlyByteBuf is a ByteBuf,
        // so we encode into a FriendlyByteBuf to mirror the on-the-wire framing path.
        StreamCodec<ByteBuf, ServerboundPlayerLoadedPacket> codec =
            ServerboundPlayerLoadedPacket.STREAM_CODEC;

        // The unit value the codec was constructed with. The packet is a zero-component
        // record, so a fresh instance is .equals() to the codec's unit value (records
        // derive structural equality), which is exactly what unit's encoder requires.
        ServerboundPlayerLoadedPacket value = new ServerboundPlayerLoadedPacket();

        // The packet is a record with no components. The only physical degrees of freedom
        // are: encode the unit value, and confirm the body is always zero bytes regardless
        // of how many times we encode into the same buffer (it stays empty). We emit a
        // few finite cases that all must yield zero readable bytes.
        String[] names = { "unit", "encode_twice_same_buf", "fresh_buffer" };

        for (String name : names) {
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());

            if (name.equals("encode_twice_same_buf")) {
                // Encoding the unit value repeatedly still writes nothing.
                codec.encode(buf, value);
                codec.encode(buf, value);
            } else {
                codec.encode(buf, value);
            }

            // Snapshot the bytes (do not advance the original reader index).
            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte() & 0xff));

            // Round-trip decode through the REAL codec — must yield the unit value and
            // consume no bytes.
            ServerboundPlayerLoadedPacket back = codec.decode(buf);
            int isUnit = (back != null && back.equals(value)) ? 1 : 0;

            // hex column is "" for a zero-byte body; we print it explicitly so the TSV
            // column is always present.
            O.printf("PLAYER_LOADED_SB\t%s\t%d\t%s\t%d%n",
                name, n, hex.toString(), isUnit);
        }
    }
}
