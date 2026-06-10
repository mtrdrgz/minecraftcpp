// Ground truth for net.minecraft.network.protocol.game.ClientboundMountScreenOpenPacket.
//
// Source (26.1.2/src/net/minecraft/network/protocol/game/ClientboundMountScreenOpenPacket.java):
//   ctor        : ClientboundMountScreenOpenPacket(int containerId, int inventoryColumns, int entityId)
//   STREAM_CODEC : Packet.codec(ClientboundMountScreenOpenPacket::write, ClientboundMountScreenOpenPacket::new)
//   write(buf)  : buf.writeContainerId(this.containerId);   // == VarInt.write (FriendlyByteBuf:679-685)
//                 buf.writeVarInt(this.inventoryColumns);    // LEB128, no zig-zag
//                 buf.writeInt(this.entityId);               // 4B big-endian (NOT VarInt)
//   read(buf)   : containerId      = buf.readContainerId();  // == VarInt.read
//                 inventoryColumns = buf.readVarInt();
//                 entityId         = buf.readInt();
// Packet.codec -> StreamCodec.ofMember: NO packet-id prefix, just the body. So the whole
// wire payload is exactly: VARINT(containerId) ++ VARINT(inventoryColumns) ++ INT(entityId, 4B BE).
//
// NOTE: writeContainerId is plain VarInt.write (verified at FriendlyByteBuf.java:679-685), and
// entityId uses writeInt (a fixed 4-byte big-endian int), NOT writeVarInt. The task hint listed
// entityId as "VarInt" but the real codec emits a fixed 4-byte int; we follow the source verbatim.
//
// Row format (tab separated), TAG = ENC:
//   ENC <containerId-dec> <inventoryColumns-dec> <entityId-dec> <readableBytes-dec> <hex>
// All three fields are decimal signed ints; <hex> is the full packet payload, lowercase hex.
//
// Every case is round-trip-decoded through the SAME codec and asserted equal (sanity) before
// emitting. The C++ pkt_mount_screen_open_parity rebuilds the packet from these fields,
// re-encodes via PacketBuffer (writeVarInt + writeVarInt + writeInt in the same order), and must
// match <hex> byte-for-byte (+ readableBytes); it also decodes <hex> via readVarInt + readVarInt
// + readInt and checks the recovered fields exact.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundMountScreenOpenPacket;
import net.minecraft.server.Bootstrap;

public class PktMountScreenOpenParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ClientboundMountScreenOpenPacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ClientboundMountScreenOpenPacket>)
                ClientboundMountScreenOpenPacket.STREAM_CODEC;

        // containerId: a window/menu id (0..100 in practice, 0 == player inventory). Sent via
        // writeContainerId == VarInt (LEB128, no zig-zag). Pin every 1->2->3->4->5 byte boundary
        // and the int extremes; negatives encode as 5 bytes (round-trip safety).
        int[] containerIds = {
            0, 1, 2, 5, 100,
            127, 128, 129,                    // 1->2 byte boundary
            255, 256,
            16383, 16384, 16385,              // 2->3 byte boundary
            2097151, 2097152, 2097153,        // 3->4 byte boundary
            268435455, 268435456, 268435457,  // 4->5 byte boundary
            Integer.MAX_VALUE,                // 0x7fffffff -> 5 bytes
            -1, -128, Integer.MIN_VALUE       // 5 bytes (no zig-zag)
        };

        // inventoryColumns: a small horse-inventory column count (typically 0..15). Independent
        // VarInt; sweep the same LEB128 boundaries to prove the codec width transitions.
        int[] columns = {
            0, 1, 2, 5, 15,
            127, 128, 129,
            16383, 16384,
            2097151, 2097152,
            268435455, 268435456,
            Integer.MAX_VALUE,
            -1, Integer.MIN_VALUE
        };

        // entityId: the mount's network entity id. Sent via writeInt -> ALWAYS exactly 4 bytes,
        // big-endian two's-complement. Cover sign, byte boundaries within the 4-byte field, and
        // the int extremes.
        int[] entityIds = {
            0, 1, -1,
            127, 128, 255, 256,
            32767, 32768, 65535, 65536,
            8388607, 8388608, 16777215, 16777216,
            123456789,
            Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // (A) sweep containerId with fixed nominal columns/entityId (the common wire case).
        for (int cid : containerIds) {
            emit(CODEC, cid, 5, 1234);
        }
        // (B) sweep inventoryColumns (VarInt boundaries) with fixed containerId/entityId.
        for (int col : columns) {
            emit(CODEC, 3, col, 1234);
        }
        // (C) sweep entityId (fixed 4-byte int) with fixed containerId/columns.
        for (int eid : entityIds) {
            emit(CODEC, 3, 5, eid);
        }
        // (D) a few combined extremes to exercise all three fields together.
        emit(CODEC, 0, 0, 0);
        emit(CODEC, Integer.MAX_VALUE, Integer.MAX_VALUE, Integer.MAX_VALUE);
        emit(CODEC, Integer.MIN_VALUE, Integer.MIN_VALUE, Integer.MIN_VALUE);
        emit(CODEC, 16384, 128, 65536);
        emit(CODEC, 128, 16384, -1);
    }

    // Construct the real packet, encode through the real STREAM_CODEC, sanity round-trip, and
    // emit the ENC row. Constructor arg order is (containerId, inventoryColumns, entityId).
    static void emit(StreamCodec<FriendlyByteBuf, ClientboundMountScreenOpenPacket> CODEC,
                     int containerId, int inventoryColumns, int entityId) {
        ClientboundMountScreenOpenPacket pkt =
            new ClientboundMountScreenOpenPacket(containerId, inventoryColumns, entityId);
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert field equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ClientboundMountScreenOpenPacket dec = CODEC.decode(rbuf);
        if (dec.getContainerId() != containerId
                || dec.getInventoryColumns() != inventoryColumns
                || dec.getEntityId() != entityId) {
            throw new IllegalStateException(
                "round-trip mismatch: in=(" + containerId + "," + inventoryColumns + ","
                + entityId + ") out=(" + dec.getContainerId() + "," + dec.getInventoryColumns()
                + "," + dec.getEntityId() + ")");
        }

        O.print("ENC\t");
        O.print(containerId);
        O.print('\t');
        O.print(inventoryColumns);
        O.print('\t');
        O.print(entityId);
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
