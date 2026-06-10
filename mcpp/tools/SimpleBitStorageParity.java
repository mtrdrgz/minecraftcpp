// Ground-truth generator for net.minecraft.util.SimpleBitStorage (the packed
// long-array chunk/heightmap storage). Pure; no Bootstrap. Dumps the private MAGIC
// table (reflection) for the C++ to embed, plus per-case packed data[], cellIndex,
// get(), and getAndSet sequences. The C++ test recomputes and must match exactly.
//
//   tools/run_groundtruth.ps1 -Tool SimpleBitStorageParity -Out mcpp/build/bit_storage.tsv
//
// Deterministic per-index value (both sides compute identically):
//   val(i, mask) = (int)(((long)i * 2654435761L + 12345L) & mask)

import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import net.minecraft.util.SimpleBitStorage;

public class SimpleBitStorageParity {
    static final java.io.PrintStream O = System.out;

    static long mask(int bits) { return (1L << bits) - 1L; }
    static int val(int i, long mask) { return (int)(((long) i * 2654435761L + 12345L) & mask); }

    static final int[] BITS = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 12, 15, 16, 20, 25, 31, 32 };
    static final int[] SIZES = { 1, 7, 16, 64, 100, 256, 257, 4096 };

    public static void main(String[] args) throws Exception {
        // Dump MAGIC[] via reflection (raw ints, decimal).
        Field magicF = SimpleBitStorage.class.getDeclaredField("MAGIC");
        magicF.setAccessible(true);
        int[] magic = (int[]) magicF.get(null);
        for (int k = 0; k < magic.length; k++) O.println("MAGIC\t" + k + "\t" + magic[k]);
        O.println("MAGICLEN\t" + magic.length);

        Constructor<SimpleBitStorage> ctorValues = SimpleBitStorage.class.getConstructor(int.class, int.class, int[].class);
        Method getRaw = SimpleBitStorage.class.getMethod("getRaw");
        Method get = SimpleBitStorage.class.getMethod("get", int.class);
        Method getAndSet = SimpleBitStorage.class.getMethod("getAndSet", int.class, int.class);
        Method cellIndexM = SimpleBitStorage.class.getDeclaredMethod("cellIndex", int.class);
        cellIndexM.setAccessible(true);

        for (int bits : BITS) {
            long m = mask(bits);
            for (int size : SIZES) {
                int[] values = new int[size];
                for (int i = 0; i < size; i++) values[i] = val(i, m);
                SimpleBitStorage sbs = ctorValues.newInstance(bits, size, values);

                long[] raw = (long[]) getRaw.invoke(sbs);
                O.println("CTORLEN\t" + bits + "\t" + size + "\t" + raw.length);
                for (int li = 0; li < raw.length; li++)
                    O.println("RAW\t" + bits + "\t" + size + "\t" + li + "\t" + raw[li]);

                // cellIndex over all bit indices 0..size-1
                for (int idx = 0; idx < size; idx++)
                    O.println("CELL\t" + bits + "\t" + size + "\t" + idx + "\t" + (int) cellIndexM.invoke(sbs, idx));

                // get(index) round-trip over all indices
                for (int idx = 0; idx < size; idx++)
                    O.println("GET\t" + bits + "\t" + size + "\t" + idx + "\t" + (int) get.invoke(sbs, idx));
            }
        }

        // getAndSet sequence on a fresh storage, then dump final raw.
        for (int bits : new int[]{ 4, 5, 15, 16 }) {
            long m = mask(bits);
            int size = 64;
            SimpleBitStorage sbs = ctorValues.newInstance(bits, size, new int[size]);
            for (int step = 0; step < size; step++) {
                int idx = (step * 37) % size;
                int newValue = (int)((step * 911L) & m);
                int old = (int) getAndSet.invoke(sbs, idx, newValue);
                O.println("GAS\t" + bits + "\t" + size + "\t" + idx + "\t" + newValue + "\t" + old);
            }
            long[] raw = (long[]) getRaw.invoke(sbs);
            for (int li = 0; li < raw.length; li++)
                O.println("GASRAW\t" + bits + "\t" + size + "\t" + li + "\t" + raw[li]);
        }
    }
}
