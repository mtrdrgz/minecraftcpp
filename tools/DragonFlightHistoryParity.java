import java.lang.reflect.Field;
import java.lang.reflect.Method;

// Ground truth for mcpp/src/world/entity/boss/enderdragon/DragonFlightHistory.h.
// Certifies the WHOLE of net.minecraft.world.entity.boss.enderdragon.DragonFlightHistory
// (Minecraft 26.1.2) by driving the REAL class — never replicating any body here.
//
// DragonFlightHistory is a fully self-contained ring buffer (no Level/Entity/RNG/GPU),
// so we construct the REAL object via its public constructor and invoke the REAL
// record / copyFrom / get(int) / get(int,float) methods reflectively, reading back the
// REAL Sample record (its y()/yRot() accessors) and the private `head` field. Every
// emitted value is produced by real net.minecraft code.
//
// Row families (all values from the REAL Java code):
//
//   RECORD  seed nRecords delay  | yBits(d) yRotBits(f) head
//        Fresh history, then apply a deterministic sequence of `nRecords` record(y,yRot)
//        calls (y/yRot derived from `seed` + index — finite physical values), then read
//        get(delay). Emits the resulting Sample (y as raw double bits, yRot as raw float
//        bits) plus the private head index. Exercises the head<0 back-fill on the first
//        record, the ++head == 64 wrap across long sequences, and the
//        (head - delay) & 63 negative-safe index arithmetic.
//
//   INTERP  seed nRecords delay partialBits(f) | yBits(d) yRotBits(f)
//        Same setup, then get(delay, partialTicks). Emits the interpolated Sample.
//        Pins Mth.lerp(partialTicks, old.y, new.y) and the rotLerp/wrapDegrees
//        shortest-arc yRot blend (including the seam where consecutive yRot samples
//        straddle +/-180 degrees, which a plain lerp would get wrong).
//
//   COPY    seed nRecords delay  | yBits(d) yRotBits(f) head srcHead
//        Build a source history (nRecords records), copyFrom it into a fresh dest, then
//        read dest.get(delay) and both heads. Pins System.arraycopy of all 64 samples
//        plus the head field copy.
//
// Doubles -> Double.doubleToRawLongBits (%016x); floats -> Float.floatToRawIntBits
// (%08x); ints decimal. All inputs are finite physical values (no NaN/Inf/neg-zero).
@SuppressWarnings({"deprecation", "unchecked"})
public class DragonFlightHistoryParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }
    static String f(float v)  { return String.format("%08x", Float.floatToRawIntBits(v)); }

    static Class<?> HIST;
    static Class<?> SAMPLE;
    static java.lang.reflect.Constructor<?> CTOR;
    static Method M_record, M_copyFrom, M_get1, M_get2;
    static Method M_sampleY, M_sampleYRot;
    static Field F_head;

    static void init() throws Exception {
        HIST = Class.forName("net.minecraft.world.entity.boss.enderdragon.DragonFlightHistory");
        SAMPLE = Class.forName("net.minecraft.world.entity.boss.enderdragon.DragonFlightHistory$Sample");
        CTOR = HIST.getDeclaredConstructor();
        CTOR.setAccessible(true);
        M_record   = HIST.getDeclaredMethod("record", double.class, float.class);
        M_copyFrom = HIST.getDeclaredMethod("copyFrom", HIST);
        M_get1     = HIST.getDeclaredMethod("get", int.class);
        M_get2     = HIST.getDeclaredMethod("get", int.class, float.class);
        M_record.setAccessible(true);
        M_copyFrom.setAccessible(true);
        M_get1.setAccessible(true);
        M_get2.setAccessible(true);
        // record component accessors y()/yRot()
        M_sampleY    = SAMPLE.getDeclaredMethod("y");
        M_sampleYRot = SAMPLE.getDeclaredMethod("yRot");
        M_sampleY.setAccessible(true);
        M_sampleYRot.setAccessible(true);
        F_head = HIST.getDeclaredField("head");
        F_head.setAccessible(true);
    }

    static Object freshHistory() throws Exception {
        return CTOR.newInstance();
    }

    // Deterministic finite y/yRot for record #i under a given seed. Pure rational
    // arithmetic only (NO Math.sin etc.): the y history values are arbitrary finite
    // physical inputs to the ring buffer; avoiding transcendentals keeps the C++
    // replay bit-identical (no libm intrinsic gap). The exact formula is irrelevant
    // to what is certified (the buffer + lerp/rotLerp); only that both sides agree.
    static double yFor(int seed, int i) {
        // A spread of magnitudes/signs; all finite physical y values (block heights).
        return (seed * 13 + i * 7) * 0.5 - 96.0 + ((i * 73 % 17) - 8) * 0.625;
    }
    static float yRotFor(int seed, int i) {
        // Yaw in degrees, deliberately sweeping past +/-180 so wrapDegrees matters.
        return (float) ((seed * 37 + i * 53) % 720) - 360.0F + (i % 5) * 11.25F;
    }

    static void applyRecords(Object h, int seed, int n) throws Exception {
        for (int i = 0; i < n; i++) {
            M_record.invoke(h, yFor(seed, i), yRotFor(seed, i));
        }
    }

    static int headOf(Object h) throws Exception { return F_head.getInt(h); }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        init();
        long n = 0;

        int[] seeds       = { 0, 1, 2, 3, 7, 13, 42, 100, 255 };
        // 0 records exercises the all-zero constructor fill + head == -1 index path.
        int[] recordCounts = { 0, 1, 2, 3, 4, 8, 16, 31, 32, 33, 63, 64, 65, 100, 127, 128, 200 };
        int[] delays       = { 0, 1, 2, 3, 5, 10, 31, 32, 50, 62, 63 };
        float[] partials   = { 0.0f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 1.0f, 0.33333334f, 0.6666667f };

        for (int seed : seeds) {
            for (int nr : recordCounts) {
                Object src = freshHistory();
                applyRecords(src, seed, nr);
                int srcHead = headOf(src);

                for (int delay : delays) {
                    // RECORD: get(delay)
                    Object s1 = M_get1.invoke(src, delay);
                    double y1 = (Double) M_sampleY.invoke(s1);
                    float yr1 = (Float) M_sampleYRot.invoke(s1);
                    O.println("RECORD\t" + seed + "\t" + nr + "\t" + delay
                            + "\t" + d(y1) + "\t" + f(yr1) + "\t" + srcHead);
                    n++;

                    // INTERP: get(delay, partialTicks)
                    for (float p : partials) {
                        Object s2 = M_get2.invoke(src, delay, p);
                        double y2 = (Double) M_sampleY.invoke(s2);
                        float yr2 = (Float) M_sampleYRot.invoke(s2);
                        O.println("INTERP\t" + seed + "\t" + nr + "\t" + delay
                                + "\t" + f(p) + "\t" + d(y2) + "\t" + f(yr2));
                        n++;
                    }

                    // COPY: copyFrom(src) into a fresh dest, then dest.get(delay)
                    Object dst = freshHistory();
                    M_copyFrom.invoke(dst, src);
                    int dstHead = headOf(dst);
                    Object s3 = M_get1.invoke(dst, delay);
                    double y3 = (Double) M_sampleY.invoke(s3);
                    float yr3 = (Float) M_sampleYRot.invoke(s3);
                    O.println("COPY\t" + seed + "\t" + nr + "\t" + delay
                            + "\t" + d(y3) + "\t" + f(yr3) + "\t" + dstHead + "\t" + srcHead);
                    n++;
                }
            }
        }

        System.err.println("rows=" + n);
    }
}
