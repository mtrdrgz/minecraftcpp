// Ground-truth generator for net.minecraft.util.CommonColors (26.1.2) — the named
// packed-ARGB int color constants. Pure data; no Bootstrap needed. Reads every
// `public static final int` field straight off the REAL class via reflection (so the
// truth is the class itself, not a re-typing) and emits one row per constant:
//
//   COLOR\t<NAME>\t<int value, decimal>
//
//   tools/run_groundtruth.ps1 -Tool CommonColorsParity -Out mcpp/build/common_colors.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Modifier;
import net.minecraft.util.CommonColors;

public class CommonColorsParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        for (Field fd : CommonColors.class.getDeclaredFields()) {
            int m = fd.getModifiers();
            if (!Modifier.isStatic(m) || !Modifier.isFinal(m)) continue;
            if (fd.getType() != int.class) continue;
            fd.setAccessible(true);
            int v = fd.getInt(null);
            O.println("COLOR\t" + fd.getName() + "\t" + v);
        }
    }
}
