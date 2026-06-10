import net.minecraft.world.Difficulty;

// Ground-truth dumper for net.minecraft.world.Difficulty (MC 26.1.2).
// Emits tab-separated rows consumed by DifficultyParityTest.cpp.
//
// TAGS:
//   CONST <ordinal> <getId> <name> <getSerializedName>
//          one row per enum constant (Difficulty.values()).
//   BYID  <id> <byId(id).getId> <byId(id).getSerializedName>
//          byId(int) over a finite/physical sweep of ids, including the
//          out-of-range values that exercise the ByIdMap WRAP (floorMod) path.
//
// getId / id are decimal; name / getSerializedName are raw strings. All values
// come from REAL net.minecraft.world.Difficulty public methods.
public class DifficultyParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings("deprecation")  // Difficulty.byId/getKey is @Deprecated (strict runner treats the javac warning as fatal)
    public static void main(String[] args) throws Exception {
        // Difficulty is a plain enum and needs no registry bootstrap, but bootstrap
        // defensively in case classloading pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — Difficulty does not require it
        }

        // Per-constant accessors.
        for (Difficulty v : Difficulty.values()) {
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.getId()
                    + "\t" + v.name()
                    + "\t" + v.getSerializedName());
        }

        // byId(int) sweep. Covers the in-range ids 0..3 and a band of out-of-range
        // ids on both sides so the WRAP (Math.floorMod) behaviour is pinned. All
        // inputs are finite physical ints.
        for (int id = -12; id <= 12; id++) {
            Difficulty d = Difficulty.byId(id);
            O.println("BYID"
                    + "\t" + id
                    + "\t" + d.getId()
                    + "\t" + d.getSerializedName());
        }
    }
}
