// Ground truth for net.minecraft.util.datafix.PackedBitStorage (Minecraft 26.1.2).
//
// PackedBitStorage is the LEGACY (pre-1.16) bit-packed integer array used by the
// DataFixerUpper chunk fixers: `size` values of `bits` bits each, tightly packed
// across a long[] with NO long-boundary alignment — a single value may straddle
// two adjacent longs. We drive the REAL class (public ctor + set/get, private
// `data` field via reflection) and emit, per scenario:
//
//   * the exact long[] backing store after a deterministic write sequence
//     (so the C++ port reproduces the packing layout BIT-FOR-BIT), and
//   * every get(i) readback (so the unpack path is checked independently).
//
// Row TAGs (tab-separated; longs/ints decimal — longs may be negative because the
// top bit of a packed long is a real data bit, printed as a signed Java long):
//   CFG   <scen> <bits> <size> <rawLen>            scenario config + long[] length
//   WSET  <scen> <index> <value>                   a set(index,value) input performed
//   RAW   <scen> <longIndex> <longValueDecimal>    backing data[longIndex] after all writes
//   GET   <scen> <index> <value>                   expected get(index) after all writes
//
// The C++ side replays the WSET inputs into a fresh PackedBitStorage(bits,size),
// then asserts data[]==RAW (the packed layout) and get(i)==GET (the unpack), all
// exact-integer. WSET fully specifies the inputs; RAW/GET are the derived truth.

import java.lang.reflect.Field;
import net.minecraft.util.datafix.PackedBitStorage;

public class PackedBitStorageParity {
    static final java.io.PrintStream O = System.out;
    static Field F_data; // private final long[] data

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        F_data = PackedBitStorage.class.getDeclaredField("data");
        F_data.setAccessible(true);

        // bits 1..32. For each bit-width, pick a size that:
        //  - is not a multiple of (64/bits) so values straddle long boundaries, and
        //  - is large enough to fill several longs.
        // Values written are deterministic but exercise the full value range,
        // including the all-ones (== mask) top value which sets the highest data bit
        // of a long (producing negative signed longs — a key 1:1 trap).
        for (int bits = 1; bits <= 32; bits++) {
            int size = chooseSize(bits);
            scenario("b" + bits, bits, size);
        }

        // A few hand-picked widths that are notorious in the old palette format
        // (4, 5, 6, 13 — the bit-widths real chunk sections used), with sizes that
        // force the cross-long straddle on nearly every value.
        scenario("palette4", 4, 4096);
        scenario("palette5", 5, 4096);
        scenario("palette6", 6, 4096);
        scenario("palette13", 13, 4096);

        // Edge: size==0 (empty), size==1, and a size whose total bit count lands
        // exactly on a 64-bit boundary (no straddle on the last value).
        scenario("empty7", 7, 0);
        scenario("one31", 31, 1);
        scenario("exact8", 8, 8);   // 8*8 = 64 bits exactly -> 1 long, aligned

        O.flush();
    }

    static int chooseSize(int bits) {
        // Aim for ~6 longs of storage, then nudge so it's not a clean multiple of
        // the values-per-long (64/bits), guaranteeing straddling values.
        int perLong = Math.max(1, 64 / bits);
        int base = perLong * 6 + 3;       // a few extra to break alignment
        return Math.max(base, 5);
    }

    static void scenario(String scen, int bits, int size) throws Exception {
        long mask = bits >= 64 ? -1L : (1L << bits) - 1L; // bits<=32 so simple
        PackedBitStorage st = new PackedBitStorage(bits, size);
        long[] raw = (long[]) F_data.get(st);
        O.println("CFG\t" + scen + "\t" + bits + "\t" + size + "\t" + raw.length);

        // PackedBitStorage.set takes an `int value`, and Validate.inclusiveBetween(
        // 0L, mask, value) WIDENS that int to long for the check — so any value with
        // bit 31 set would arrive as a negative long and be rejected. Therefore the
        // largest value this legacy API can store is min(mask, Integer.MAX_VALUE).
        // For bits<=31 that is the true all-ones top value (still sets the highest
        // data bit of a long, producing negative signed longs in the backing store —
        // the key 1:1 trap); for bits==32 the class genuinely cannot express
        // bit-31-set values through this int parameter, and our GT respects that.
        long topValue = Math.min(mask, (long) Integer.MAX_VALUE);
        // Deterministic value generator: a multiplicative LCG-ish walk masked to the
        // value range, hitting 0, topValue, and a spread of interior values.
        long state = 0x9E3779B97F4A7C15L + (long) bits * 0x6C8E9CF570932BD5L;
        for (int i = 0; i < size; i++) {
            int value;
            // Salt the sequence so the boundary values appear at a few positions.
            if (i == 0) value = 0;
            else if (i == 1) value = (int) topValue;        // max -> top data bit
            else if (i % 7 == 0) value = (int) topValue;
            else if (i % 11 == 0) value = 0;
            else {
                state = state * 6364136223846793005L + 1442695040888963407L;
                long v = (state >>> 33) % (topValue + 1);   // in [0, topValue]
                value = (int) v;
            }
            st.set(i, value);
            O.println("WSET\t" + scen + "\t" + i + "\t" + value);
        }

        // Emit the backing long[] verbatim (the packed layout — the real trap).
        long[] after = (long[]) F_data.get(st);
        for (int j = 0; j < after.length; j++) {
            O.println("RAW\t" + scen + "\t" + j + "\t" + after[j]);
        }
        // Emit every readback.
        for (int i = 0; i < size; i++) {
            O.println("GET\t" + scen + "\t" + i + "\t" + st.get(i));
        }
    }
}
