// Ground truth for net.minecraft.network.VarInt: getByteSize(int) and the encode
// bytes (VarInt.write) over a wide int range, emitted with the REAL class.
//
// The C++ varint_size_parity gate must reproduce, BIT-FOR-BIT:
//   * SIZE\t<value>\t<getByteSize>          (decimal int -> decimal int)
//   * ENC \t<value>\t<hex encode bytes>      (decimal int -> lowercase hex)
//
// VarInt.getByteSize and VarInt.write are both `public static`, so no reflection
// or bootstrap is needed.
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import java.util.LinkedHashSet;
import net.minecraft.network.VarInt;

public class VarIntSizeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Build a finite/physical battery of int inputs that exercises every
        // getByteSize bucket boundary (1..5 bytes) and the encode continuation
        // logic. All values are real 32-bit ints (no NaN/Inf — VarInt takes int).
        LinkedHashSet<Integer> vals = new LinkedHashSet<>();

        // Exact 7-bit-group boundaries: each getByteSize bucket edge +/- 1.
        // size==1: [0, 127]; ==2: [128, 16383]; ==3: [16384, 2097151];
        // ==4: [2097152, 268435455]; ==5: [268435456, .. ] and all negatives.
        int[] edges = {
            0, 1, 2, 126, 127, 128, 129, 255, 256,
            16382, 16383, 16384, 16385, 65535, 65536,
            2097150, 2097151, 2097152, 2097153, 16777215, 16777216,
            268435454, 268435455, 268435456, 268435457,
            1073741823, 1073741824,
            Integer.MAX_VALUE - 1, Integer.MAX_VALUE,
            -1, -2, -127, -128, -255, -256,
            -16384, -2097152, -268435456,
            Integer.MIN_VALUE, Integer.MIN_VALUE + 1
        };
        for (int e : edges) vals.add(e);

        // Powers of two and (2^k - 1) masks across the whole int width.
        for (int k = 0; k < 31; k++) {
            vals.add(1 << k);
            vals.add((1 << k) - 1);
            vals.add((1 << k) + 1);
        }
        // A spread of large/negative values via a deterministic LCG so the
        // battery isn't only near boundaries.
        long s = 0x9E3779B97F4A7C15L;
        for (int i = 0; i < 200; i++) {
            s = s * 6364136223846793005L + 1442695040888963407L;
            vals.add((int)(s >>> 32));
        }

        for (int v : vals) {
            O.print("SIZE\t");
            O.print(v);
            O.print('\t');
            O.println(VarInt.getByteSize(v));

            ByteBuf b = Unpooled.buffer();
            VarInt.write(b, v);
            StringBuilder hex = new StringBuilder();
            while (b.isReadable()) hex.append(String.format("%02x", b.readByte()));
            O.print("ENC\t");
            O.print(v);
            O.print('\t');
            O.println(hex);
        }
    }
}
