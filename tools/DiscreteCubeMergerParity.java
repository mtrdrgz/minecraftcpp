import java.lang.reflect.Constructor;
import java.lang.reflect.Field;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.List;

import it.unimi.dsi.fastutil.doubles.DoubleList;
import net.minecraft.world.phys.shapes.DiscreteCubeMerger;
// NOTE: net.minecraft.world.phys.shapes.IndexMerger (and its nested IndexConsumer)
// is PACKAGE-PRIVATE in 26.1.2 (IndexMerger.java:5 — `interface IndexMerger`, no
// `public`), so it cannot be imported/referenced from this default-package tool.
// DiscreteCubeMerger itself is `public final` (DiscreteCubeMerger.java:6) but its
// ctor is package-private and forMergedIndexes(IndexConsumer) takes the inaccessible
// nested interface. We therefore construct via reflection (already done below) and
// drive forMergedIndexes via a dynamic Proxy of the IndexConsumer interface that we
// load reflectively (see capture()).

// Ground truth for mcpp/src/world/phys/shapes/IndexMerger.h (class mc::DiscreteCubeMerger),
// which verifies the existing C++ port against the REAL
// net.minecraft.world.phys.shapes.DiscreteCubeMerger.
//
// DiscreteCubeMerger.java (Minecraft 26.1.2):
//   ctor(int firstSize, int secondSize):
//       result   = new CubePointRange((int)Shapes.lcm(firstSize, secondSize))
//                  where Shapes.lcm(a,b) = (long)a * (b / IntMath.gcd(a,b))
//       gcd      = IntMath.gcd(firstSize, secondSize)
//       firstDiv = firstSize / gcd
//       secondDiv= secondSize / gcd
//   size()              = result.size()                       (= lcm + 1)
//   getList()           = result (a CubePointRange)
//   forMergedIndexes(c) : for i in [0, result.size()-1):
//                            c.merge(i / secondDiv, i / firstDiv, i)
//
// The ctor is package-private and firstDiv/secondDiv/result are private, so this
// tool uses reflection + setAccessible to construct an instance and exercise it,
// and drives the public IndexMerger interface (size/getList/forMergedIndexes).
//
// Inputs are FINITE/PHYSICAL: positive sizes as produced by Shapes.createIndexMerger,
// i.e. firstSize,secondSize = CubePointRange.size()-1 >= 1, guarded so that the
// merge grid lcm stays <= 256 (cost*lcm <= 256L at the call site). We additionally
// probe a handful of larger lcm grids that the ctor itself accepts, to fully cover
// the integer division / gcd arithmetic.
//
// Row formats (TAG \t inputs... \t outputs...):
//   CTOR   firstSize secondSize | gcd firstDiv secondDiv resultParts size
//   GET    firstSize secondSize index | getDouble(double, %016x raw bits of result list)
//   MERGE  firstSize secondSize | count  i0:first0:second0  i1:first1:second1  ...
//            (one tab-separated token per merged index i in [0, size-1): "i:firstIdx:secondIdx")
public class DiscreteCubeMergerParity {
    static final java.io.PrintStream O = System.out;

    static String d(double v) { return String.format("%016x", Double.doubleToRawLongBits(v)); }

