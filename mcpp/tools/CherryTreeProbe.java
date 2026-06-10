// Debug probe (not a parity gate): place ONE cherry tree (cherry_bees_005 config)
// at (0,100,0) on a flat super-flat-like grid (grass at y=99, dirt below, air
// above), seeded XoroshiroRandomSource(<seed>), and print every setBlock the
// tree makes in order plus the final RNG draw count. Diffed against the C++
// cherry_tree_probe to isolate cherry trunk/foliage RNG divergences.
//   java CherryTreeProbe <seed>
import java.util.List;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.WorldGenLevel;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.levelgen.XoroshiroRandomSource;
import net.minecraft.world.level.levelgen.feature.ConfiguredFeature;
import java.lang.reflect.InvocationHandler;
import java.lang.reflect.Method;
import java.lang.reflect.Proxy;
import java.util.HashMap;
import java.util.Map;

public class CherryTreeProbe {
    static final java.io.PrintStream ERR = System.err;
    static Map<Long, BlockState> world = new HashMap<>();

    static long key(BlockPos p) { return (((long) p.getX() & 0xFFFFF) << 40) | (((long) p.getY() & 0xFFFFF) << 20) | ((long) p.getZ() & 0xFFFFF); }

    static BlockState stateAt(BlockPos p) {
        BlockState s = world.get(key(p));
        if (s != null) return s;
        if (p.getY() == 99) return Blocks.GRASS_BLOCK.defaultBlockState();
        if (p.getY() < 99) return Blocks.DIRT.defaultBlockState();
        return Blocks.AIR.defaultBlockState();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();
        // block tags needed by validTreePos etc.
        java.lang.reflect.Method bind = Class.forName("FullChunkDecorateParity").getDeclaredMethod("bindVanillaBlockTags");
        bind.setAccessible(true);
        bind.invoke(null);

        long seed = args.length > 0 ? Long.parseLong(args[0]) : 1L;
        HolderLookup.Provider provider = VanillaRegistries.createLookup();
        Holder<ConfiguredFeature<?, ?>> cherry = provider.lookupOrThrow(Registries.CONFIGURED_FEATURE)
            .getOrThrow(ResourceKey.create(Registries.CONFIGURED_FEATURE, Identifier.parse("minecraft:cherry_bees_005")));

        InvocationHandler h = (proxy, m, a) -> {
            switch (m.getName()) {
                case "getBlockState": return stateAt((BlockPos) a[0]);
                case "getFluidState": return stateAt((BlockPos) a[0]).getFluidState();
                case "isStateAtPosition": { @SuppressWarnings("unchecked") var pr = (java.util.function.Predicate<BlockState>) a[1]; return pr.test(stateAt((BlockPos) a[0])); }
                case "isFluidAtPosition": { @SuppressWarnings("unchecked") var pr = (java.util.function.Predicate<net.minecraft.world.level.material.FluidState>) a[1]; return pr.test(stateAt((BlockPos) a[0]).getFluidState()); }
                case "setBlock": {
                    BlockPos p = (BlockPos) a[0]; BlockState st = (BlockState) a[1];
                    world.put(key(p), st);
                    System.out.println("PUT\t" + p.getX() + "\t" + p.getY() + "\t" + p.getZ() + "\t"
                        + st.getBlock().builtInRegistryHolder().key().identifier());
                    return true;
                }
                case "ensureCanWrite": return true;
                case "getMinY": return -64;
                case "getMaxY": return 319;
                case "getRandom": return RandomSource.create(0);
                case "isOutsideBuildHeight": { int y = a[0] instanceof BlockPos bp ? bp.getY() : (Integer) a[0]; return y < -64 || y >= 320; }
                case "getHeight": if (a != null && a.length == 3) return 100; return 384;
                case "isEmptyBlock": return stateAt((BlockPos) a[0]).isAir();
                case "scheduleTick": case "markPosForPostprocessing": case "blockUpdated": return null;
                case "toString": return "probe";
                case "hashCode": return 1;
                case "equals": return proxy == a[0];
                default:
                    if (m.isDefault()) return InvocationHandler.invokeDefault(proxy, m, a);
                    throw new UnsupportedOperationException("probe." + m.getName() + "/" + (a == null ? 0 : a.length));
            }
        };
        WorldGenLevel level = (WorldGenLevel) Proxy.newProxyInstance(WorldGenLevel.class.getClassLoader(),
            new Class<?>[] { WorldGenLevel.class }, h);

        XoroshiroRandomSource inner = new XoroshiroRandomSource(seed);
        RandomSource random = new RandomSource() {   // draw-tracing delegate
            @Override public RandomSource fork() { return inner.fork(); }
            @Override public net.minecraft.world.level.levelgen.PositionalRandomFactory forkPositional() { return inner.forkPositional(); }
            @Override public void setSeed(long s) { inner.setSeed(s); }
            @Override public int nextInt() { int v = inner.nextInt(); System.out.println("RNG\tnextInt\t" + v); return v; }
            @Override public int nextInt(int bound) { int v = inner.nextInt(bound); System.out.println("RNG\tnextInt" + bound + "\t" + v); return v; }
            @Override public long nextLong() { long v = inner.nextLong(); System.out.println("RNG\tnextLong\t" + v); return v; }
            @Override public boolean nextBoolean() { boolean v = inner.nextBoolean(); System.out.println("RNG\tnextBoolean\t" + v); return v; }
            @Override public float nextFloat() { float v = inner.nextFloat(); System.out.println("RNG\tnextFloat\t" + v); return v; }
            @Override public double nextDouble() { double v = inner.nextDouble(); System.out.println("RNG\tnextDouble\t" + v); return v; }
            @Override public double nextGaussian() { double v = inner.nextGaussian(); System.out.println("RNG\tnextGaussian\t" + v); return v; }
        };
        boolean ok = cherry.value().place(level, null, random, new BlockPos(0, 100, 0));
        ERR.println("ok=" + ok);
    }
}
