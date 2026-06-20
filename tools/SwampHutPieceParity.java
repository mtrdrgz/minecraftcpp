// Ground-truth generator for the block-placement body of the REAL decompiled
// 26.1.2 class:
//   net.minecraft.world.level.levelgen.structure.structures.SwampHutPiece
//     -> public void postProcess(WorldGenLevel, StructureManager, ChunkGenerator,
//                                RandomSource, BoundingBox chunkBB, ChunkPos, BlockPos)
//
// We drive the REAL SwampHutPiece.postProcess against a CAPTURING WorldGenLevel
// (java.lang.reflect.Proxy) that records every setBlock(pos, state) into a
// LinkedHashMap (last-write-wins) and:
//   * returns AIR for getBlockState on unknown positions (so skipAir in
//     generateBox never skips)
//   * returns a stable Y=64 for getHeightmapPos(MOTION_BLOCKING_NO_LEAVES, pos)
//     so updateAverageGroundHeight stays at the constructor's floor (64) —
//     this is the same value the C++ gate's StructureWorldAccess.getHeight
//     returns, decoupling parity from real terrain.
//   * returns a no-op LevelChunk (allocated via sun.misc.Unsafe.allocateInstance,
//     since LevelChunk is a class not an interface) for getChunk(pos) so
//     placeBlock's markPosForPostprocessing call on SHAPE_CHECK_BLOCKS (oak_fence)
//     doesn't NPE.
//   * returns null for getLevel() so the witch + cat spawns inside postProcess
//     short-circuit (EntityType.create(null, ...) returns null and the
//     `if (witch != null)` / `if (cat != null)` bodies are skipped). The
//     current C++ port does NOT spawn entities either, so the captured
//     block writes are the 1:1 comparable surface.
//
//   tools/run_groundtruth.sh SwampHutPieceParity build/swamp_hut_piece.tsv
//
// TSV rows (leading tag, all integers decimal, props string is comma-separated
// `k=v` in alphabetical key order):
//   BOX    <seed> <west> <north> <minX> <minY> <minZ> <maxX> <maxY> <maxZ> <dirOrd> <rotOrd> <mirOrd>
//   PLACE  <seed> <west> <north> <wx> <wy> <wz> <blockName> <props>
//   COUNT  <seed> <west> <north> <numPlaced>

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.LinkedHashMap;
import java.util.Map;
import java.util.TreeMap;

import net.minecraft.SharedConstants;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Mirror;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.block.state.properties.Property;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.structures.SwampHutPiece;

public class SwampHutPieceParity {
    static final java.io.PrintStream O = System.out;

    static String propsOf(BlockState s) {
        if (s == null) return "";
        TreeMap<String, String> tm = new TreeMap<>();
        s.getValues().forEach(v -> tm.put(v.property().getName(), v.valueName()));
        StringBuilder sb = new StringBuilder();
        boolean first = true;
        for (Map.Entry<String, String> e : tm.entrySet()) {
            if (!first) sb.append(',');
            sb.append(e.getKey()).append('=').append(e.getValue());
            first = false;
        }
        return sb.toString();
    }

    static String nameOf(BlockState s) {
        return net.minecraft.core.registries.BuiltInRegistries.BLOCK.getKey(s.getBlock()).toString()
                .replace("minecraft:", "");
    }

    static sun.misc.Unsafe unsafe() throws Exception {
        java.lang.reflect.Field f = sun.misc.Unsafe.class.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        return (sun.misc.Unsafe) f.get(null);
    }

    static WorldGenLevel makeCapturingLevel(final Map<BlockPos, BlockState> placed) {
        final BlockState air = net.minecraft.world.level.block.Blocks.AIR.defaultBlockState();

        // LevelChunk is a CLASS not an interface, so we can't Proxy it. Allocate
        // an instance without invoking its ctor via Unsafe. The only method
        // placeBlock invokes on this instance is markPosForPostprocessing(pos),
        // which LevelChunk doesn't override — the inherited ChunkAccess default
        // just logs a WARN (silenced by log4j root level OFF). All other fields
        // are null/0 and that's fine because we never call any other method.
        final Object noOpChunk;
        try {
            noOpChunk = unsafe().allocateInstance(net.minecraft.world.level.chunk.LevelChunk.class);
        } catch (Exception e) {
            throw new RuntimeException("Failed to allocate LevelChunk instance", e);
        }

        InvocationHandler h = (proxy, method, mArgs) -> {
            switch (method.getName()) {
                case "setBlock":
                    placed.put(((BlockPos) mArgs[0]).immutable(), (BlockState) mArgs[1]);
                    return Boolean.TRUE;
                case "getBlockState": {
                    BlockState st = placed.get(mArgs[0]);
                    return st != null ? st : air;
                }
                case "getFluidState": {
                    BlockState st = placed.get(mArgs[0]);
                    return st != null ? st.getFluidState()
                                      : net.minecraft.world.level.material.Fluids.EMPTY.defaultFluidState();
                }
                case "getHeightmapPos":
                    // Heightmap.Types.MOTION_BLOCKING_NO_LEAVES probe → Y=64.
                    return new BlockPos(((BlockPos) mArgs[1]).getX(), 64, ((BlockPos) mArgs[1]).getZ());
                case "getHeight":           return 384;
                case "getMinY":
                case "getMinBuildHeight":   return -64;
                case "getMaxY":
                case "getMaxBuildHeight":   return 319;
                case "getBlockEntity":      return null;
                case "getLevel":            return null;  // entity spawns short-circuit
                case "getChunk":            return noOpChunk;
                case "isClientSide":        return Boolean.FALSE;
                case "registryAccess":      return net.minecraft.core.registries.BuiltInRegistries.REGISTRY;
                case "scheduleTick":        return null;
                case "addFreshEntityWithPassengers": return Boolean.TRUE;
                case "getCurrentDifficultyAt":
                    return new net.minecraft.world.DifficultyInstance(
                        net.minecraft.world.Difficulty.PEACEFUL, 0L, 0L, 0.0F);
                case "toString":            return "CapturingWorldGenLevel";
                case "hashCode":            return System.identityHashCode(proxy);
                case "equals":              return proxy == mArgs[0];
                default:
                    Class<?> rt = method.getReturnType();
                    if (rt == boolean.class) return Boolean.FALSE;
                    if (rt == int.class)     return 0;
                    if (rt == long.class)    return 0L;
                    if (rt == float.class)   return 0f;
                    if (rt == double.class)  return 0d;
                    if (rt.isPrimitive())    return 0;
                    return null;
            }
        };
        return (WorldGenLevel) Proxy.newProxyInstance(
            WorldGenLevel.class.getClassLoader(), new Class[]{WorldGenLevel.class}, h);
    }