    static final Constructor<DiscreteCubeMerger> CTOR;
    static final Field F_RESULT, F_FIRSTDIV, F_SECONDDIV;
    static {
        try {
            CTOR = DiscreteCubeMerger.class.getDeclaredConstructor(int.class, int.class);
            CTOR.setAccessible(true);
            F_RESULT = DiscreteCubeMerger.class.getDeclaredField("result");
            F_RESULT.setAccessible(true);
            F_FIRSTDIV = DiscreteCubeMerger.class.getDeclaredField("firstDiv");
            F_FIRSTDIV.setAccessible(true);
            F_SECONDDIV = DiscreteCubeMerger.class.getDeclaredField("secondDiv");
            F_SECONDDIV.setAccessible(true);
        } catch (ReflectiveOperationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    static DiscreteCubeMerger make(int firstSize, int secondSize) throws Exception {
        return CTOR.newInstance(firstSize, secondSize);
    }

    // The package-private nested interface net.minecraft.world.phys.shapes.IndexMerger$IndexConsumer
    // (IndexMerger.java:12-14: boolean merge(int firstIndex, int secondIndex, int resultIndex)),
    // and the public DiscreteCubeMerger.forMergedIndexes(IndexConsumer) that consumes it
    // (DiscreteCubeMerger.java:18-29). Both loaded reflectively because the param type is
    // inaccessible from this default package.
    static final Class<?> INDEX_CONSUMER;
    static final Method M_FOR_MERGED_INDEXES;
    static {
        try {
            INDEX_CONSUMER = Class.forName("net.minecraft.world.phys.shapes.IndexMerger$IndexConsumer");
            M_FOR_MERGED_INDEXES = DiscreteCubeMerger.class.getMethod("forMergedIndexes", INDEX_CONSUMER);
            M_FOR_MERGED_INDEXES.setAccessible(true);
        } catch (ReflectiveOperationException e) {
            throw new ExceptionInInitializerError(e);
        }
    }

    // Capture forMergedIndexes via an IndexConsumer that records (first, second, result).
    // IndexConsumer is package-private, so we implement it with a dynamic Proxy and invoke
    // forMergedIndexes reflectively. DiscreteCubeMerger.forMergedIndexes calls
    // consumer.merge(i/secondDiv, i/firstDiv, i) (DiscreteCubeMerger.java:23), i.e. the proxy
    // receives (firstIndex=i/secondDiv, secondIndex=i/firstDiv, resultIndex=i); we record
    // {firstIndex, secondIndex, resultIndex} exactly as the original lambda did.
    static List<int[]> capture(DiscreteCubeMerger m) throws Exception {
        final List<int[]> rows = new ArrayList<>();
        InvocationHandler h = (proxy, method, args) -> {
            String n = method.getName();
            if (n.equals("merge")) {
                int first = (Integer) args[0];
                int second = (Integer) args[1];
                int result = (Integer) args[2];
                rows.add(new int[]{first, second, result});
                return Boolean.TRUE; // never abort
            }
            if (n.equals("toString")) return "IndexConsumerProxy";
            if (n.equals("hashCode")) return System.identityHashCode(proxy);
            if (n.equals("equals")) return proxy == args[0];
            throw new UnsupportedOperationException("stub IndexConsumer." + n);
        };
        Object consumer = Proxy.newProxyInstance(
            INDEX_CONSUMER.getClassLoader(), new Class[]{INDEX_CONSUMER}, h);
        M_FOR_MERGED_INDEXES.invoke(m, consumer);
        return rows;
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // FINITE/PHYSICAL sizes: positive list-segment counts. The vanilla call site
        // (Shapes.createIndexMerger) only builds a DiscreteCubeMerger when both lists
        // are CubePointRanges and cost*lcm(firstSize,secondSize) <= 256, so the common
        // domain is 1..16 (block grid resolutions) with lcm bounded by 256. We cover
        // that domain exhaustively for small sizes, plus larger sizes the ctor accepts.
        int[] sizes = {
            1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16,
            18, 20, 24, 32, 48, 64, 128, 256
        };

        for (int firstSize : sizes) {
            for (int secondSize : sizes) {
                DiscreteCubeMerger m = make(firstSize, secondSize);

                int gcd = (Integer) F_FIRSTDIV.get(m) == 0 ? 0 : firstSize / (Integer) F_FIRSTDIV.get(m);
                // Recover gcd directly from the int dividers (firstSize/gcd, secondSize/gcd):
                int firstDiv = (Integer) F_FIRSTDIV.get(m);
                int secondDiv = (Integer) F_SECONDDIV.get(m);
                gcd = firstSize / firstDiv; // exact: firstSize is divisible by firstDiv
                DoubleList result = (DoubleList) F_RESULT.get(m);
                int resultParts = result.size() - 1; // CubePointRange.size() = parts + 1
                int size = m.size();

                O.println("CTOR\t" + firstSize + "\t" + secondSize + "\t"
                    + gcd + "\t" + firstDiv + "\t" + secondDiv + "\t" + resultParts + "\t" + size);

                // getDouble over the whole result list (the CubePointRange grid).
                for (int index = 0; index < result.size(); index++) {
                    O.println("GET\t" + firstSize + "\t" + secondSize + "\t" + index
                        + "\t" + d(result.getDouble(index)));
                }

                // forMergedIndexes: full enumeration of (i/secondDiv, i/firstDiv, i).
                List<int[]> rows = capture(m);
                StringBuilder sb = new StringBuilder();
                sb.append("MERGE\t").append(firstSize).append('\t').append(secondSize)
                  .append('\t').append(rows.size());
                for (int[] r : rows) {
                    // token: result : first : second  (r = {first, second, result})
                    sb.append('\t').append(r[2]).append(':').append(r[0]).append(':').append(r[1]);
                }
                O.println(sb.toString());
            }
        }
    }
}
