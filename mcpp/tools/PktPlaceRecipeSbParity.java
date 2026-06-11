// Ground truth for net.minecraft.network.protocol.game.ServerboundPlaceRecipePacket's
// StreamCodec. Strict 1:1 reverse-engineering reference for the C++ parity gate.
//
// The packet is a record (ServerboundPlaceRecipePacket.java:10-19) encoded by a
// StreamCodec.composite of three primitive codecs, in field order:
//   ByteBufCodecs.CONTAINER_ID  -> containerId   (FriendlyByteBuf.writeContainerId
//                                                  == VarInt.write, a LEB128 VarInt)
//   RecipeDisplayId.STREAM_CODEC-> recipe        (composite of ByteBufCodecs.VAR_INT
//                                                  over RecipeDisplayId.index; a VarInt)
//   ByteBufCodecs.BOOL          -> useMaxItems   (single byte 0/1)
//
// So the wire body is exactly:
//   writeVarInt(containerId) + writeVarInt(recipe.index) + writeBoolean(useMaxItems)
// composite codecs carry NO packet-id or length prefix.
//
// CONTAINER_ID resolves to FriendlyByteBuf.writeContainerId(out, id) == VarInt.write
// (FriendlyByteBuf.java:679-681). RecipeDisplayId is a record(int index) whose
// STREAM_CODEC is composite(ByteBufCodecs.VAR_INT, RecipeDisplayId::index, ...).
//
// Row formats (tab separated). All integers decimal; useMaxItems decimal 0/1;
// hexBytes is lowercase %02x of the encoded body.
//   ENC \t <name> \t <containerId> \t <recipeIndex> \t <useMaxItems> \t <readableBytes> \t <hexBytes>
// The C++ gate re-encodes the SAME fields through the certified PacketBuffer
// (writeVarInt(containerId) + writeVarInt(recipeIndex) + writeBool(useMaxItems)) and
// must match byte-for-byte, then round-trips the bytes back to the three fields.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundPlaceRecipePacket;
import net.minecraft.world.item.crafting.display.RecipeDisplayId;

public class PktPlaceRecipeSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet).
        StreamCodec<FriendlyByteBuf, ServerboundPlaceRecipePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundPlaceRecipePacket>)
                ServerboundPlaceRecipePacket.STREAM_CODEC;

        // Finite / physical battery. Columns: name, containerId, recipeIndex, useMaxItems.
        // containerId is a CONTAINER_ID (VarInt): exercise 0, every VarInt byte boundary
        // (127/128/16383/16384/2097151/2097152/268435455) and Integer.MAX (5-byte VarInt).
        // Container ids are small/non-negative in practice, but the codec is a raw VarInt
        // so we still probe full positive VarInt width. recipeIndex is a plain VarInt:
        // same boundary sweep plus Integer.MAX/MIN (negative -> 5-byte VarInt).
        // useMaxItems is a BOOL.
        int[][] cases = {
            // {containerId, recipeIndex, useMaxItems}
            {0, 0, 0},
            {0, 0, 1},
            {1, 1, 1},
            {1, 5, 0},
            {2, 127, 1},
            {3, 128, 0},
            {127, 16383, 1},
            {128, 16384, 0},
            {16383, 2097151, 1},
            {16384, 2097152, 0},
            {2097151, 268435455, 1},
            {2097152, 2147483647, 0},
            {268435455, 0, 1},
            {2147483647, 42, 0},
            {7, -1, 1},
            {9, -2147483648, 0},
            {100, 99999, 1},
            {255, 1234567, 0},
        };

        for (int[] c : cases) {
            int containerId = c[0];
            int recipeIndex = c[1];
            boolean useMaxItems = c[2] != 0;

            // ENC: encode through the REAL codec, dump the body bytes.
            FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
            ServerboundPlaceRecipePacket pkt =
                new ServerboundPlaceRecipePacket(containerId, new RecipeDisplayId(recipeIndex), useMaxItems);
            CODEC.encode(buf, pkt);
            int readable = buf.readableBytes();
            String hex = toHex(buf);

            // Round-trip decode through the SAME codec; assert all fields equal.
            FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
            ServerboundPlaceRecipePacket dec = CODEC.decode(rbuf);
            if (dec.containerId() != containerId)
                throw new IllegalStateException("containerId roundtrip " + dec.containerId()
                    + " != " + containerId);
            if (dec.recipe().index() != recipeIndex)
                throw new IllegalStateException("recipeIndex roundtrip " + dec.recipe().index()
                    + " != " + recipeIndex);
            if (dec.useMaxItems() != useMaxItems)
                throw new IllegalStateException("useMaxItems roundtrip " + dec.useMaxItems()
                    + " != " + useMaxItems);

            String name = "case_c" + containerId + "_r" + recipeIndex + "_m" + (useMaxItems ? 1 : 0);
            O.print("ENC\t");
            O.print(name);
            O.print('\t');
            O.print(containerId);
            O.print('\t');
            O.print(recipeIndex);
            O.print('\t');
            O.print(useMaxItems ? 1 : 0);
            O.print('\t');
            O.print(readable);
            O.print('\t');
            O.print(hex);
            O.print('\n');
        }
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
