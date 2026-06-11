// Ground-truth generator for the C++ 1:1 port of the REAL 26.1.2 jigsaw template
// pool + element type hierarchy:
//   net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool
//   + StructurePoolElement / SinglePoolElement / LegacySinglePoolElement /
//     ListPoolElement / EmptyPoolElement.
//
// RULE #0: we drive the REAL classes; we NEVER reimplement weight-expansion /
// Util.shuffle / getMaxSize / element.getBoundingBox Java-side. We load the four
// pillager_outpost pools from the vanilla registry (VanillaRegistries.createLookup),
// read each pool's EXPANDED `templates` list (post-weight-expansion, in order) and
// `fallback` reflectively, and call the REAL public methods:
//   getMaxSize(StructureTemplateManager)                -> a jar-backed manager
//   getShuffledTemplates(RandomSource)                  -> WorldgenRandom(LegacyRandomSource(seed))
//   element.getBoundingBox(manager, BlockPos, Rotation) -> the REAL element box
//   element.getProjection() / getGroundLevelDelta() / getSize(manager, rotation)
//
// The headless StructureTemplateManager is a JigsawPlacementParity-style subclass
// allocated WITHOUT a constructor (Unsafe.allocateInstance, accessed reflectively —
// NEVER `import sun.misc.Unsafe`) whose getOrCreate/get load
// data/minecraft/structure/<id.path>.nbt from 26.1.2/client.jar. We also base64-ship
// each referenced .nbt so the C++ test can recover the element template SIZES.
//
//   tools/run_groundtruth.ps1 -Tool StructureTemplatePoolParity -Out mcpp/build/structure_template_pool.tsv
//
// TSV rows (leading TAG; all ints decimal; strings ASCII Identifiers verbatim):
//   NBT      <id>            <base64nbt> <sizeX> <sizeY> <sizeZ>
//   POOL     <poolKey>       <fallbackId> <size> <maxSize>
//   TEMPLATE <poolKey>       <index> <elementLocation> <projectionName> <groundLevelDelta>
//   SHUFFLE  <poolKey>       <seed>  <orderIndex> <elementLocation> <projectionName>
//   BOX      <poolKey>       <index> <rotOrd> <posX> <posY> <posZ>
//            <minX> <minY> <minZ> <maxX> <maxY> <maxZ> <ySpan>

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.ArrayList;
import java.util.Base64;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Optional;
import java.util.Set;
import java.util.zip.ZipFile;

import net.minecraft.core.BlockPos;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.Vec3i;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.NbtAccounter;
import net.minecraft.nbt.NbtIo;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Rotation;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.pools.ListPoolElement;
import net.minecraft.world.level.levelgen.structure.pools.SinglePoolElement;
import net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;

public final class StructureTemplatePoolParity {

    // The four pillager_outpost pools (PillagerOutpostPools.bootstrap registers these).
    private static final String[] POOL_KEYS = {
        "pillager_outpost/base_plates",
        "pillager_outpost/towers",
        "pillager_outpost/feature_plates",
        "pillager_outpost/features",
    };

    // Fixed (pos, rotation) battery for element boxes — mirrored in the C++ test.
    private record BoxCase(int x, int y, int z, Rotation rotation) {}

    private static List<BoxCase> boxCases() {
        List<BoxCase> list = new ArrayList<>();
        int[][] offs = { {0, 0, 0}, {3, -7, 11}, {-13, 64, -5}, {100, 0, -100}, {-1, -1, -1} };
        for (int[] o : offs)
            for (Rotation r : Rotation.values())
                list.add(new BoxCase(o[0], o[1], o[2], r));
        return list;
    }

    // Seed battery for getShuffledTemplates.
    private static final long[] SEEDS = {
        0L, 1L, 2L, 7L, 42L, 99L, 12345L, -1L, -42L, 123456789L,
        9223372036854775807L, -9223372036854775808L,
    };

    // ---- headless StructureTemplateManager backed by client.jar (JigsawPlacementParity style) ----
    private static final class JarTemplateManager extends StructureTemplateManager {
        private ZipFile jar;
        private HolderGetter<Block> blockLookup;
        private java.util.Map<Identifier, StructureTemplate> cache;

        private JarTemplateManager() { super(null, null, null, null); }

        @Override
        public StructureTemplate getOrCreate(final Identifier id) {
            return this.cache.computeIfAbsent(id, this::loadFromJar);
        }

        @Override
        public Optional<StructureTemplate> get(final Identifier id) {
            return Optional.of(this.getOrCreate(id));
        }

