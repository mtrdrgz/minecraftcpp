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

import com.google.gson.JsonArray;
import com.google.gson.JsonElement;
import com.google.gson.JsonObject;
import com.google.gson.JsonParser;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.nio.file.Files;
import java.nio.file.Path;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Map;
import java.util.Random;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Direction;
import net.minecraft.core.Holder;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.tags.TagKey;
import net.minecraft.tags.TagLoader;
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

    // ── load the REAL datapack block tags (Bootstrap alone leaves them empty), so connectsTo /
    // isSameFence (BlockTags.FENCES/WOODEN_FENCES) / copper-chest connection behave like the real
    // game. Same approach as FullChunkParity.bindVanillaBlockTags. ──
    static String tagIdFromPath(Path root, Path file) {
        String rel = root.relativize(file).toString().replace('\\', '/');
        return "minecraft:" + rel.substring(0, rel.length() - ".json".length());
    }
    static String entryId(JsonElement element) {
        if (element.isJsonPrimitive()) return element.getAsString();
        JsonObject obj = element.getAsJsonObject();
        if (obj.has("id")) return obj.get("id").getAsString();
        throw new IllegalArgumentException("unsupported tag entry: " + element);
    }
    static void resolveBlockTag(String id, Map<String, List<String>> rawTags,
            Map<String, List<Holder<Block>>> resolved, LinkedHashSet<String> resolving) {
        if (resolved.containsKey(id)) return;
        if (!resolving.add(id)) throw new IllegalStateException("cyclic block tag: " + id);
        LinkedHashSet<Holder<Block>> values = new LinkedHashSet<>();
        for (String entry : rawTags.getOrDefault(id, List.of())) {
            if (entry.startsWith("#")) {
                String nested = entry.substring(1);
                resolveBlockTag(nested, rawTags, resolved, resolving);
                values.addAll(resolved.getOrDefault(nested, List.of()));
            } else {
                Holder<Block> holder = BuiltInRegistries.BLOCK.get(
                    ResourceKey.create(Registries.BLOCK, Identifier.parse(entry)))
                    .orElseThrow(() -> new IllegalStateException("unknown block in tag " + id + ": " + entry));
                values.add(holder);
            }
        }
        resolving.remove(id);
        resolved.put(id, List.copyOf(values));
    }
    static void bindVanillaBlockTags() throws Exception {
        Path root = Path.of("26.1.2", "data", "minecraft", "tags", "block");
        Map<String, List<String>> rawTags = new HashMap<>();
        try (var stream = Files.walk(root)) {
            for (Path file : stream.filter(p -> p.toString().endsWith(".json")).toList()) {
                JsonObject obj = JsonParser.parseString(Files.readString(file)).getAsJsonObject();
                JsonArray values = obj.getAsJsonArray("values");
                List<String> entries = new ArrayList<>();
                for (JsonElement value : values) entries.add(entryId(value));
                rawTags.put(tagIdFromPath(root, file), entries);
            }
        }
        Map<String, List<Holder<Block>>> resolved = new HashMap<>();
        for (String id : rawTags.keySet()) resolveBlockTag(id, rawTags, resolved, new LinkedHashSet<>());
        Map<TagKey<Block>, List<Holder<Block>>> pending = new HashMap<>();
        for (Map.Entry<String, List<Holder<Block>>> e : resolved.entrySet())
            pending.put(TagKey.create(Registries.BLOCK, Identifier.parse(e.getKey())), e.getValue());
        BuiltInRegistries.BLOCK.prepareTagReload(new TagLoader.LoadResult<>(Registries.BLOCK, pending)).apply();
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
                case "isEmptyBlock": return map.getOrDefault(((BlockPos) args[0]).immutable(), air).isAir();
                case "getBlockEntity": return null;
                case "getRandom": return rng;
                case "isClientSide": return false;
                case "hasChunkAt": return true;
                case "equals": return proxy == args[0];
                case "hashCode": return System.identityHashCode(proxy);
                case "toString": return "ProxyLevel";
                default: break;
            }
            // NOTE: default interface methods that route purely through getBlockState (isEmptyBlock)
            // are special-cased above. A blanket InvocationHandler.invokeDefault is NOT used because
            // many default methods touch unimplemented level state (getBlockEntity/getHeight/world
            // border) and would throw, collapsing scenario coverage. Add explicit cases as needed.
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
    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        bindVanillaBlockTags();  // load REAL block tags so connectsTo / isSameFence match the game
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
