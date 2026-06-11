// Ground truth for net.minecraft.network.protocol.game.ServerboundRecipeBookSeenRecipePacket.
//
// The packet is a record(RecipeDisplayId recipe). Its STREAM_CODEC is
//   StreamCodec.composite(RecipeDisplayId.STREAM_CODEC, ::recipe, ::new)
// and RecipeDisplayId is itself a record(int index) whose STREAM_CODEC is
//   StreamCodec.composite(ByteBufCodecs.VAR_INT, RecipeDisplayId::index, RecipeDisplayId::new)
// (net.minecraft.network.protocol.game.ServerboundRecipeBookSeenRecipePacket lines 9-12,
//  net.minecraft.world.item.crafting.display.RecipeDisplayId lines 7-10).
// composite + ByteBufCodecs.VAR_INT: NO packet-id prefix, just the body, so the whole wire
// payload is exactly ONE VarInt (LEB128, signed, no zig-zag): recipe.index().
//
// ByteBufCodecs.VAR_INT == FriendlyByteBuf.writeVarInt / readVarInt -> negatives encode as
// 5 bytes (no zig-zag). RecipeDisplayId.index is a plain int with no validation, so any
// 32-bit value is legal.
//
// Row format (tab separated):
//   ENC <index-dec> <readableBytes-dec> <hex>      encode: STREAM_CODEC.encode(buf, pkt)
//
// We round-trip-decode every case through the SAME codec and assert index equality as a
// sanity check before emitting. The C++ pkt_recipe_book_seen_recipe_sb_parity rebuilds the
// packet from <index>, re-encodes via PacketBuffer.writeVarInt once, and must match <hex>
// byte-for-byte (+ readableBytes); it also decodes <hex> via one readVarInt and checks the
// recovered int.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.SharedConstants;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundRecipeBookSeenRecipePacket;
import net.minecraft.server.Bootstrap;
import net.minecraft.world.item.crafting.display.RecipeDisplayId;

public class PktRecipeBookSeenRecipeSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();

        // The real StreamCodec for this packet (FriendlyByteBuf, packet). Declared over
        // FriendlyByteBuf (RecipeDisplayId's codec is over ByteBuf, but composite widens it).
        StreamCodec<FriendlyByteBuf, ServerboundRecipeBookSeenRecipePacket> CODEC =
            (StreamCodec<FriendlyByteBuf, ServerboundRecipeBookSeenRecipePacket>)
                ServerboundRecipeBookSeenRecipePacket.STREAM_CODEC;

        // recipe.index() is an arbitrary RecipeDisplayId index; pin every LEB128 byte
        // boundary (1->2->3->4->5 bytes) and the int extremes (negatives encode as 5
        // bytes in LEB128 since VAR_INT does not zig-zag).
        int[] indexes = {
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

        for (int idx : indexes) {
            emit(CODEC, idx);
        }
    }

    static void emit(StreamCodec<FriendlyByteBuf, ServerboundRecipeBookSeenRecipePacket> CODEC,
                     int index) throws Exception {
        // ENC: construct the real packet, encode through the real codec, dump bytes.
        ServerboundRecipeBookSeenRecipePacket pkt =
            new ServerboundRecipeBookSeenRecipePacket(new RecipeDisplayId(index));
        FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
        CODEC.encode(buf, pkt);

        int readable = buf.readableBytes();
        String hex = toHex(buf);

        // Sanity: round-trip decode through the SAME codec and assert equality.
        FriendlyByteBuf rbuf = new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
        ServerboundRecipeBookSeenRecipePacket dec = CODEC.decode(rbuf);
        if (dec.recipe().index() != index) {
            throw new IllegalStateException(
                "round-trip mismatch: in=" + index + " out=" + dec.recipe().index());
        }
        if (rbuf.isReadable()) {
            throw new IllegalStateException(
                "trailing bytes after decode for index=" + index + ": " + rbuf.readableBytes());
        }

        O.print("ENC\t");
        O.print(index);
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
