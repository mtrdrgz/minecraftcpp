// Ground truth for net.minecraft.network.protocol.game.ClientboundChunksBiomesPacket.
//
// The packet's real STREAM_CODEC is Packet.codec(write, ::new) over a plain
// FriendlyByteBuf (NOT a RegistryFriendlyByteBuf -- no registry state on the wire).
// The write(FriendlyByteBuf) body is, VERBATIM (ClientboundChunksBiomesPacket.java:28-30):
//   output.writeCollection(this.chunkBiomeData, (o, c) -> c.write(o));
//
//   writeCollection (FriendlyByteBuf) = VarInt.write(size) then, per element in
//   iteration order, ChunkBiomeData.write(o) which is (lines 81-84):
//       output.writeChunkPos(this.pos);     // FriendlyByteBuf.java:406-409
//       output.writeByteArray(this.buffer); // FriendlyByteBuf.java:284-292
//
//   writeChunkPos(pos)  -> output.writeLong(pos.pack())             (8 bytes, big-endian)
//        ChunkPos.pack(x,z) = (x & 0xFFFFFFFFL) | ((z & 0xFFFFFFFFL) << 32)
//        (ChunkPos.java:72-78)  -- x in the low 32 bits, z in the high 32 bits.
//   writeByteArray(bytes) -> VarInt.write(bytes.length); writeBytes(bytes)  (raw bytes)
//        (FriendlyByteBuf.java:289-292)
//
//   read (lines 20-22, 47-49):  list = readList(ChunkBiomeData::new)
//        ChunkBiomeData = (readChunkPos(), readByteArray(2097152))
//        readChunkPos = ChunkPos.unpack(readLong()) ; unpack(k)=( (int)k, (int)(k>>32) )
//        readByteArray(max) = bytes whose length is the VarInt prefix.
//
// So the body is exactly:
//     VarInt(size)
//     for each entry (insertion / list order):
//         long  pos.pack()                 (8 BE bytes)
//         VarInt(bufferLen)  bufferBytes    (raw)
//
// Row format (tab separated), TAG = ENC:
//   ENC <name> <size> [<posLong-dec> <bufHex-or-_>]*size <readableBytes> <hex>
// where posLong is pos.pack() (signed 64-bit decimal), bufHex is the raw buffer
// bytes as lowercase hex (the token "_" means a zero-length buffer), readableBytes
// is the total payload length, and hex is the full payload as lowercase bytes.
//
// The C++ pkt_chunks_biomes_cb_parity rebuilds the body from size + (posLong,buffer)
// pairs via PacketBuffer (writeVarInt(size) then per entry writeLong + writeVarInt(len)
// + raw bytes) and must match hex byte-for-byte and readableBytes; it then decodes hex
// back and checks the fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.List;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundChunksBiomesPacket;
import net.minecraft.network.protocol.game.ClientboundChunksBiomesPacket.ChunkBiomeData;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.ChunkPos;

public class PktChunksBiomesParity {
    static final java.io.PrintStream O = System.out;

    // A single chunk entry: (chunkX, chunkZ, raw biome buffer bytes).
    record Entry(int x, int z, byte[] buf) {}

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // ChunkPos packing corners: x in the LOW 32 bits, z in the HIGH 32 bits.
        // Use the full signed 32-bit range plus VarInt-irrelevant fixed longs.
        int[][] coords = {
            {0, 0},
            {1, 0},
            {0, 1},
            {1, 2},
            {-1, -1},
            {-1, 0},
            {0, -1},
            {2147483647, 2147483647},   // max corner
            {-2147483648, -2147483648}, // min corner
            {2147483647, -2147483648},
            {-2147483648, 2147483647},
            {123456, -654321},
            {-7, 7},
            {16777216, -16777216},
        };

        // Buffer batteries crossing the VarInt length boundary (1->2 bytes at 128).
        byte[] empty = new byte[0];
        byte[] one   = bytes(0xAB);
        byte[] three = bytes(0x00, 0x7F, 0xFF);
        byte[] len127 = ramp(127);   // VarInt length = 0x7f  (1 byte)
        byte[] len128 = ramp(128);   // VarInt length = 0x80 0x01 (2 bytes)
        byte[] len300 = ramp(300);   // VarInt length = 0xac 0x02 (2 bytes)

