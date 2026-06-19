// Ground truth for net.minecraft.network.protocol.game.ClientboundSectionBlocksUpdatePacket.
//
// 26.1.2 wire format (verified VERBATIM against 26.1.2/src
// net/minecraft/network/protocol/game/ClientboundSectionBlocksUpdatePacket.java):
//
//   STREAM_CODEC = Packet.codec(ClientboundSectionBlocksUpdatePacket::write, ::new)  over a PLAIN FriendlyByteBuf
//   private void write(FriendlyByteBuf output) {
//       SectionPos.STREAM_CODEC.encode(output, this.sectionPos);   // = output.writeLong(sectionPos.asLong())  (8 BE bytes)
//       output.writeVarInt(this.positions.length);                 // VarInt count
//       for (int i = 0; i < this.positions.length; i++)
//           output.writeVarLong((long)Block.getId(this.states[i]) << 12 | this.positions[i]);  // VarLong per change
//   }
//   The private read-ctor pulls them back in the same order:
//       this.sectionPos = SectionPos.STREAM_CODEC.decode(input);   // SectionPos.of(input.readLong())
//       int count = input.readVarInt();
//       for (i): long c = input.readVarLong();
//                positions[i] = (short)(c & 4095L);                 // low 12 bits = pos-in-section
//                states[i]    = Block.BLOCK_STATE_REGISTRY.byId((int)(c >>> 12));  // high bits = block state id
//
//   SectionPos.STREAM_CODEC (SectionPos.java:34) = ByteBufCodecs.LONG.map(SectionPos::of, SectionPos::asLong)
//   ByteBufCodecs.LONG (ByteBufCodecs.java:114-122) = readLong()/writeLong()  (8 BE bytes)
//
// Round-trip identity used by this generator: we prefill a buf in READ order
// (writeLong(sectionLong), writeVarInt(count), writeVarLong(change)*count),
// invoke the private (FriendlyByteBuf) read-ctor reflectively, then re-encode via
// STREAM_CODEC. For the re-encoded bytes to equal the prefilled bytes:
//   * SectionPos.of(L).asLong() == L  holds for every long L (asLong packs x/y/z
//     back with the same masks SectionPos.of split out), so any 64-bit value is valid.
//   * each change uses a VALID block-state id (Block.getId(byId(id)) == id) and a
//     pos in 0..4095, so (id<<12 | pos) round-trips exactly.
//
// Row format (tab-separated), TAG = ENC:
//   ENC <name> <sectionLong-dec> <count-dec> <change0-dec> <change1-dec> ... <bytesN-dec> <hexBytes>
// sectionLong and each change are SIGNED decimal longs (as carried over the wire).
// <hexBytes> is the full packet payload, lowercase hex.
import io.netty.buffer.Unpooled;

import java.lang.reflect.Constructor;

import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.protocol.game.ClientboundSectionBlocksUpdatePacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.state.BlockState;

@SuppressWarnings({"unchecked", "deprecation"})
public class PktSectionBlocksUpdateParity {
    static final java.io.PrintStream O = System.out;

    static Constructor<ClientboundSectionBlocksUpdatePacket> READ_CTOR;

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        READ_CTOR = ClientboundSectionBlocksUpdatePacket.class.getDeclaredConstructor(FriendlyByteBuf.class);
        READ_CTOR.setAccessible(true);

        // Pick a handful of unconditionally-valid block-state ids. id 0 (air) plus a
        // few early-registered states; we verify each is a real, round-trippable id.
        int[] stateIds = {0, 1, 2, 9, 10, 50, 100, 1000};
        for (int id : stateIds) {
            BlockState bs = Block.BLOCK_STATE_REGISTRY.byId(id);
            if (bs == null || Block.getId(bs) != id) {
                throw new IllegalStateException("block-state id " + id + " is not round-trippable");
            }
        }

        // SectionPos longs: zero, sign, extrema, and a few packed coords across the
        // 22/20/22-bit fields so SectionPos.of/asLong is exercised broadly.
        long[] sectionLongs = {
            0L, 1L, -1L,
            asSec(0, 0, 0), asSec(1, 2, 3), asSec(-1, -1, -1),
            asSec(2097151, 524287, 2097151), asSec(-2097152, -524288, -2097152),
            asSec(12, -34, 56), asSec(-1000, 200, -3000),
            Long.MAX_VALUE, Long.MIN_VALUE, 0x0123456789ABCDEFL
        };

