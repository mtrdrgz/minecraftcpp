// Ground truth for net.minecraft.network.protocol.game.ServerboundContainerButtonClickPacket.
//
// The packet is a record(int containerId, int buttonId). Its STREAM_CODEC is
// StreamCodec.composite(ByteBufCodecs.CONTAINER_ID, ..::containerId,
//                       ByteBufCodecs.VAR_INT,      ..::buttonId, ..::new)
// (ServerboundContainerButtonClickPacket.java:10-16).
//   ByteBufCodecs.CONTAINER_ID -> FriendlyByteBuf.writeContainerId == VarInt.write   (ByteBufCodecs.java:200-208, FriendlyByteBuf.java:679-681)
//   ByteBufCodecs.VAR_INT      -> VarInt.write                                        (ByteBufCodecs.java:102-110)
// StreamCodec.composite carries NO packet-id prefix, just the two body fields in order,
// so the whole wire payload is: VarInt(containerId) ++ VarInt(buttonId), both signed
// ints in plain LEB128 (no zig-zag; negatives encode as 5 bytes).
//
// Row format (tab separated):
//   ENC <containerId-dec> <buttonId-dec> <readableBytes-dec> <hex>
//
// We round-trip-decode every case through the SAME codec and assert field equality as a
// sanity check before emitting. The C++ pkt_container_button_click_sb_parity rebuilds the
// packet from <containerId>/<buttonId>, re-encodes via PacketBuffer.writeVarInt twice, and
// must match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt
// twice and checks the recovered fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundContainerButtonClickPacket;
import net.minecraft.server.Bootstrap;

public class PktContainerButtonClickSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundContainerButtonClickPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundContainerButtonClickPacket>)
                ServerboundContainerButtonClickPacket.STREAM_CODEC;

        // Finite/physical battery for the two VarInt fields. containerId is a small
        // menu window id (0 = player inventory, then 1..N for open menus, occasionally
        // -1/-2 for synthetic ids) and buttonId is a small lectern/loom/enchant button
        // index; both are nevertheless full signed ints over the wire, so we pin every
        // LEB128 byte boundary (1->2->3->4->5 bytes) and the int extremes (negatives
        // encode as 5 bytes since VarInt.write does not zig-zag).
        int[] battery = {
            0, 1, 2, 7, 42, 100,
            127, 128, 129,                   // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,             // 2->3 byte boundary
            2097151, 2097152, 2097153,       // 3->4 byte boundary
            268435455, 268435456, 268435457, // 4->5 byte boundary
            123456789,
            Integer.MAX_VALUE,               // 0x7fffffff -> 5 bytes
            -1, -2, -128, -16384, -2097152,
            Integer.MIN_VALUE                // 0x80000000 -> 5 bytes
        };

        // Realistic small pairings (every menu button is a tiny index) plus the full
        // boundary cross-product so both VarInt encoders are exercised independently.
        int[][] realistic = {
            {0, 0}, {0, 1}, {1, 0}, {1, 1}, {2, 3}, {5, 2},
            {0, 7}, {3, 0}, {1, 2}, {4, 4}
        };
        for (int[] pair : realistic) {
            emit(CODEC, pair[0], pair[1]);
        }
        for (int c : battery) {
            for (int b : battery) {
                emit(CODEC, c, b);
            }
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundContainerButtonClickPacket> CODEC,
                     int containerId, int buttonId) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundContainerButtonClickPacket pkt =
            new ServerboundContainerButtonClickPacket(containerId, buttonId);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert field equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundContainerButtonClickPacket dec = CODEC.decode(rbuf);
        if (dec.containerId() != containerId || dec.buttonId() != buttonId) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + containerId + "," + buttonId + ") out=("
                    + dec.containerId() + "," + dec.buttonId() + ")");
        }

        O.print("ENC\t");
        O.print(containerId);
        O.print('\t');
        O.print(buttonId);
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