        private StructureTemplate loadFromJar(final Identifier id) {
            String entryName = "data/" + id.getNamespace() + "/structure/" + id.getPath() + ".nbt";
            java.util.zip.ZipEntry entry = this.jar.getEntry(entryName);
            if (entry == null) {
                throw new IllegalStateException("missing structure nbt in client.jar: " + entryName);
            }
            try (java.io.InputStream in = this.jar.getInputStream(entry)) {
                CompoundTag tag = NbtIo.readCompressed(in, NbtAccounter.unlimitedHeap());
                StructureTemplate template = new StructureTemplate();
                template.load(this.blockLookup, tag);
                return template;
            } catch (java.io.IOException e) {
                throw new RuntimeException("cannot load structure " + id + " from client.jar", e);
            }
        }
    }

    @SuppressWarnings("unchecked")
    private static StructureTemplateManager makeJarManager(final ZipFile jar, final HolderGetter<Block> blockLookup) throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        Object unsafe = theUnsafe.get(null);
        Method allocateInstance = unsafeClass.getMethod("allocateInstance", Class.class);
        JarTemplateManager m = (JarTemplateManager) allocateInstance.invoke(unsafe, JarTemplateManager.class);
        setField(JarTemplateManager.class, m, "jar", jar);
        setField(JarTemplateManager.class, m, "blockLookup", blockLookup);
        setField(JarTemplateManager.class, m, "cache", new java.util.HashMap<Identifier, StructureTemplate>());
        return m;
    }

    private static void setField(final Class<?> owner, final Object target, final String name, final Object value) throws Exception {
        Field f = owner.getDeclaredField(name);
        f.setAccessible(true);
        f.set(target, value);
    }

    @SuppressWarnings("unchecked")
    private static <T> T getField(final Class<?> owner, final Object target, final String name) throws Exception {
        Field f = owner.getDeclaredField(name);
        f.setAccessible(true);
        return (T) f.get(target);
    }

    // Canonical element-location string — must match StructureTemplatePool.h locationString().
    private static String elementLocation(final StructurePoolElement element) {
        if (element instanceof SinglePoolElement single) {
            try {
                return single.getTemplateLocation().toString();
            } catch (RuntimeException e) {
                return element.toString();
            }
        }
        if (element instanceof ListPoolElement list) {
            StringBuilder sb = new StringBuilder("list[");
            List<StructurePoolElement> sub = list.getElements();
            for (int i = 0; i < sub.size(); i++) {
                if (i > 0) sb.append(',');
                sb.append(elementLocation(sub.get(i)));
            }
            return sb.append(']').toString();
        }
        // EmptyPoolElement / FeaturePoolElement: match locationString() ("Empty"/"Feature").
        if (element.toString().equals("Empty")) return "Empty";
        if (element.toString().startsWith("Feature")) return "Feature";
        return element.toString();
    }

    // Collect every template Identifier a pool element references (Single/Legacy ->
    // its location; List -> its sub-elements' locations). Used to base64-ship the .nbt.
    private static void collectLocations(final StructurePoolElement element, final Set<String> out) {
        if (element instanceof SinglePoolElement single) {
            try { out.add(single.getTemplateLocation().toString()); } catch (RuntimeException ignored) {}
        } else if (element instanceof ListPoolElement list) {
            for (StructurePoolElement sub : list.getElements()) collectLocations(sub, out);
        }
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(final String[] args) throws Exception {
        // Silence any logging before bootStrap so javac/stderr stay clean.
        try {
            Class<?> configurator = Class.forName("org.apache.logging.log4j.core.config.Configurator");
            Class<?> level = Class.forName("org.apache.logging.log4j.Level");
            Object off = level.getField("OFF").get(null);
            configurator.getMethod("setRootLevel", level).invoke(null, off);
        } catch (Throwable ignored) {
            // log4j core not present / API moved — non-fatal; nothing logs at this point.
        }

        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        final HolderLookup.Provider holders = VanillaRegistries.createLookup();
        final HolderLookup.RegistryLookup<StructureTemplatePool> poolLookup =
            holders.lookupOrThrow(Registries.TEMPLATE_POOL);

        final HolderGetter<Block> blockLookup = BuiltInRegistries.BLOCK;
        final java.nio.file.Path jarPath = java.nio.file.Path.of("26.1.2", "client.jar");
        try (ZipFile jar = new ZipFile(jarPath.toFile())) {
            final StructureTemplateManager manager = makeJarManager(jar, blockLookup);

            // The private `templates` (expanded) and `fallback` fields of the pool.
            Field templatesField = StructureTemplatePool.class.getDeclaredField("templates");
            templatesField.setAccessible(true);
            Field fallbackField = StructureTemplatePool.class.getDeclaredField("fallback");
            fallbackField.setAccessible(true);

            // First, gather + emit every referenced .nbt (base64 + size) across all pools.
            Set<String> allLocations = new LinkedHashSet<>();
            for (final String key : POOL_KEYS) {
                StructureTemplatePool pool = poolLookup.getOrThrow(
                    ResourceKey.create(Registries.TEMPLATE_POOL, Identifier.withDefaultNamespace(key))).value();
                List<StructurePoolElement> templates = (List<StructurePoolElement>) templatesField.get(pool);
                for (StructurePoolElement e : templates) collectLocations(e, allLocations);
            }
            for (final String loc : allLocations) {
                Identifier id = Identifier.parse(loc);
                String entryName = "data/" + id.getNamespace() + "/structure/" + id.getPath() + ".nbt";
                java.util.zip.ZipEntry entry = jar.getEntry(entryName);
                if (entry == null) throw new IllegalStateException("missing structure nbt: " + entryName);
                final byte[] raw;
                try (java.io.InputStream in = jar.getInputStream(entry)) { raw = in.readAllBytes(); }
                final String b64 = Base64.getEncoder().encodeToString(raw);
                StructureTemplate t = manager.getOrCreate(id);
                Vec3i size = t.getSize();
                out.println("NBT\t" + loc + "\t" + b64
                    + "\t" + size.getX() + "\t" + size.getY() + "\t" + size.getZ());
            }

            // Now per-pool: fallback, size, maxSize, expanded templates, shuffles, boxes.
            for (final String key : POOL_KEYS) {
                StructureTemplatePool pool = poolLookup.getOrThrow(
                    ResourceKey.create(Registries.TEMPLATE_POOL, Identifier.withDefaultNamespace(key))).value();

                net.minecraft.core.Holder<StructureTemplatePool> fallback =
                    (net.minecraft.core.Holder<StructureTemplatePool>) fallbackField.get(pool);
                String fallbackId = fallback.unwrapKey()
                    .map(rk -> rk.identifier().toString())
                    .orElse("minecraft:empty");

                int size = pool.size();
                int maxSize = pool.getMaxSize(manager);
                out.println("POOL\t" + key + "\t" + fallbackId + "\t" + size + "\t" + maxSize);

                List<StructurePoolElement> templates = (List<StructurePoolElement>) templatesField.get(pool);

                // Expanded template order + projection + groundLevelDelta.
                for (int i = 0; i < templates.size(); i++) {
                    StructurePoolElement e = templates.get(i);
                    out.println("TEMPLATE\t" + key + "\t" + i
                        + "\t" + elementLocation(e)
                        + "\t" + e.getProjection().getSerializedName()
                        + "\t" + e.getGroundLevelDelta());
                }

                // getShuffledTemplates(random) — the structure WorldgenRandom over a
                // LegacyRandomSource (the random type addPieces uses). Emit the post-
                // shuffle ORDER as element (location, projection): the placer-visible
                // sequence. (Duplicate elements e.g. EmptyPoolElement.INSTANCE x6 are
                // identity-indistinguishable, so location/projection — not slot index —
                // is the unambiguous, comparable observable; both sides produce the same
                // permutation by construction over the same-length nextInt stream.)
                for (final long seed : SEEDS) {
                    RandomSource random = new WorldgenRandom(new LegacyRandomSource(seed));
                    List<StructurePoolElement> shuffled = pool.getShuffledTemplates(random);
                    for (int oi = 0; oi < shuffled.size(); oi++) {
                        StructurePoolElement e = shuffled.get(oi);
                        out.println("SHUFFLE\t" + key + "\t" + seed
                            + "\t" + oi + "\t" + elementLocation(e)
                            + "\t" + e.getProjection().getSerializedName());
                    }
                }

                // Element boxes at the fixed (pos, rotation) battery. EmptyPoolElement
                // throws in getBoundingBox (filter me!) — emit BOX_EMPTY for those so the
                // C++ test asserts the same throw behaviour.
                for (int i = 0; i < templates.size(); i++) {
                    StructurePoolElement e = templates.get(i);
                    for (final BoxCase c : boxCases()) {
                        BlockPos pos = new BlockPos(c.x(), c.y(), c.z());
                        if (e.toString().equals("Empty")) {
                            out.println("BOX_EMPTY\t" + key + "\t" + i
                                + "\t" + c.rotation().ordinal()
                                + "\t" + c.x() + "\t" + c.y() + "\t" + c.z());
                            continue;
                        }
                        BoundingBox bb = e.getBoundingBox(manager, pos, c.rotation());
                        out.println("BOX\t" + key + "\t" + i
                            + "\t" + c.rotation().ordinal()
                            + "\t" + c.x() + "\t" + c.y() + "\t" + c.z()
                            + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                            + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                            + "\t" + bb.getYSpan());
                    }
                }
            }
        }

        out.flush();
    }
}