        // (A) size==0 : empty collection.
        emit("empty", new ArrayList<>());

        // (B) Single-entry sweeps over coordinate corners, fixed small buffer.
        for (int[] c : coords) {
            emit("coord", List.of(new Entry(c[0], c[1], three)));
        }

        // (C) Single-entry sweeps over buffer-length VarInt boundaries, fixed pos.
        emit("buf0",   List.of(new Entry(5, 6, empty)));
        emit("buf1",   List.of(new Entry(5, 6, one)));
        emit("buf127", List.of(new Entry(5, 6, len127)));
        emit("buf128", List.of(new Entry(5, 6, len128)));
        emit("buf300", List.of(new Entry(5, 6, len300)));

        // (D) Multi-entry collections (size 2, 3) mixing coords + buffer lengths.
        emit("two", List.of(
            new Entry(0, 0, one),
            new Entry(-1, -1, len128)));
        emit("three", List.of(
            new Entry(1, 2, empty),
            new Entry(2147483647, -2147483648, len127),
            new Entry(-654321, 123456, three)));
        emit("four", List.of(
            new Entry(0, 0, empty),
            new Entry(1, 0, one),
            new Entry(0, 1, three),
            new Entry(-1, -1, len300)));
    }

    @SuppressWarnings("deprecation")
    static void emit(String name, List<Entry> entries) {
        List<ChunkBiomeData> data = new ArrayList<>();
        for (Entry e : entries) {
            data.add(new ChunkBiomeData(new ChunkPos(e.x(), e.z()), e.buf().clone()));
        }
        ClientboundChunksBiomesPacket pkt = new ClientboundChunksBiomesPacket(data);

        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundChunksBiomesPacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i) & 0xff));

        // Round-trip decode through the SAME codec and assert equality (sanity).
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex.toString())));
        ClientboundChunksBiomesPacket back = ClientboundChunksBiomesPacket.STREAM_CODEC.decode(rbuf);
        List<ChunkBiomeData> bd = back.chunkBiomeData();
        if (bd.size() != entries.size()) {
            throw new IllegalStateException("round-trip size mismatch for " + name
                + " want=" + entries.size() + " got=" + bd.size());
        }
        for (int i = 0; i < bd.size(); i++) {
            Entry e = entries.get(i);
            ChunkBiomeData d = bd.get(i);
            if (d.pos().x() != e.x() || d.pos().z() != e.z() || !java.util.Arrays.equals(d.buffer(), e.buf())) {
                throw new IllegalStateException("round-trip entry mismatch for " + name + " idx=" + i
                    + " wantPos=(" + e.x() + "," + e.z() + ") gotPos=" + d.pos()
                    + " wantBufLen=" + e.buf().length + " gotBufLen=" + d.buffer().length);
            }
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(entries.size());
        for (Entry e : entries) {
            O.print('\t');
            O.print(ChunkPos.pack(e.x(), e.z()));   // signed 64-bit decimal (low=x, high=z)
            O.print('\t');
            O.print(e.buf().length == 0 ? "_" : hexOf(e.buf()));
        }
        O.print('\t');
        O.print(n);                                 // readableBytes
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }

    static byte[] bytes(int... v) {
        byte[] b = new byte[v.length];
        for (int i = 0; i < v.length; i++) b[i] = (byte) v[i];
        return b;
    }

    // Deterministic ramp so the C++ side reconstructs identical content from hex.
    static byte[] ramp(int len) {
        byte[] b = new byte[len];
        for (int i = 0; i < len; i++) b[i] = (byte) ((i * 31 + 7) & 0xff);
        return b;
    }

    static String hexOf(byte[] b) {
        StringBuilder s = new StringBuilder(b.length * 2);
        for (byte x : b) s.append(String.format("%02x", x & 0xff));
        return s.toString();
    }

    static byte[] unhex(String s) {
        byte[] out = new byte[s.length() / 2];
        for (int i = 0; i < out.length; i++)
            out[i] = (byte) Integer.parseInt(s.substring(i * 2, i * 2 + 2), 16);
        return out;
    }
}
