// Ground truth for net.minecraft.network.protocol.game.ServerboundContainerSlotStateChangedPacket.
//
// The packet is a record(int slotId, int containerId, boolean newState). Its STREAM_CODEC is
// Packet.codec(write, new), where write/read are exactly (see
// net.minecraft.network.protocol.game.ServerboundContainerSlotStateChangedPacket lines 13-21):
//   write : output.writeVarInt(this.slotId);
//           output.writeContainerId(this.containerId);   // FriendlyByteBuf.writeContainerId == VarInt.write
//           output.writeBoolean(this.newState);
//   read  : this(input.readVarInt(), input.readContainerId(), input.readBoolean());
// FriendlyByteBuf.writeContainerId(int)/readContainerId() are exactly VarInt.write/VarInt.read
// (FriendlyByteBuf.java lines 671-685), so containerId is just a plain VarInt -- NO zig-zag.
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. So the whole wire
// payload is: VarInt(slotId) ++ VarInt(containerId) ++ byte(newState ? 1 : 0).
//
// Row format (tab separated):
//   ENC <slotId-dec> <containerId-dec> <newState-dec(0|1)> <readableBytes-dec> <hex>
//        encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert all three fields equal as a
// sanity check before emitting. The C++ pkt_container_slot_state_sb_parity rebuilds the wire from
// <slotId,containerId,newState> via PacketBuffer.writeVarInt x2 + writeBool, must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt x2 + readBool and checks
// the recovered fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundContainerSlotStateChangedPacket;
import net.minecraft.server.Bootstrap;

public class PktContainerSlotStateSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundContainerSlotStateChangedPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundContainerSlotStateChangedPacket>)
                ServerboundContainerSlotStateChangedPacket.STREAM_CODEC;

        // Both slotId and containerId are plain VarInts (LEB128, signed, no zig-zag).
        // Pin every byte-length boundary (1->2->3->4->5 bytes) and the int extremes
        // (negatives encode as 5 bytes since writeVarInt does not zig-zag).
        int[] varints = {
            0, 1, 2, 7, 36, 100,
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

        // Pair each slotId with a rotating containerId (and a rotating newState), so both
        // VarInt-length boundaries are exercised on both fields, plus both bool values.
        for (int i = 0; i < varints.length; i++) {
            int slot = varints[i];
            int cid = varints[(i + 7) % varints.length];
            boolean state = (i & 1) == 0;
            emit(CODEC, slot, cid, state);
        }
        // Reverse pairing so containerId also sweeps every width against a rotating slotId.
        for (int j = 0; j < varints.length; j++) {
            int cid = varints[j];
            int slot = varints[(j + 3) % varints.length];
            boolean state = (j & 1) != 0;
            emit(CODEC, slot, cid, state);
        }
        // Explicit corner combos: zero/zero, extremes, both bool values on the same fields.
        emit(CODEC, 0, 0, false);
        emit(CODEC, 0, 0, true);
        emit(CODEC, Integer.MAX_VALUE, Integer.MAX_VALUE, true);
        emit(CODEC, Integer.MIN_VALUE, Integer.MIN_VALUE, false);
        emit(CODEC, Integer.MIN_VALUE, Integer.MAX_VALUE, true);
        emit(CODEC, Integer.MAX_VALUE, Integer.MIN_VALUE, false);
        emit(CODEC, -1, -1, true);
        emit(CODEC, 127, 128, false);
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundContainerSlotStateChangedPacket> CODEC,
                     int slotId, int containerId, boolean newState) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundContainerSlotStateChangedPacket pkt =
            new ServerboundContainerSlotStateChangedPacket(slotId, containerId, newState);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundContainerSlotStateChangedPacket dec = CODEC.decode(rbuf);
        if (dec.slotId() != slotId || dec.containerId() != containerId
            || dec.newState() != newState) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + slotId + "," + containerId + "," + newState
                    + ") out=(" + dec.slotId() + "," + dec.containerId() + "," + dec.newState()
                    + ")");
        }

        O.print("ENC\t");
        O.print(slotId);
        O.print('\t');
        O.print(containerId);
        O.print('\t');
        O.print(newState ? 1 : 0);
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
