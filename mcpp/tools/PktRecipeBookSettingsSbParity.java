// Ground truth for ServerboundRecipeBookChangeSettingsPacket's StreamCodec.
//
// net.minecraft.network.protocol.game.ServerboundRecipeBookChangeSettingsPacket
// (26.1.2):
//   private final RecipeBookType bookType;
//   private final boolean isOpen;
//   private final boolean isFiltering;
//   STREAM_CODEC = Packet.codec(::write, ::new)
//   write(FriendlyByteBuf):              (ServerboundRecipeBookChangeSettingsPacket.java:29-33)
//       output.writeEnum(this.bookType);       // VarInt(ordinal)
//       output.writeBoolean(this.isOpen);      // 1 byte 0/1
//       output.writeBoolean(this.isFiltering); // 1 byte 0/1
//   new(FriendlyByteBuf):                (ServerboundRecipeBookChangeSettingsPacket.java:23-27)
//       readEnum(RecipeBookType.class) = getEnumConstants()[readVarInt()]
//       readBoolean(); readBoolean();
//
// FriendlyByteBuf.writeEnum(value) == writeVarInt(value.ordinal())  (FriendlyByteBuf.java:471-473)
// FriendlyByteBuf.readEnum(clazz)  == getEnumConstants()[readVarInt()] (FriendlyByteBuf.java:467-469)
// RecipeBookType ordinals (RecipeBookType.java:3-7):
//   CRAFTING=0, FURNACE=1, BLAST_FURNACE=2, SMOKER=3
//
// Packet.codec -> no packet-id prefix, just the body bytes.
//
// Row format (tab separated):
//   ENUM <ordinal> <name>                              per RecipeBookType constant (id gate)
//   ENC  <name> <ord> <isOpen> <isFiltering> <readableBytes> <hexBytes>
//        STREAM_CODEC.encode(buf, pkt)
//   DEC  <hexBytes> <ordIn> <ordOut> <isOpen> <isFiltering>
//        STREAM_CODEC.decode(buf) round-trip of every field
//
// The C++ side encodes writeVarInt(ordinal)+writeBool+writeBool and decodes the
// inverse, and must match byte-for-byte (ENC) and value-for-value (DEC).
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.codec.StreamCodec;
import net.minecraft.network.protocol.game.ServerboundRecipeBookChangeSettingsPacket;
import net.minecraft.world.inventory.RecipeBookType;

public class PktRecipeBookSettingsSbParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // The real StreamCodec for this packet. STREAM_CODEC is typed
        // StreamCodec<FriendlyByteBuf, ...>; a fresh FriendlyByteBuf satisfies it.
        StreamCodec<FriendlyByteBuf, ServerboundRecipeBookChangeSettingsPacket> CODEC =
            ServerboundRecipeBookChangeSettingsPacket.STREAM_CODEC;

        RecipeBookType[] types = RecipeBookType.values();

        // Id gate: dump ordinal()+name() for every RecipeBookType constant in
        // declaration order. writeEnum uses ordinal(), so this pins the wire id.
        for (RecipeBookType t : types) {
            O.print("ENUM\t");
            O.print(t.ordinal());
            O.print('\t');
            O.print(t.name());
            O.print('\n');
        }

        // ENC / DEC: round-trip every (bookType, isOpen, isFiltering) combo
        // through the REAL STREAM_CODEC. 4 enum values * 2 * 2 = 16 cases.
        boolean[] bools = {false, true};
        for (RecipeBookType t : types) {
            for (boolean isOpen : bools) {
                for (boolean isFiltering : bools) {
                    FriendlyByteBuf buf = new FriendlyByteBuf(Unpooled.buffer());
                    ServerboundRecipeBookChangeSettingsPacket pkt =
                        new ServerboundRecipeBookChangeSettingsPacket(t, isOpen, isFiltering);
                    CODEC.encode(buf, pkt);
                    int readable = buf.readableBytes();
                    String hex = toHex(buf);
                    O.print("ENC\t");
                    O.print(t.name());
                    O.print('\t');
                    O.print(t.ordinal());
                    O.print('\t');
                    O.print(isOpen ? 1 : 0);
                    O.print('\t');
                    O.print(isFiltering ? 1 : 0);
                    O.print('\t');
                    O.print(readable);
                    O.print('\t');
                    O.print(hex);
                    O.print('\n');

                    FriendlyByteBuf rbuf =
                        new FriendlyByteBuf(Unpooled.wrappedBuffer(unhex(hex)));
                    ServerboundRecipeBookChangeSettingsPacket dec = CODEC.decode(rbuf);
                    O.print("DEC\t");
                    O.print(hex);
                    O.print('\t');
                    O.print(t.ordinal());
                    O.print('\t');
                    O.print(dec.getBookType().ordinal());
                    O.print('\t');
                    O.print(dec.isOpen() ? 1 : 0);
                    O.print('\t');
                    O.print(dec.isFiltering() ? 1 : 0);
                    O.print('\n');
                }
            }
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
