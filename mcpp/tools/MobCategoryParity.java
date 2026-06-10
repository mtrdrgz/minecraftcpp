import net.minecraft.world.entity.MobCategory;

// Ground-truth dumper for net.minecraft.world.entity.MobCategory (MC 26.1.2).
// Emits tab-separated rows consumed by MobCategoryParityTest.cpp.
//
// TAGS:
//   CONST  <ordinal> <getName> <getSerializedName> <getMaxInstancesPerChunk>
//          <isFriendly> <isPersistent> <getDespawnDistance> <getNoDespawnDistance>
//
// getName / getSerializedName are raw strings; the four ints are decimal; the two
// booleans are decimal 0/1. All values come from REAL net.minecraft public getters.
public class MobCategoryParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        // MobCategory is a plain enum and needs no registry bootstrap, but bootstrap
        // defensively in case classloading pulls in registry-touching dependencies.
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // ignore — MobCategory does not require it
        }

        for (MobCategory v : MobCategory.values()) {
            O.println("CONST"
                    + "\t" + v.ordinal()
                    + "\t" + v.getName()
                    + "\t" + v.getSerializedName()
                    + "\t" + v.getMaxInstancesPerChunk()
                    + "\t" + (v.isFriendly() ? 1 : 0)
                    + "\t" + (v.isPersistent() ? 1 : 0)
                    + "\t" + v.getDespawnDistance()
                    + "\t" + v.getNoDespawnDistance());
        }
    }
}
