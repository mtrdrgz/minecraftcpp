// Ground-truth generator for the per-constant ordinal + getSerializedName()
// tables of a second family of StringRepresentable enums under
//   net.minecraft.world.level.block.state.properties:
//     BambooLeaves, BellAttachType, DripstoneThickness, CreakingHeartState,
//     SculkSensorPhase, SideChainPart, StructureMode, TestBlockMode.
//
// For each enum it loads the REAL net.minecraft class, walks getEnumConstants()
// in declaration (== ordinal) order, and calls the REAL StringRepresentable
// getSerializedName() on every constant. No registry/world/Codec is touched;
// Bootstrap is invoked defensively but is not required for these pure enums.
//
// TSV columns (tab-separated, one row per probe):
//   COUNT   <SimpleName>                       -> <numConstants>
//   NAME    <SimpleName>  <ordinal>            -> <serializedName>
//
// Run:
//   tools/run_groundtruth.ps1 -Tool BlockStateEnums2Parity -Out mcpp/build/bsenums2.tsv

import java.lang.reflect.Method;

public class BlockStateEnums2Parity {
    static final java.io.PrintStream O = System.out;

    // The 8 gated enums, by simple class name (all in the .properties package).
    static final String[] ENUMS = {
        "BambooLeaves", "BellAttachType", "DripstoneThickness",
        "CreakingHeartState", "SculkSensorPhase", "SideChainPart",
        "StructureMode", "TestBlockMode",
    };

    static final String PKG = "net.minecraft.world.level.block.state.properties.";

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) { /* pure string enums; not required */ }

        // StringRepresentable.getSerializedName() — same signature on every enum.
        for (String simple : ENUMS) {
            Class<?> clazz = Class.forName(PKG + simple);
            Object[] constants = clazz.getEnumConstants();
            // getSerializedName() is declared on the StringRepresentable interface;
            // resolve it on the concrete enum class and force-access just in case.
            Method m = clazz.getMethod("getSerializedName");
            m.setAccessible(true);

            O.println("COUNT\t" + simple + "\t" + constants.length);
            for (int ord = 0; ord < constants.length; ord++) {
                Object ec = constants[ord];
                // Sanity: declaration index must equal ordinal().
                int realOrd = ((Enum<?>) ec).ordinal();
                String ser = (String) m.invoke(ec);
                O.println("NAME\t" + simple + "\t" + realOrd + "\t" + ser);
            }
        }
    }
}
