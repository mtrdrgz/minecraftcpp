// Ground-truth generator for the DATA surface of
//   net.minecraft.world.level.block.state.properties.WoodType  (Minecraft 26.1.2)
//
// Calls the REAL net.minecraft classes. WoodType is a record:
//   (String name, BlockSetType setType, SoundType soundType,
//    SoundType hangingSignSoundType, SoundEvent fenceGateClose,
//    SoundEvent fenceGateOpen)
//
// SoundEvents.register(...) goes through Registry.register(BuiltInRegistries.
// SOUND_EVENT, ...), so Bootstrap is REQUIRED before touching WoodType's static
// initializer (the fence-gate SoundEvents would otherwise throw
// "Not bootstrapped"). We bootstrap up front.
//
// What is emitted (all pure data; the SoundType/SoundEvent OBJECT graphs are not
// byte-comparable across Java<->C++, so we capture only their portable
// identity/location facts):
//
//   WT  <idx>  <name>  <setTypeName>  <canOpenByHand 0/1>  <canOpenByWindCharge 0/1>
//              <canButtonBeActivatedByArrows 0/1>  <pressurePlateSensitivity ordinal>
//              <soundCategory>  <fenceGateCloseLoc>  <fenceGateOpenLoc>
//        soundCategory is one of: DEFAULT_WOOD / CHERRY_WOOD / NETHER_WOOD /
//        BAMBOO_WOOD, derived from object identity of soundType() &
//        hangingSignSoundType() vs the named SoundType constants.
//
//   CNT  <count>                              -> number of WoodType.values()
//
//   CODEC  <inputName>  <present 0/1>  <resolvedName | ->
//        CODEC.parse via the underlying TYPES::get (name -> WoodType).
//
//   tools/run_groundtruth.ps1 -Tool WoodTypeParity -Out mcpp/build/wood_type.tsv

import java.util.List;
import java.util.stream.Collectors;

import net.minecraft.world.level.block.SoundType;
import net.minecraft.world.level.block.state.properties.BlockSetType;
import net.minecraft.world.level.block.state.properties.WoodType;

public class WoodTypeParity {
    static final java.io.PrintStream O = System.out;

    static String soundCategory(WoodType wt) {
        SoundType st = wt.soundType();
        SoundType hs = wt.hangingSignSoundType();
        if (st == SoundType.WOOD && hs == SoundType.HANGING_SIGN) return "DEFAULT_WOOD";
        if (st == SoundType.CHERRY_WOOD && hs == SoundType.CHERRY_WOOD_HANGING_SIGN) return "CHERRY_WOOD";
        if (st == SoundType.NETHER_WOOD && hs == SoundType.NETHER_WOOD_HANGING_SIGN) return "NETHER_WOOD";
        if (st == SoundType.BAMBOO_WOOD && hs == SoundType.BAMBOO_WOOD_HANGING_SIGN) return "BAMBOO_WOOD";
        return "UNKNOWN";
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // WoodType.values() — Stream<WoodType> in TYPES (insertion) order.
        List<WoodType> all = WoodType.values().collect(Collectors.toList());

        O.println("CNT\t" + all.size());

        int idx = 0;
        for (WoodType wt : all) {
            BlockSetType bst = wt.setType();
            String closeLoc = wt.fenceGateClose().location().toString();
            String openLoc = wt.fenceGateOpen().location().toString();
            O.println("WT\t" + idx + "\t"
                + wt.name() + "\t"
                + bst.name() + "\t"
                + (bst.canOpenByHand() ? 1 : 0) + "\t"
                + (bst.canOpenByWindCharge() ? 1 : 0) + "\t"
                + (bst.canButtonBeActivatedByArrows() ? 1 : 0) + "\t"
                + bst.pressurePlateSensitivity().ordinal() + "\t"
                + soundCategory(wt) + "\t"
                + closeLoc + "\t"
                + openLoc);
            idx++;
        }

        // CODEC resolution battery: name -> WoodType via the stringResolver
        // (TYPES::get). Cover every real name + some misses (case, plural,
        // namespaced, blank, BlockSetType-only names that are NOT WoodTypes).
        String[] probes = {
            "oak", "spruce", "birch", "acacia", "cherry", "jungle",
            "dark_oak", "pale_oak", "crimson", "warped", "mangrove", "bamboo",
            // misses:
            "OAK", "Oak", "oaks", "oak ", " oak", "", "iron", "copper",
            "gold", "stone", "polished_blackstone", "wood", "minecraft:oak",
            "unknown",
        };
        for (String p : probes) {
            // Codec.stringResolver expects a string-typed value; feed a JSON
            // string primitive via JsonOps (the ops the rest of the harness uses).
            WoodType r = WoodType.CODEC
                .parse(com.mojang.serialization.JsonOps.INSTANCE, new com.google.gson.JsonPrimitive(p))
                .result().orElse(null);
            O.println("CODEC\t" + p + "\t" + (r != null ? 1 : 0) + "\t"
                + (r != null ? r.name() : "-"));
        }
    }
}
