// Ground truth for the network primitive codec (FriendlyByteBuf): VarInt, VarLong,
// writeUtf, network NBT framing — encoded with the REAL classes, emitted as hex.
// The C++ packet_buffer_parity must reproduce every encoding byte-for-byte and
// round-trip-decode them to identical values.
//
//   ENC <name> <hex>
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.StringTag;
import net.minecraft.network.FriendlyByteBuf;
import net.minecraft.network.VarInt;
import net.minecraft.network.VarLong;

public class FriendlyByteBufParity {
    static final StringBuilder OUT = new StringBuilder();

    public static void main(String[] args) {
        int[] varints = { 0, 1, 2, 127, 128, 255, 16383, 16384, 2097151, 2097152,
                          268435455, 268435456, Integer.MAX_VALUE, -1, -2147483648 };
        for (int v : varints) {
            ByteBuf b = Unpooled.buffer();
            VarInt.write(b, v);
            emit("varint_" + v, b);
        }
        long[] varlongs = { 0L, 1L, 127L, 128L, 16383L, 16384L, 34359738367L,
                            Long.MAX_VALUE, -1L, Long.MIN_VALUE, -1234567890123456789L };
        for (long v : varlongs) {
            ByteBuf b = Unpooled.buffer();
            VarLong.write(b, v);
            emit("varlong_" + v, b);
        }
        String[][] utfs = { { "empty", "" }, { "ascii", "hello world" },
                            { "unicode", "niño 中文 😀 ü" } };
        for (String[] s : utfs) {
            FriendlyByteBuf b = new FriendlyByteBuf(Unpooled.buffer());
            b.writeUtf(s[1]);
            emit("utf_" + s[0], b);
        }
        // network NBT: unnamed root; null -> EndTag byte
        CompoundTag tag = new CompoundTag();
        tag.putInt("x", 42);
        tag.putString("s", "ü");
        ListTag list = new ListTag();
        list.add(StringTag.valueOf("a"));
        tag.put("l", list);
        FriendlyByteBuf nb = new FriendlyByteBuf(Unpooled.buffer());
        nb.writeNbt(tag);
        emit("nbt_compound", nb);
        FriendlyByteBuf nn = new FriendlyByteBuf(Unpooled.buffer());
        nn.writeNbt(null);
        emit("nbt_null", nn);

        System.out.print(OUT);
    }

    static void emit(String name, ByteBuf b) {
        StringBuilder hex = new StringBuilder();
        while (b.isReadable()) hex.append(String.format("%02x", b.readByte()));
        OUT.append("ENC\t").append(name).append('\t').append(hex).append('\n');
    }
}
