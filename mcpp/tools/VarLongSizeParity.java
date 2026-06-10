// Ground truth for net.minecraft.network.VarLong.getByteSize(long) AND the
// VarLong.write(...) byte encoding, produced with the REAL decompiled class.
//
// Both methods are `public static`, so they are invoked directly (no reflection
// needed). The C++ varlong_size_parity gate must reproduce, for every value:
//   * getByteSize -> identical int (1..10)
//   * write       -> identical byte sequence (hex)
//
// Emits tab-separated rows:
//   SIZE  <value(decimal long)>  <byteSize(decimal)>  <encoded hex>
import io.netty.buffer.ByteBuf;
import io.netty.buffer.Unpooled;
import net.minecraft.network.VarLong;

public class VarLongSizeParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Boundary + sweep of FINITE/PHYSICAL longs. VarLong handles the full
        // signed 64-bit range (negatives encode to 10 bytes), so the battery
        // spans every byte-size class plus exact powers-of-128 boundaries.
        java.util.LinkedHashSet<Long> set = new java.util.LinkedHashSet<>();

        // Exact byte-size boundaries: 2^(7k) - 1 and 2^(7k) for k = 1..9.
        for (int k = 1; k <= 9; k++) {
            long boundary = (k * 7 >= 63) ? Long.MAX_VALUE : (1L << (k * 7));
            set.add(boundary - 1L);
            set.add(boundary);
            set.add(boundary + 1L);
        }

        // Small values 0..300 (covers 1- and 2-byte transitions densely).
        for (long v = 0; v <= 300; v++) set.add(v);

        // Powers of two across the whole positive range.
        for (int b = 0; b < 63; b++) set.add(1L << b);

        // A spread of arbitrary mid-range positives.
        long[] mids = {
            1000L, 65535L, 65536L, 1000000L, 16777215L, 16777216L,
            2147483647L, 2147483648L, 4294967295L, 4294967296L,
            34359738367L, 34359738368L, 1234567890L, 9876543210L,
            1099511627775L, 1099511627776L, 281474976710655L, 281474976710656L,
            72057594037927935L, 72057594037927936L, 1234567890123456789L,
            Long.MAX_VALUE, Long.MAX_VALUE - 1L,
        };
        for (long v : mids) set.add(v);

        // Negatives: every negative long has the sign bit set, so getByteSize
        // returns 10 and write emits 10 bytes. Include a representative spread.
        long[] negs = {
            -1L, -2L, -127L, -128L, -129L, -255L, -256L,
            -1000L, -65536L, -2147483648L, -4294967296L,
            -1234567890123456789L, Long.MIN_VALUE, Long.MIN_VALUE + 1L,
        };
        for (long v : negs) set.add(v);

        for (long v : set) {
            int size = VarLong.getByteSize(v);
            ByteBuf b = Unpooled.buffer();
            VarLong.write(b, v);
            StringBuilder hex = new StringBuilder();
            while (b.isReadable()) hex.append(String.format("%02x", b.readByte()));
            O.print("SIZE\t");
            O.print(v);
            O.print('\t');
            O.print(size);
            O.print('\t');
            O.print(hex.toString());
            O.print('\n');
        }
    }
}