    static void runCase(long seed, int west, int north) {
        try {
            RandomSource r1 = RandomSource.create(seed);
            SwampHutPiece piece = new SwampHutPiece(r1, west, north);
            BoundingBox bb = piece.getBoundingBox();

            Direction dir = piece.getOrientation();
            Rotation rot = piece.getRotation();
            Mirror mir = piece.getMirror();
            O.println("BOX\t" + seed + "\t" + west + "\t" + north
                + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                + "\t" + (dir == null ? -1 : dir.ordinal())
                + "\t" + rot.ordinal()
                + "\t" + mir.ordinal());

            // chunkBB that covers the whole piece generously (no write filtering).
            BoundingBox chunkBB = new BoundingBox(
                bb.minX() - 1, bb.minY() - 1, bb.minZ() - 1,
                bb.maxX() + 1, bb.maxY() + 1, bb.maxZ() + 1);

            Map<BlockPos, BlockState> placed = new LinkedHashMap<>();
            WorldGenLevel level = makeCapturingLevel(placed);
            RandomSource r2 = RandomSource.create(seed);
            // r1 advanced one nextInt(4) in the ctor; mirror that on r2 so the
            // postProcess-side random state matches what real generation would do.
            r2.nextInt(4);

            try {
                piece.postProcess(level, null, null, r2, chunkBB,
                                  new ChunkPos(west >> 4, north >> 4),
                                  new BlockPos(west, 64, north));
            } catch (Throwable t) {
                // The witch + cat spawns at the tail of postProcess call
                // EntityType.create(level.getLevel(), ...) which NPEs because our
                // proxy's getLevel() returns null. The C++ port doesn't spawn
                // entities yet either, so the captured block writes (all of which
                // happen BEFORE the entity spawn) are the 1:1 comparable surface.
            }

            for (Map.Entry<BlockPos, BlockState> e : placed.entrySet()) {
                BlockPos p = e.getKey();
                O.println("PLACE\t" + seed + "\t" + west + "\t" + north
                    + "\t" + p.getX() + "\t" + p.getY() + "\t" + p.getZ()
                    + "\t" + nameOf(e.getValue())
                    + "\t" + propsOf(e.getValue()));
            }
            O.println("COUNT\t" + seed + "\t" + west + "\t" + north + "\t" + placed.size());
        } catch (Throwable t) {
            java.io.StringWriter sw = new java.io.StringWriter();
            t.printStackTrace(new java.io.PrintWriter(sw));
            O.println("RUNCASE_THREW\t" + seed + "\t" + west + "\t" + north + "\t"
                + sw.toString().replace('\n', '|').replace('\t', ' '));
        }
    }

    public static void main(String[] args) {
        // Unbuffer stdout so partial output is not lost on a fatal JVM error.
        System.setOut(new java.io.PrintStream(new java.io.FileOutputStream(java.io.FileDescriptor.out), true));

        try {
            // Silence log4j root — BoundingBox ctor logs an error on inverted boxes.
            org.apache.logging.log4j.core.config.Configurator.setRootLevel(
                org.apache.logging.log4j.Level.OFF);

            SharedConstants.tryDetectVersion();
            Bootstrap.bootStrap();

            // Seeds chosen so nextInt(4) lands on each of NORTH/EAST/SOUTH/WEST.
            long[] seeds = {0L, 1L, 2L, 3L, 7L, 42L, 123L, -1L, -42L};
            int[] coords = {0, 16, -16, 64, 256};

            for (long seed : seeds) {
                for (int west : coords) {
                    for (int north : coords) {
                        runCase(seed, west, north);
                    }
                }
            }
        } catch (Throwable t) {
            t.printStackTrace();
            System.exit(1);
        }
    }
}