        // pos-in-section values (0..4095): VarLong byte boundaries are driven mainly by
        // the high block-state id, but exercise low/mid/high pos too.
        int[] posVals = {0, 1, 127, 128, 4095, 2048, 1234};

        int caseNo = 0;

        // 1) Sweep section longs with a single change (a stone-ish state, mid pos).
        for (long sl : sectionLongs) {
            emit("sec" + (caseNo++), sl, new long[]{ pack(50, 2048) });
        }

        // 2) Sweep state ids (single change) — drives the VarLong length boundaries.
        for (int id : stateIds) {
            emit("id" + (caseNo++), asSec(5, 6, 7), new long[]{ pack(id, 17) });
        }

        // 3) Sweep pos-in-section (single change).
        for (int p : posVals) {
            emit("pos" + (caseNo++), asSec(8, 9, 10), new long[]{ pack(100, p) });
        }

        // 4) Empty change list (count 0).
        emit("empty" + (caseNo++), asSec(0, 0, 0), new long[0]);

        // 5) Multi-change lists of varying sizes mixing ids and positions.
        emit("multi2" + (caseNo++), asSec(1, 0, -1), new long[]{ pack(0, 0), pack(1000, 4095) });
        emit("multi3" + (caseNo++), asSec(-5, 5, -5),
            new long[]{ pack(1, 1), pack(9, 2048), pack(100, 4094) });
        long[] big = new long[10];
        for (int i = 0; i < big.length; i++) big[i] = pack(stateIds[i % stateIds.length], (i * 401) & 4095);
        emit("multi10" + (caseNo++), asSec(7, 7, 7), big);
    }

    // Build a long via the real SectionPos packing so 'of' decodes the same coords.
    static long asSec(int x, int y, int z) {
        return net.minecraft.core.SectionPos.asLong(x, y, z);
    }

    // packedChange = (blockStateId << 12) | posInSection   (posInSection in 0..4095)
    static long pack(int stateId, int posInSection) {
        return ((long) stateId << 12) | (posInSection & 4095L);
    }

    static void emit(String name, long sectionLong, long[] changes) throws Exception {
        // Prefill a buffer in READ order, then invoke the private (FriendlyByteBuf) ctor.
        FriendlyByteBuf pre = new FriendlyByteBuf(Unpooled.buffer());
        pre.writeLong(sectionLong);
        pre.writeVarInt(changes.length);
        for (long c : changes) pre.writeVarLong(c);

        ClientboundSectionBlocksUpdatePacket pkt = READ_CTOR.newInstance(pre);

        // Re-encode through the REAL STREAM_CODEC — this is the ground-truth payload.
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSectionBlocksUpdatePacket.STREAM_CODEC.encode(buf, pkt);

        int n = buf.readableBytes();
        StringBuilder hex = new StringBuilder();
        for (int i = 0; i < n; i++) hex.append(String.format("%02x", buf.getByte(buf.readerIndex() + i)));

        // Round-trip decode through the SAME codec (sanity): payload must be stable and
        // leave nothing trailing.
        FriendlyByteBuf rb = new FriendlyByteBuf(Unpooled.copiedBuffer(buf));
        ClientboundSectionBlocksUpdatePacket back = ClientboundSectionBlocksUpdatePacket.STREAM_CODEC.decode(rb);
        if (rb.readableBytes() != 0) {
            throw new IllegalStateException("round-trip left " + rb.readableBytes() + " trailing bytes for " + name);
        }
        FriendlyByteBuf buf2 = new FriendlyByteBuf(Unpooled.buffer());
        ClientboundSectionBlocksUpdatePacket.STREAM_CODEC.encode(buf2, back);
        if (buf2.readableBytes() != n) {
            throw new IllegalStateException("re-encode size mismatch for " + name);
        }
        for (int i = 0; i < n; i++) {
            if (buf2.getByte(buf2.readerIndex() + i) != buf.getByte(buf.readerIndex() + i)) {
                throw new IllegalStateException("re-encode byte mismatch for " + name);
            }
        }

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(sectionLong);
        O.print('\t');
        O.print(changes.length);
        for (long c : changes) { O.print('\t'); O.print(c); }
        O.print('\t');
        O.print(n);
        O.print('\t');
        O.print(hex.toString());
        O.print('\n');
    }
}
