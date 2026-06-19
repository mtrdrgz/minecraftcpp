// Ground-truth generator for net.minecraft.world.phys.shapes.BooleanOp — the 16
// named two-input boolean operators (FALSE, NOT_OR, ..., OR, TRUE) used by
// Shapes.join / joinIsNotEmpty. Each constant is a BooleanOp lambda field on the
// interface; we reflect the real field, then invoke its real apply(boolean,boolean)
// over the complete 2x2 truth table. The C++ port (world/phys/shapes/BooleanOp.h)
// must reproduce every truth table BIT-for-BIT.
//
//   tools/run_groundtruth.ps1 -Tool BooleanOpParity -Out mcpp/build/boolean_op.tsv
//
// Output rows (tab-separated):
//   APPLY <opName> <first:0|1> <second:0|1> <result:0|1>

import java.lang.reflect.Field;
import net.minecraft.world.phys.shapes.BooleanOp;

public class BooleanOpParity {
    static final java.io.PrintStream O = System.out;

    // Declaration order from BooleanOp.java (verbatim field names, lines 4-19).
    static final String[] OP_NAMES = {
        "FALSE", "NOT_OR", "ONLY_SECOND", "NOT_FIRST", "ONLY_FIRST", "NOT_SECOND",
        "NOT_SAME", "NOT_AND", "AND", "SAME", "SECOND", "CAUSES", "FIRST",
        "CAUSED_BY", "OR", "TRUE"
    };

    public static void main(String[] args) throws Exception {
        // Pure interface constants — no registries needed, but bootstrap is cheap
        // and harmless and keeps the harness uniform with the rest of the battery.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        boolean[] vals = { false, true };
        for (String name : OP_NAMES) {
            Field f = BooleanOp.class.getDeclaredField(name);
            f.setAccessible(true);
            BooleanOp op = (BooleanOp) f.get(null);
            for (boolean first : vals) {
                for (boolean second : vals) {
                    boolean r = op.apply(first, second);
                    O.println("APPLY\t" + name + "\t" + (first ? 1 : 0) + "\t"
                              + (second ? 1 : 0) + "\t" + (r ? 1 : 0));
                }
            }
        }
    }
}
