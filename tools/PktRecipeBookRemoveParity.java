// Ground truth for ClientboundRecipeBookRemovePacket's StreamCodec.
//
// The packet body is exactly the list codec of RecipeDisplayId:
//   STREAM_CODEC = StreamCodec.composite(
//       RecipeDisplayId.STREAM_CODEC.apply(ByteBufCodecs.list()),
//       ClientboundRecipeBookRemovePacket::recipes,
//       ClientboundRecipeBookRemovePacket::new)
//   (net.minecraft.network.protocol.game.ClientboundRecipeBookRemovePacket lines 11-14.)
//
// RecipeDisplayId is `record RecipeDisplayId(int index)` and its STREAM_CODEC is
//   StreamCodec.composite(ByteBufCodecs.VAR_INT, RecipeDisplayId::index, RecipeDisplayId::new)
//   (net.minecraft.world.item.crafting.display.RecipeDisplayId lines 7-11) -> a single
//   plain VarInt (LEB128, NO zig-zag; negatives encode to 5 bytes).
//
// ByteBufCodecs.list() -> collection(ArrayList::new, elementCodec):
//   encode: writeCount(size) = VarInt(size); then each element.encode (ByteBufCodecs.java
//   lines 428-434, 399-405). decode is the mirror.
//
// So the whole packet body is exactly:
//   VarInt(recipes.size())  then for each:  VarInt(recipe.index())
// which the C++ mc::net::PacketBuffer supports 1:1 via writeVarInt / readVarInt.
//
// Packet.codec is NOT used here; STREAM_CODEC.composite over a plain ByteBuf is the
// body only (no packet-id prefix).
//
// Row format (tab separated):
//   ENC <name> <count> <idx0,idx1,...|-> <readableBytes> <hexBytes>
//     encode: STREAM_CODEC.encode(buf, packet); dump readableBytes + bytes.
//     The index list column is a comma-joined decimal list, or "-" when empty.
// The C++ side reconstructs the list, writes writeVarInt(count) + each writeVarInt(idx)
// and must match byte-for-byte AND on readableBytes; it then decodes the expected bytes
// back and must round-trip the count + every index.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ClientboundRecipeBookRemovePacket;
import net.minecraft.world.item.crafting.display.RecipeDisplayId;

public class PktRecipeBookRemoveParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (plain ByteBuf, packet body only).
        StreamCodec<ByteBuf, ClientboundRecipeBookRemovePacket> CODEC =
            (StreamCodec<ByteBuf, ClientboundRecipeBookRemovePacket>)
                ClientboundRecipeBookRemovePacket.STREAM_CODEC;

        // VarInt byte-boundary values (zig-zag is NOT used here — plain VarInt of the
        // raw int, so negatives encode to 5 bytes). Real recipe-display ids are small
        // non-negative ints, but RecipeDisplayId.STREAM_CODEC passes the index verbatim
        // through ByteBufCodecs.VAR_INT, so we pin the full VarInt 1->2->3->4->5 byte
        // boundaries + sign + extremes.
        int[] V = {
            0, 1, 2, 127, 128, 129, 255, 256, 16383, 16384, 16385,
            2097151, 2097152, 2097153, 268435455, 268435456, 268435457,
            -1, -2, -128, -2097152, Integer.MAX_VALUE, Integer.MIN_VALUE
        };

        // --- empty list (count = 0) ---
        emit("empty", new int[] {}, CODEC);

        // --- singletons: each boundary value alone (count = 1) ---
        for (int v : V) {
            emit("one_" + v, new int[] { v }, CODEC);
        }

        // --- typical small recipe-id runs ---
        emit("run_small", new int[] { 1, 2, 3, 4, 5 }, CODEC);
        emit("run_ids", new int[] { 42, 100, 1000, 32767, 65536 }, CODEC);
        emit("run_seq", new int[] { 10, 11, 12, 13, 14, 15, 16, 17, 18, 19 }, CODEC);

        // --- mixed boundary + sign + extremes in one list (count multi-byte too) ---
        emit("mixed_all", V, CODEC);

        // --- a list whose SIZE itself crosses VarInt byte boundaries: 127 and 128
        //     elements force the count prefix to be 1 then 2 bytes. ---
        int[] c127 = new int[127];
        for (int i = 0; i < c127.length; i++) c127[i] = i;
        emit("count127", c127, CODEC);

        int[] c128 = new int[128];
        for (int i = 0; i < c128.length; i++) c128[i] = i;
        emit("count128", c128, CODEC);

        int[] c200 = new int[200];
        for (int i = 0; i < c200.length; i++) c200[i] = i * 7 - 13; // spread of signs/sizes
        emit("count200", c200, CODEC);
    }

    static void emit(String name, int[] idx,
                     StreamCodec<ByteBuf, ClientboundRecipeBookRemovePacket> codec) {
        List<RecipeDisplayId> list = new ArrayList<>(idx.length);
        for (int v : idx) list.add(new RecipeDisplayId(v));
        ClientboundRecipeBookRemovePacket pkt = new ClientboundRecipeBookRemovePacket(list);

        // ENC: encode through the real codec, dump readableBytes + body bytes.
        ByteBuf buf = Unpooled.buffer();
        codec.encode(buf, pkt);
        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Round-trip decode through the SAME codec (sanity).
        ByteBuf rbuf = Unpooled.wrappedBuffer(unhex(hex));
        ClientboundRecipeBookRemovePacket dec = codec.decode(rbuf);
        List<RecipeDisplayId> back = dec.recipes();
        if (back.size() != idx.length)
            throw new IllegalStateException(name + ": round-trip size " + back.size() + " != " + idx.length);
        for (int i = 0; i < idx.length; i++)
            if (back.get(i).index() != idx[i])
                throw new IllegalStateException(name + ": round-trip idx[" + i + "] " + back.get(i).index() + " != " + idx[i]);

        O.print("ENC\t");
        O.print(name);
        O.print('\t');
        O.print(idx.length);
        O.print('\t');
        O.print(joinIds(idx));
        O.print('\t');
        O.print(readable);
        O.print('\t');
        O.print(hex);
        O.print('\n');
    }

    static String joinIds(int[] idx) {
        if (idx.length == 0) return "-";
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < idx.length; i++) {
            if (i > 0) sb.append(',');
            sb.append(idx[i]);
        }
        return sb.toString();
    }

    static String toHex(ByteBuf b) {
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
