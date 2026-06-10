// Ground truth for the ClientboundResetChatPacket codec (configuration protocol).
//
// net.minecraft.network.protocol.configuration.ClientboundResetChatPacket:
//
//   public static final ClientboundResetChatPacket INSTANCE = new ClientboundResetChatPacket();
//   public static final StreamCodec<ByteBuf, ClientboundResetChatPacket> STREAM_CODEC =
//       StreamCodec.unit(INSTANCE);
//
// StreamCodec.unit(instance).encode(output, value) writes NOTHING to the buffer —
// it only asserts value.equals(instance) (the singleton) and otherwise throws. Its
// decode(input) returns the singleton without consuming any bytes. Therefore the
// encoded body is EXACTLY zero bytes (readableBytes()==0), and decode yields the
// INSTANCE. There are no fields of any kind on the wire (no registry-held types,
// no ItemStack, no Component, no NBT, no Holder, no SoundEvent) — nothing for
// PacketBuffer to build.
//
// We drive the REAL StreamCodec (encode + round-trip decode) and dump:
//   RESET_CHAT_CFG <name> <readableBytes> <hexBytes(empty)> <isInstance>
//
// where <hexBytes> is the empty string for a zero-length body. The C++
// pkt_reset_chat_cfg_parity asserts the same: an empty PacketBuffer (nothing
// written) with size()==0 and hex=="" matching the expected zero-byte encoding.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.configuration.ClientboundResetChatPacket;

public class PktResetChatCfgParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // STREAM_CODEC is typed StreamCodec<ByteBuf, ...>; FriendlyByteBuf is a ByteBuf,
        // so we encode into a FriendlyByteBuf to mirror the on-the-wire framing path.
        StreamCodec<ByteBuf, ClientboundResetChatPacket> codec =
            ClientboundResetChatPacket.STREAM_CODEC;

        // The packet is a singleton with no fields. The only physical degrees of freedom
        // are: encode the INSTANCE, and confirm the body is always zero bytes regardless
        // of how many times we encode into the same buffer (it stays empty). We emit a
        // few finite cases that all must yield zero readable bytes.
        String[] names = { "instance", "encode_twice_same_buf", "fresh_buffer" };

        for (String name : names) {
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());

            if (name.equals("encode_twice_same_buf")) {
                // Encoding the unit value repeatedly still writes nothing.
                codec.encode(buf, ClientboundResetChatPacket.INSTANCE);
                codec.encode(buf, ClientboundResetChatPacket.INSTANCE);
            } else {
                codec.encode(buf, ClientboundResetChatPacket.INSTANCE);
            }

            // Snapshot the bytes (do not advance the original reader index).
            int n = buf.readableBytes();
            StringBuilder hex = new StringBuilder();
            ByteBuf dup = buf.duplicate();
            while (dup.isReadable()) hex.append(String.format("%02x", dup.readByte() & 0xff));

            // Round-trip decode through the REAL codec — must yield the singleton and
            // consume no bytes.
            ClientboundResetChatPacket back = codec.decode(buf);
            int isInstance = (back == ClientboundResetChatPacket.INSTANCE) ? 1 : 0;

            // hex column is "" for a zero-byte body; we print it explicitly so the TSV
            // column is always present.
            O.printf("RESET_CHAT_CFG\t%s\t%d\t%s\t%d%n",
                name, n, hex.toString(), isInstance);
        }
    }
}
