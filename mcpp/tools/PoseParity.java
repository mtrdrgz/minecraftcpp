import net.minecraft.world.entity.Pose;

// Ground-truth dumper for net.minecraft.world.entity.Pose (MC 26.1.2).
// Emits tab-separated rows consumed by PoseParityTest.cpp.
//
// TAGS:
//   CONST  <ordinal> <id> <name> <getSerializedName>
//
// ordinal / id are decimal ints; name (java.lang.Enum.name()) and
// getSerializedName() are raw strings. All values come from REAL net.minecraft.
public class PoseParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // Pose is a plain enum and needs no registry bootstrap, but bootstrap
        // defensively in case classloading pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — Pose does not require it
        }

        for (Pose v : Pose.values()) {
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.id()
                    + "\t" + v.name()
                    + "\t" + v.getSerializedName());
        }
    }
}
