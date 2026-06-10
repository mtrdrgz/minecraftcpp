// Ground truth for net.minecraft.network.protocol.game.ClientboundLevelEventPacket.
//
// The packet's real STREAM_CODEC is Packet.codec(write, ::new) over a plain
// FriendlyByteBuf (ClientboundLevelEventPacket.java:10-12). The
// write(FriendlyByteBuf) body is, VERBATIM (lines 32-37):
//   output.writeInt(this.type);          // 4-byte big-endian int
//   output.writeBlockPos(this.pos);      // long asLong(), big-endian 8 bytes
//   output.writeInt(this.data);          // 4-byte big-endian int
//   output.writeBoolean(this.globalEvent); // 1 byte 0/1
//
//   writeBlockPos -> output.writeLong(pos.asLong())   (FriendlyByteBuf.java:398-400)
//   asLong packs x(26)/y(12)/z(26): X_OFFSET=38, Z_OFFSET=12, Y_OFFSET=0
//        (BlockPos.java:107-116).  y is the 12-bit signed field [-2048, 2047];
//        x,z the 26-bit signed field [-33554432, 33554431]. All cases stay in
//        range so the round-trip is exact.
//   read: type=readInt(); pos=readBlockPos(); data=readInt(); global=readBoolean()
//        (lines 25-30)
//
// No registry-held type / ItemStack / Component / Holder / NBT is on the wire:
// every field is a primitive (int, long-packed BlockPos, int, bool), so the C++
// PacketBuffer can rebuild the body field-for-field.
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <type-dec> <posLong-dec> <data-dec> <global-dec> <readableBytes> <hex>
// where type/data are signed 32-bit decimal, posLong is pos.asLong() (signed
// 64-bit decimal), global is 0/1, readableBytes is the total payload length, and
// hex is the full payload as lowercase bytes.
import io.netty.buffer.Unpooled;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundLevelEventPacket;
import net.minecraft.server.Bootstrap;

public class PktLevelEventParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // int battery crossing sign, zero, and 32-bit extremes.
        int[] ints = {
            0, 1, -1, 2, -2, 127, 128, -128, 255, 256, -256,
            32767, 32768, 65535, 65536, 1000000, -1000000,
            2147483647, -2147483648, 16777216, -16777216,
            // LevelEvent-ish realistic event ids / data
            1000, 1004, 1010, 2001, 3000, 1500, 9, 10, 999,
        };

        // BlockPos battery: 0, small, signs, and the exact 26/12/26 packing corners.
        int[][] positions = {
            {0, 0, 0},
            {1, 2, 3},
            {-1, -1, -1},
            {1, 0, 0},
            {0, 1, 0},
            {0, 0, 1},
            {33554431, 2047, 33554431},    // max corner
            {-33554432, -2048, -33554432}, // min corner
            {16777216, 100, -16777216},
            {-16777216, -100, 16777216},
            {1000000, 320, -1000000},
            {-7, 2047, 7},
            {123456, -2048, -654321},
            {30000000, 64, -30000000},
        };

        // (A) Sweep `type` ids at a fixed pos + data + global.
        for (int t : ints) {
            emit("type", t, 5, 6, 7, 2001, false);
            emit("type", t, -8, 9, -10, 1004, true);
        }

        // (B) Sweep BlockPos with a fixed type + data + global.
        for (int[] p : positions) {
            emit("pos", 2001, p[0], p[1], p[2], 0, false);
            emit("pos", 1004, p[0], p[1], p[2], 3, true);
        }

        // (C) Sweep `data` ids at a fixed pos + type + global.
        for (int d : ints) {
            emit("data", 1500, 1, 2, 3, d, false);
            emit("data", 3000, -1, -2, -3, d, true);
        }

        // (D) Both global flag values explicitly at origin.
        emit("global", 1000, 0, 0, 0, 0, false);
        emit("global", 1000, 0, 0, 0, 0, true);

        // (E) Fully-mixed extreme cases.
        emit("mix", Integer.MAX_VALUE, 33554431, 2047, 33554431, Integer.MIN_VALUE, true);
        emit("mix", Integer.MIN_VALUE, -33554432, -2048, -33554432, Integer.MAX_VALUE, false);
        emit("mix", -1, -1, -1, -1, -1, true);
        emit("mix", 0, 0, 0, 0, 0, false);
    }

    @SuppressWarnings("deprecation")
    static void emit(String name, int type, int x, int y, int z, int data, boolean global) {
        BlockPos pos = new BlockPos(x, y, z);

        ClientboundLevelEventPacket pkt = new ClientboundLevelEventPacket(type, pos, data, global);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundLevelEventPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ClientboundLevelEventPacket back = ClientboundLevelEventPacket.STREAM_CODEC.decode(rbuf);
        if (back.getType() != type || !back.getPos().equals(pos)
            || back.getData() != data || back.isGlobalEvent() != global) {
            throw new IllegalStateException("round-trip mismatch for " + name
                + " type=" + type + " pos=" + pos + " data=" + data + " global=" + global
                + " -> type=" + back.getType() + " pos=" + back.getPos()
                + " data=" + back.getData() + " global=" + back.isGlobalEvent());
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(type);            // signed 32-bit decimal
        O.print('\t');
        O.print(pos.asLong());    // signed 64-bit decimal
        O.print('\t');
        O.print(data);            // signed 32-bit decimal
        O.print('\t');
        O.print(global ? 1 : 0);  // 0/1
        O.print('\t');
        O.print(n);               // readableBytes
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
