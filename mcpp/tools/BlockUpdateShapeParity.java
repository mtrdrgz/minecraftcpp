// Ground-truth oracle for net.minecraft...Block.updateFromNeighbourShapes — the per-block
// updateShape connection recompute that runs at the tail of StructureTemplate.placeInWorld
// (knownShape=false) and on every neighbour update. RULE #0: we drive the REAL
// updateFromNeighbourShapes over real states + controlled 3x3x3 neighbourhoods; never reimplement.
//
// For each block whose class overrides updateShape, we sample its states and, for each, build K
// deterministic random 3x3x3 neighbourhoods from a fixed probe pool, place them in a Proxy
// LevelAccessor (getBlockState -> the mapped state, air elsewhere), run
// Block.updateFromNeighbourShapes(centerState, proxyLevel, ZERO) and emit the resulting state id.
// The C++ port replays the SAME neighbourhood (it reads the emitted neighbour ids) and compares.
//
// Rows:
//   OFFSETS <n> [<dx> <dy> <dz>]...                          (fixed neighbourhood cell order)
//   U <centerStateId> <neighbourStateId * n> <outStateId>   (one scenario)
//   FAM <blockKey> <updateShapeDeclaringClass>
//   TOTAL <scenarioCount>

import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Random;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.LevelAccessor;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockBehaviour;
import net.minecraft.world.level.block.state.BlockState;

public final class BlockUpdateShapeParity {

    // fixed neighbourhood: the 26 non-centre cells of a 3x3x3 cube, deterministic order.
    private static int[][] offsets() {
        List<int[]> o = new ArrayList<>();
        for (int dx = -1; dx <= 1; dx++)
            for (int dy = -1; dy <= 1; dy++)
                for (int dz = -1; dz <= 1; dz++)
                    if (dx != 0 || dy != 0 || dz != 0) o.add(new int[]{dx, dy, dz});
        return o.toArray(new int[0][]);
    }

    private static String declaringClass(Class<?> start, String name, Class<?>... params) {
        for (Class<?> c = start; c != null; c = c.getSuperclass()) {
            try { if (c.getDeclaredMethod(name, params) != null) return c.getSimpleName(); }
            catch (NoSuchMethodException ignored) {}
        }
        return "?";
    }

