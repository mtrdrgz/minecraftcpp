// Ground-truth generator for net.minecraft.world.level.LightLayer (Minecraft 26.1.2).
//
// LightLayer is a bare enum { SKY, BLOCK; } — no fields, no constructor, no
// StringRepresentable, no accessors. This tool calls the REAL enum and emits, as
// tab-separated rows to STDOUT (the runner captures stdout into light_layer.tsv):
//
//   CNT   <count>                                 LightLayer.values().length
//   ORD   <ordinal>  <name>                       per constant, in values() order
//   VAL   <ordinal>  <name>                        per LightLayer.valueOf(name) round-trip
//
// LightLayer is pure (no registry/world coupling); the bootstrap guard is purely
// defensive in case enum class init ever touches anything.
//
//   tools/run_groundtruth.ps1 -Tool LightLayerParity -Out mcpp/build/light_layer.tsv

import net.minecraft.world.level.LightLayer;

public class LightLayerParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        try {
            net.minecraft.SharedConstants.tryDetectVersion();
            net.minecraft.server.Bootstrap.bootStrap();
        } catch (Throwable t) {
            // LightLayer does not need the bootstrap; ignore.
        }

        LightLayer[] VALUES = LightLayer.values();   // SKY, BLOCK (ordinals 0,1)

        // (1) count — locks the number of constants.
        O.println("CNT\t" + VALUES.length);

        // (2) ordinal + name for every constant, in values() (= declaration) order.
        for (LightLayer l : VALUES) {
            O.println("ORD\t" + l.ordinal() + "\t" + l.name());
        }

        // (3) valueOf(name) round-trips — the name->constant mapping must agree.
        for (LightLayer l : VALUES) {
            LightLayer r = LightLayer.valueOf(l.name());
            O.println("VAL\t" + r.ordinal() + "\t" + r.name());
        }
    }
}
