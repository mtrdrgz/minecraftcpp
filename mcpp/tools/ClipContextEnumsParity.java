// Ground-truth generator for the two pure nested enums of
//   net.minecraft.world.level.ClipContext:
//     ClipContext.Block  -> COLLIDER, OUTLINE, VISUAL, FALLDAMAGE_RESETTING
//     ClipContext.Fluid  -> NONE, SOURCE_ONLY, ANY, WATER
//
// Neither enum implements StringRepresentable, so the only pure accessors a
// plain Java enum exposes are ordinal() and name(). We load the REAL
// net.minecraft classes, walk getEnumConstants() in declaration (== ordinal)
// order, and emit ordinal()+name() for every constant. No registry/world/Codec
// is touched; Bootstrap is invoked defensively but is not required for these
// pure enums (the behavioural get()/canPick() shape lookups are intentionally
// NOT exercised — they need a live world/tag/collision-context).
//
// TSV columns (tab-separated, one row per probe):
//   COUNT   <SimpleName>                 -> <numConstants>
//   NAME    <SimpleName>  <ordinal>      -> <name()>
//
// Run:
//   tools/run_groundtruth.ps1 -Tool ClipContextEnumsParity -Out mcpp/build/clip_context_enums.tsv

public class ClipContextEnumsParity {
    static final java.io.PrintStream O = System.out;

    // Binary names of the two nested enums (outer$Inner).
    static final String[] ENUMS = {
        "net.minecraft.world.level.ClipContext$Block",
        "net.minecraft.world.level.ClipContext$Fluid",
    };
    // Short tag emitted in the TSV for each enum.
    static final String[] TAGS = { "Block", "Fluid" };

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) { /* pure string enums; not required */ }

        for (int i = 0; i < ENUMS.length; i++) {
            Class<?> clazz = Class.forName(ENUMS[i]);
            Object[] constants = clazz.getEnumConstants();
            String tag = TAGS[i];

            O.println("COUNT\t" + tag + "\t" + constants.length);
            for (int ord = 0; ord < constants.length; ord++) {
                Enum<?> ec = (Enum<?>) constants[ord];
                // Sanity: declaration index must equal ordinal().
                int realOrd = ec.ordinal();
                String nm = ec.name();
                O.println("NAME\t" + tag + "\t" + realOrd + "\t" + nm);
            }
        }
    }
}