    // Proxy LevelAccessor: getBlockState -> map (air default); getFluidState -> that state's fluid;
    // getBlockEntity -> null; getRandom -> fixed; everything else -> type default / no-op.
    private static LevelAccessor proxyLevel(final Map<BlockPos, BlockState> map, final RandomSource rng) {
        final BlockState air = Blocks.AIR.defaultBlockState();
        InvocationHandler h = (proxy, method, args) -> {
            switch (method.getName()) {
                case "getBlockState": return map.getOrDefault(((BlockPos) args[0]).immutable(), air);
                case "getFluidState": return map.getOrDefault(((BlockPos) args[0]).immutable(), air).getFluidState();
                case "getBlockEntity": return null;
                case "getRandom": return rng;
                case "isClientSide": return false;
                case "hasChunkAt": return true;
                case "equals": return proxy == args[0];
                case "hashCode": return System.identityHashCode(proxy);
                case "toString": return "ProxyLevel";
                default: break;
            }
            Class<?> rt = method.getReturnType();
            if (rt == boolean.class) return false;
            if (rt == int.class) return 0;
            if (rt == long.class) return 0L;
            if (rt == float.class) return 0.0F;
            if (rt == double.class) return 0.0;
            if (rt == void.class) return null;
            return null;
        };
        return (LevelAccessor) Proxy.newProxyInstance(
            LevelAccessor.class.getClassLoader(), new Class<?>[]{LevelAccessor.class}, h);
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(String[] args) {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        final StringBuilder out = new StringBuilder(1 << 24);

        final Class<?>[] usParams = new Class<?>[]{
            BlockState.class, net.minecraft.world.level.LevelReader.class,
            net.minecraft.world.level.ScheduledTickAccess.class, BlockPos.class, Direction.class,
            BlockPos.class, BlockState.class, RandomSource.class};

        final int[][] offs = offsets();
        out.append("OFFSETS\t").append(offs.length);
        for (int[] o : offs) out.append('\t').append(o[0]).append('\t').append(o[1]).append('\t').append(o[2]);
        out.append('\n');

        // probe pool: states that exercise connection / shape recompute logic.
        List<BlockState> probes = new ArrayList<>();
        probes.add(Blocks.AIR.defaultBlockState());
        probes.add(Blocks.STONE.defaultBlockState());
        probes.add(Blocks.OAK_FENCE.defaultBlockState());
        probes.add(Blocks.NETHER_BRICK_FENCE.defaultBlockState());
        probes.add(Blocks.COBBLESTONE_WALL.defaultBlockState());
        probes.add(Blocks.GLASS_PANE.defaultBlockState());
        probes.add(Blocks.IRON_BARS.defaultBlockState());
        probes.add(Blocks.OAK_STAIRS.defaultBlockState());
        probes.add(Blocks.OAK_STAIRS.defaultBlockState().setValue(net.minecraft.world.level.block.StairBlock.FACING, Direction.SOUTH));
        probes.add(Blocks.REDSTONE_WIRE.defaultBlockState());
        probes.add(Blocks.OAK_SLAB.defaultBlockState());
        probes.add(Blocks.CHEST.defaultBlockState());
        probes.add(Blocks.GLASS.defaultBlockState());
        probes.add(Blocks.WATER.defaultBlockState());
        final RandomSource rng = RandomSource.create(0L);

        long scenarios = 0;
        for (Block b : BuiltInRegistries.BLOCK) {
            String us = declaringClass(b.getClass(), "updateShape", usParams);
            out.append("FAM\t").append(BuiltInRegistries.BLOCK.getKey(b)).append('\t').append(us).append('\n');
            if (us.equals("BlockBehaviour")) continue;  // no override -> updateShape returns state unchanged

            List<BlockState> states = b.getStateDefinition().getPossibleStates();
            int total = states.size();
            int cap = 48;
            int stride = Math.max(1, total / cap);
            // per-block probe pool = global pool + this block's own default state.
            List<BlockState> pool = new ArrayList<>(probes);
            pool.add(b.defaultBlockState());

            for (int si = 0; si < total; si += stride) {
                BlockState centre = states.get(si);
                int centreId = Block.BLOCK_STATE_REGISTRY.getId(centre);
                for (int k = 0; k < 4; k++) {
                    // deterministic neighbourhood from (blockId, si, k).
                    Random r = new Random((((long) BuiltInRegistries.BLOCK.getId(b)) << 20) ^ ((long) si << 4) ^ k);
                    Map<BlockPos, BlockState> map = new HashMap<>();
                    int[] nbrIds = new int[offs.length];
                    for (int i = 0; i < offs.length; i++) {
                        BlockState ns = pool.get(r.nextInt(pool.size()));
                        map.put(new BlockPos(offs[i][0], offs[i][1], offs[i][2]), ns);
                        nbrIds[i] = Block.BLOCK_STATE_REGISTRY.getId(ns);
                    }
                    LevelAccessor level = proxyLevel(map, rng);
                    int outId;
                    try {
                        BlockState res = Block.updateFromNeighbourShapes(centre, level, BlockPos.ZERO);
                        outId = Block.BLOCK_STATE_REGISTRY.getId(res);
                    } catch (Throwable t) {
                        outId = -1;  // reads something the proxy can't serve: skip in C++
                    }
                    out.append("U\t").append(centreId);
                    for (int id : nbrIds) out.append('\t').append(id);
                    out.append('\t').append(outId).append('\n');
                    scenarios++;
                }
            }
        }
        out.append("TOTAL\t").append(scenarios).append('\n');
        System.out.print(out);
    }
}
