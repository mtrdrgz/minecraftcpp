// Reference generator for the C++ HeightmapPlacement / HeightRangePlacement
// ports. Runs the real decompiled modifiers against a stub world.
//
//   javac -cp 26.1.2/client.jar:26.1.2/libs/* -d <out> mcpp/tools/WorldPlacementParity.java
//   java  -cp <out>:26.1.2/client.jar:26.1.2/libs/* WorldPlacementParity out.tsv
//
// WorldGenLevel is an interface, so it is implemented with a dynamic Proxy whose
// getHeight(type,x,z) is a deterministic stub. PlacementContext's only ctor needs
// a ChunkGenerator, so it is built with Unsafe (set minY/height + level fields).
import net.minecraft.core.BlockPos;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.VerticalAnchor;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.heightproviders.ConstantHeight;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
import net.minecraft.world.level.levelgen.heightproviders.TrapezoidHeight;
import net.minecraft.world.level.levelgen.heightproviders.UniformHeight;
import net.minecraft.world.level.levelgen.placement.HeightRangePlacement;
import net.minecraft.world.level.levelgen.placement.HeightmapPlacement;
import net.minecraft.world.level.levelgen.placement.NoiseBasedCountPlacement;
import net.minecraft.world.level.levelgen.placement.NoiseThresholdCountPlacement;
import net.minecraft.world.level.levelgen.placement.PlacementContext;
import net.minecraft.world.level.levelgen.placement.PlacementModifier;

import java.io.PrintWriter;
import java.lang.reflect.Field;
import java.lang.reflect.Proxy;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.stream.Collectors;

public class WorldPlacementParity {
    static final int MIN_Y = -64;
    static final int HEIGHT = 384;

    // Deterministic stub heightmap: range -64..63, so height==MIN_Y (empty
    // branch) is exercised when the low 7 bits are zero.
    static int stubHeight(int typeIndex, int x, int z) {
        return ((x * 31 + z * 17 + typeIndex * 7) & 127) - 64;
    }

    static WorldGenLevel proxyLevel() {
        return (WorldGenLevel) Proxy.newProxyInstance(
            WorldGenLevel.class.getClassLoader(),
            new Class[]{ WorldGenLevel.class },
            (proxy, method, args) -> {
                String n = method.getName();
                if (n.equals("getHeight") && args != null && args.length == 3) {
                    int idx = ((Heightmap.Types) args[0]).ordinal();
                    return stubHeight(idx, (Integer) args[1], (Integer) args[2]);
                }
                if (n.equals("getMinY")) return MIN_Y;
                if (n.equals("toString")) return "StubLevel";
                if (n.equals("hashCode")) return System.identityHashCode(proxy);
                if (n.equals("equals")) return proxy == args[0];
                throw new UnsupportedOperationException("stub WorldGenLevel." + n);
            });
    }

    static PlacementContext makeContext(WorldGenLevel level) throws Exception {
        Field theUnsafe = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        sun.misc.Unsafe unsafe = (sun.misc.Unsafe) theUnsafe.get(null);
        PlacementContext ctx = (PlacementContext) unsafe.allocateInstance(PlacementContext.class);
        Field fMinY = WorldGenerationContext.class.getDeclaredField("minY");
        Field fHeight = WorldGenerationContext.class.getDeclaredField("height");
        unsafe.putInt(ctx, unsafe.objectFieldOffset(fMinY), MIN_Y);
        unsafe.putInt(ctx, unsafe.objectFieldOffset(fHeight), HEIGHT);
        Field fLevel = PlacementContext.class.getDeclaredField("level");
        Field fGen = PlacementContext.class.getDeclaredField("generator");
        Field fTop = PlacementContext.class.getDeclaredField("topFeature");
        unsafe.putObject(ctx, unsafe.objectFieldOffset(fLevel), level);
        unsafe.putObject(ctx, unsafe.objectFieldOffset(fGen), null);
        unsafe.putObject(ctx, unsafe.objectFieldOffset(fTop), Optional.empty());
        return ctx;
    }

    static String posList(PlacementModifier m, PlacementContext ctx, RandomSource r, BlockPos origin) {
        List<BlockPos> positions = m.getPositions(ctx, r, origin).collect(Collectors.toList());
        String s = positions.stream().map(p -> p.getX() + ":" + p.getY() + ":" + p.getZ()).collect(Collectors.joining(","));
        return positions.size() + "\t" + (s.isEmpty() ? "-" : s);
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        PlacementContext ctx = makeContext(proxyLevel());
        int[][] origins = { {0, 0}, {16, 16}, {-5, 7}, {100, -50}, {1000, 2000}, {-13, -29}, {3, 0} };

        Map<String, HeightProvider> heights = new LinkedHashMap<>();
        heights.put("const50", ConstantHeight.of(VerticalAnchor.absolute(50)));
        heights.put("uni_full", UniformHeight.of(VerticalAnchor.aboveBottom(0), VerticalAnchor.belowTop(0)));
        heights.put("trap", TrapezoidHeight.of(VerticalAnchor.absolute(0), VerticalAnchor.absolute(100), 20));

        try (PrintWriter out = new PrintWriter(args.length > 0 ? args[0] : "world_placement_cases.tsv")) {
            for (Heightmap.Types type : Heightmap.Types.values()) {
                HeightmapPlacement m = HeightmapPlacement.onHeightmap(type);
                for (int[] o : origins) {
                    RandomSource r = new LegacyRandomSource(0L);
                    out.println("HMAP\t" + type.ordinal() + "\t" + o[0] + "\t64\t" + o[1] + "\t"
                            + posList(m, ctx, r, new BlockPos(o[0], 64, o[1])));
                }
            }
            long[] seeds = { 0L, 1L, 42L, 123456789L };
            for (Map.Entry<String, HeightProvider> e : heights.entrySet()) {
                HeightRangePlacement m = HeightRangePlacement.of(e.getValue());
                for (long seed : seeds) {
                    for (int[] o : origins) {
                        RandomSource r = new LegacyRandomSource(seed);
                        out.println("HRANGE\t" + e.getKey() + "\t" + seed + "\t" + o[0] + "\t64\t" + o[1] + "\t"
                                + posList(m, ctx, r, new BlockPos(o[0], 64, o[1])));
                    }
                }
            }

            // Noise-count modifiers (sample Biome.BIOME_INFO_NOISE; no RNG). Emit the
            // resulting count over a grid of XZ so the noise drives different counts.
            Map<String, PlacementModifier> noiseCounts = new LinkedHashMap<>();
            noiseCounts.put("ntc_flower", NoiseThresholdCountPlacement.of(-0.8, 15, 4)); // flower_plains
            noiseCounts.put("nbc", NoiseBasedCountPlacement.of(160, 80.0, 0.3));
            int[][] grid = { {0, 0}, {200, 200}, {-200, 400}, {123, -456}, {1000, 2000}, {37, 37}, {-13, -29}, {800, -800} };
            RandomSource unused = new LegacyRandomSource(0L);
            for (Map.Entry<String, PlacementModifier> e : noiseCounts.entrySet()) {
                for (int[] o : grid) {
                    long count = e.getValue().getPositions(ctx, unused, new BlockPos(o[0], 64, o[1])).count();
                    out.println("NCOUNT\t" + e.getKey() + "\t" + o[0] + "\t" + o[1] + "\t" + count);
                }
            }
        }
    }
}
