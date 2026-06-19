// Reference value generator for the C++ port of
//   net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces
//        .SmoothStoneSelector
// (a private static StructurePiece.BlockSelector) — verified against
// mcpp/src/world/level/levelgen/structure/structures/SmoothStoneSelector.h.
//
// Drives the REAL decompiled class from client.jar. SmoothStoneSelector is a
// private static nested class with an implicit no-arg constructor; we obtain it
// reflectively and invoke its public override
//     void next(RandomSource random, int worldX, int worldY, int worldZ, boolean isEdge)
// then read the selected BlockState via the inherited public getNext(). The block
// identity that crosses to C++ is the registry resource-location string
// (BuiltInRegistries.BLOCK.getKey(state.getBlock())), which the C++ test maps to
// the same StrongholdBlock enum kind.
//
// RNG: a REAL net.minecraft.world.level.levelgen.XoroshiroRandomSource(seed),
// continuously advancing across the per-seed call sequence (no reseed between
// calls), exactly as the real stronghold generateBox loop drives it. The C++ test
// seeds mc::levelgen::XoroshiroRandomSource(seed) identically.
//
//   javac -cp 26.1.2/client.jar -d <out> mcpp/tools/SmoothStoneSelectorParity.java
//   java  -cp <out>;26.1.2/client.jar SmoothStoneSelectorParity > smooth_stone_selector.tsv
//
// TSV rows (tab-separated), leading TAG SSS:
//   SSS  <seed>  <isEdgeMask>  <k0> <k1> ... <kN-1>
// where:
//   isEdgeMask  : the fixed 0/1 isEdge pattern applied across N calls (decimal,
//                 LSB = call 0), so the C++ side replays the identical isEdge args.
//   k0..kN-1    : the StrongholdBlock kind id of getNext() after each call.
//                 We prepend k=-1 conceptually via a dedicated INIT row below.
//
// Plus one INIT row per program (no seed): the base BlockSelector's initial
//   next == Blocks.AIR.defaultBlockState() before any next() call.
//
//   SSSINIT  <airKind>
//
// O is captured at class load so any bootstrap chatter on stdout stays out of TSV.
@SuppressWarnings({"deprecation", "unchecked"})
public class SmoothStoneSelectorParity {
    static final java.io.PrintStream O = System.out;

    // Fixed call pattern per seed: 24 calls, alternating/clustered isEdge so both
    // branches (RNG-consuming edge, RNG-free interior) are exercised and the float
    // thresholds 0.2/0.5/0.55 are crossed across many seeds. LSB = call 0.
    // Pattern bits: edge, edge, interior, edge, edge, edge, interior, interior, ...
    static final int N_CALLS = 24;
    static final long ISEDGE_MASK = 0b101101101101101101101101L & ((1L << N_CALLS) - 1);

    static int kindOf(String key) {
        switch (key) {
            case "minecraft:air":                   return 0; // AIR
            case "minecraft:cave_air":              return 1; // CAVE_AIR
            case "minecraft:cracked_stone_bricks":  return 2; // CRACKED_STONE_BRICKS
            case "minecraft:mossy_stone_bricks":    return 3; // MOSSY_STONE_BRICKS
            case "minecraft:infested_stone_bricks": return 4; // INFESTED_STONE_BRICKS
            case "minecraft:stone_bricks":          return 5; // STONE_BRICKS
            default: throw new IllegalStateException("unexpected block key: " + key);
        }
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // Resolve the private static nested class SmoothStoneSelector.
        Class<?> selClass = Class.forName(
            "net.minecraft.world.level.levelgen.structure.structures.StrongholdPieces$SmoothStoneSelector");
        java.lang.reflect.Constructor<?> ctor = selClass.getDeclaredConstructor();
        ctor.setAccessible(true);

        // The override is public on the class; resolve by name+arity to avoid
        // depending on the exact RandomSource interface spelling.
        java.lang.reflect.Method nextM = null;
        for (java.lang.reflect.Method m : selClass.getDeclaredMethods()) {
            if (m.getName().equals("next") && m.getParameterCount() == 5) { nextM = m; break; }
        }
        nextM.setAccessible(true);

        // getNext() is inherited from StructurePiece.BlockSelector (public).
        java.lang.reflect.Method getNextM = null;
        for (Class<?> c = selClass; c != null; c = c.getSuperclass()) {
            try { getNextM = c.getDeclaredMethod("getNext"); break; } catch (NoSuchMethodException ignored) {}
        }
        getNextM.setAccessible(true);

        // INIT row: a freshly constructed selector's next == Blocks.AIR.
        {
            Object freshSel = ctor.newInstance();
            Object airState = getNextM.invoke(freshSel);
            String airKey = net.minecraft.core.registries.BuiltInRegistries.BLOCK
                .getKey(((net.minecraft.world.level.block.state.BlockState) airState).getBlock()).toString();
            O.println("SSSINIT\t" + kindOf(airKey));
        }

        long[] seeds = {
            0L, 1L, 2L, 3L, 42L, 100L, 123456789L,
            -1L, -2L, -42L, -987654321L,
            2147483647L, -2147483648L,
            8675309L, 1234567890123456789L, -1234567890123456789L,
            9999999999L, -5555555555L, 314159265358979L, 271828182845904L
        };

        for (long seed : seeds) {
            net.minecraft.world.level.levelgen.XoroshiroRandomSource rng =
                new net.minecraft.world.level.levelgen.XoroshiroRandomSource(seed);
            Object sel = ctor.newInstance();
            StringBuilder sb = new StringBuilder();
            sb.append("SSS\t").append(seed).append('\t').append(ISEDGE_MASK);
            for (int i = 0; i < N_CALLS; i++) {
                boolean isEdge = ((ISEDGE_MASK >> i) & 1L) != 0L;
                // worldX/Y/Z are unused by this selector; feed varying values anyway.
                nextM.invoke(sel, rng, i, i * 2, i * 3, isEdge);
                Object state = getNextM.invoke(sel);
                String key = net.minecraft.core.registries.BuiltInRegistries.BLOCK
                    .getKey(((net.minecraft.world.level.block.state.BlockState) state).getBlock()).toString();
                sb.append('\t').append(kindOf(key));
            }
            O.println(sb.toString());
        }
    }
}
