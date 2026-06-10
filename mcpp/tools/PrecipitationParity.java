// Ground-truth generator for the nested enum
// net.minecraft.world.level.biome.Biome.Precipitation (26.1.2).
//
// Drives the REAL enum over all three constants (NONE/RAIN/SNOW) and dumps
// tab-separated <TAG>\t<inputs...>\t<outputs...> rows to STDOUT. Per constant we
// emit ordinal(), name() and getSerializedName(). All values come from the real
// class; no inputs are fabricated.
//
//   tools/run_groundtruth.ps1 -Tool PrecipitationParity -Out mcpp/build/precipitation.tsv

import net.minecraft.world.level.biome.Biome;

public class PrecipitationParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Biome.Precipitation.values() — declaration order is the ground truth for
        // ordinal(). For each: ordinal(), name(), getSerializedName().
        for (Biome.Precipitation p : Biome.Precipitation.values()) {
            // PREC  ordinal  name  serializedName
            O.println("PREC\t" + p.ordinal() + "\t" + p.name() + "\t" + p.getSerializedName());
        }

        // Count of declared constants (values().length), as an extra invariant.
        // COUNT  values().length
        O.println("COUNT\t" + Biome.Precipitation.values().length);
    }
}
