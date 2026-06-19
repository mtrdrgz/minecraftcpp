// Ground truth for net.minecraft.world.level.material.PushReaction (Minecraft 26.1.2).
//
// PushReaction is a bare enum:
//
//   public enum PushReaction { NORMAL, DESTROY, BLOCK, IGNORE, PUSH_ONLY; }
//
// with no fields or methods. The only observable contract worth gating is the
// declaration order (== Enum.ordinal()) and each constant's Enum.name(). We drive
// the REAL enum via PushReaction.values() and emit, for every constant, its
// ordinal and name. We also emit the total count (values().length).
//
// Row TAGs (tab-separated):
//   COUNT  <n>                      n = PushReaction.values().length
//   CONST  <ordinal>  <name>        one row per constant, in declaration order
//
// The C++ test rebuilds the same enum from world/level/material/PushReaction.h
// and compares every ordinal (decimal, bit-for-bit) and name (raw string).

import net.minecraft.world.level.material.PushReaction;

public class PushReactionParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Bare enum has no static init dependencies, but bootstrap is cheap and
        // guards against any transitive class-load needing it.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // PushReaction itself needs no bootstrap; ignore if unavailable.
        }

        PushReaction[] values = PushReaction.values();
        O.println("COUNT\t" + values.length);
        for (PushReaction r : values) {
            O.println("CONST\t" + r.ordinal() + "\t" + r.name());
        }

        O.flush();
    }
}
