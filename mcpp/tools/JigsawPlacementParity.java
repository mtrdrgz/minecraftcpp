// Ground-truth generator for the REAL 26.1.2 jigsaw assembly loop:
//   net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement.addPieces(
//       Structure.GenerationContext, Holder<StructureTemplatePool> startPool,
//       Optional<Identifier> startJigsaw, int maxDepth, BlockPos position,
//       boolean doExpansionHack, Optional<Heightmap.Types> projectStartToHeightmap,
//       JigsawStructure.MaxDistance maxDistanceFromCenter, PoolAliasLookup,
//       DimensionPadding, LiquidSettings)
//
// This is the oracle for the C++ 1:1 port of the jigsaw assembly loop. RULE #0:
// we drive the REAL method; we NEVER reimplement assembly Java-side. We load the
// REAL pillager_outpost JigsawStructure from the vanilla registry and read EVERY
// addPieces argument off it reflectively (start_pool, size, project_start_to_heightmap,
// max_distance_from_center, dimension_padding, liquid_settings, pool_aliases,
// start_height) so the call is byte-for-byte what JigsawStructure.findGenerationPoint
// would pass — then we invoke JigsawPlacement.addPieces directly.
//
// DECOUPLING FROM THE NOISE HEIGHTMAP: addPieces' only world coupling is
// chunkGenerator.getFirstFreeHeight(x,z,heightmap,heightAccessor,randomState)
// (JigsawPlacement.java:112 for the start projection, and :401/:448 for non-rigid
// junctions). We supply a concrete ChunkGenerator subclass (StubGenerator) whose
// getFirstFreeHeight/getBaseHeight return a FIXED CONSTANT (64) for ALL (x,z) and
// throw for every other abstract method (none are reached by addPieces). This
// certifies the ASSEMBLY LOGIC (RNG draws, rotation, alignment, collision shape
// joins, junction math, expansion hack) in isolation from the noise pipeline,
// which is certified separately.
//
// The headless StructureTemplateManager is a subclass allocated WITHOUT running any
// constructor (reflective Unsafe.allocateInstance) whose getOrCreate(Identifier)/
// get(Identifier) load minecraft:pillager_outpost/<name> from 26.1.2/client.jar at
// data/minecraft/structure/pillager_outpost/<name>.nbt (NbtIo.readCompressed +
// StructureTemplate.load). Every pool-element path into the manager funnels through
// getOrCreate (SinglePoolElement.getTemplate), so these two overrides suffice.
//
//   tools/run_groundtruth.ps1 -Tool JigsawPlacementParity -Out mcpp/build/jigsaw_placement.tsv
//
// TSV rows (leading TAG; all ints decimal):
//   PIECE <seed> <pieceIndex> <elementLocation> <rotationOrdinal> <posX> <posY> <posZ>
//         <bbMinX> <bbMinY> <bbMinZ> <bbMaxX> <bbMaxY> <bbMaxZ> <groundLevelDelta> <numJunctions>
//   COUNT <seed> <numPieces>
// Pieces are emitted in the builder's natural piece order (StructurePiecesBuilder.build()).

import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Optional;
import java.util.zip.ZipFile;

import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.Registry;
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
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.NoiseColumn;
import net.minecraft.world.level.StructureManager;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldGenerationContext;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.heightproviders.HeightProvider;
import net.minecraft.world.level.levelgen.placement.PlacedFeature;
import net.minecraft.world.level.levelgen.structure.BoundingBox;
import net.minecraft.world.level.levelgen.structure.PoolElementStructurePiece;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructurePiece;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePiecesBuilder;
import net.minecraft.world.level.levelgen.structure.pools.DimensionPadding;
import net.minecraft.world.level.levelgen.structure.pools.FeaturePoolElement;
import net.minecraft.world.level.levelgen.structure.pools.JigsawPlacement;
import net.minecraft.world.level.levelgen.structure.pools.ListPoolElement;
import net.minecraft.world.level.levelgen.structure.pools.SinglePoolElement;
import net.minecraft.world.level.levelgen.structure.pools.StructurePoolElement;
import net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool;
import net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasBinding;
import net.minecraft.world.level.levelgen.structure.pools.alias.PoolAliasLookup;
import net.minecraft.world.level.levelgen.structure.structures.JigsawStructure;
import net.minecraft.world.level.levelgen.structure.templatesystem.LiquidSettings;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;

public final class JigsawPlacementParity {
    // The constant the stub generator reports for getFirstFreeHeight at EVERY (x,z),
    // decoupling the assembly from the noise heightmap.
    private static final int FIXED_FREE_HEIGHT = 64;
    // Overworld height span (NoiseGeneratorSettings.OVERWORLD noise settings: minY=-64,
    // height=384). Only consumed by WorldGenerationContext / the height accessor; the
    // start_height for pillager_outpost is absolute:0 so the value never depends on it.
    private static final int WORLD_MIN_Y = -64;
    private static final int WORLD_HEIGHT = 384;
    private static final int GEN_DEPTH = 384;

    // ---- headless StructureTemplateManager backed by client.jar ----
    // Subclass that overrides the only two methods pool elements call (getOrCreate / get).
    // It is allocated WITHOUT a constructor (Unsafe.allocateInstance) so the parent's
    // ResourceManager/LevelStorageAccess/DataFixer/HolderGetter fields stay null and
    // unused. getOrCreate is the single funnel: SinglePoolElement.getTemplate maps the
    // element's Either<Identifier,StructureTemplate> through manager::getOrCreate.
    private static final class JarTemplateManager extends StructureTemplateManager {
        private final ZipFile jar;
        private final HolderGetter<Block> blockLookup;
        private final java.util.Map<Identifier, StructureTemplate> cache = new java.util.HashMap<>();

        private JarTemplateManager(final ZipFile jar, final HolderGetter<Block> blockLookup) {
            // never called — instances are produced via Unsafe.allocateInstance — but a
            // (private, super-less) ctor cannot exist for a subclass, so this is only here
            // to host the fields; we set them reflectively after allocation instead.
            super(null, null, null, null);
            this.jar = jar;
            this.blockLookup = blockLookup;
        }

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

    // ---- stub ChunkGenerator: getFirstFreeHeight == FIXED_FREE_HEIGHT everywhere ----
    // addPieces only ever calls getFirstFreeHeight on the generator. We override it (and
    // getBaseHeight, which the default getFirstFreeHeight would otherwise delegate to) to
    // return the constant; getGenDepth/getMinY/getSeaLevel return real spans because
    // WorldGenerationContext (used by the start_height sample) reads getGenDepth. All other
    // abstract methods throw — they are never reached by addPieces.
    private static final class StubGenerator extends ChunkGenerator {
        private StubGenerator(final BiomeSource biomeSource) {
            super(biomeSource);
        }

        @Override
        public int getFirstFreeHeight(final int x, final int z, final Heightmap.Types type,
                                      final LevelHeightAccessor heightAccessor, final RandomState randomState) {
            return FIXED_FREE_HEIGHT;
        }

        @Override
        public int getBaseHeight(final int x, final int z, final Heightmap.Types type,
                                 final LevelHeightAccessor heightAccessor, final RandomState randomState) {
            return FIXED_FREE_HEIGHT;
        }

        @Override
        public int getGenDepth() {
            return GEN_DEPTH;
        }

        @Override
        public int getMinY() {
            return WORLD_MIN_Y;
        }

        @Override
        public int getSeaLevel() {
            return 63;
        }

        @Override
        public NoiseColumn getBaseColumn(final int x, final int z, final LevelHeightAccessor heightAccessor, final RandomState randomState) {
            throw new UnsupportedOperationException("getBaseColumn not reached by addPieces");
        }

        @Override
        protected com.mojang.serialization.MapCodec<? extends ChunkGenerator> codec() {
            throw new UnsupportedOperationException("codec not reached by addPieces");
        }

        @Override
        public void applyCarvers(final net.minecraft.server.level.WorldGenRegion region, final long seed,
                                 final RandomState randomState, final BiomeManager biomeManager,
                                 final StructureManager structureManager, final ChunkAccess chunk) {
            throw new UnsupportedOperationException("applyCarvers not reached by addPieces");
        }

        @Override
        public void buildSurface(final net.minecraft.server.level.WorldGenRegion level, final StructureManager structureManager,
                                 final RandomState randomState, final ChunkAccess protoChunk) {
            throw new UnsupportedOperationException("buildSurface not reached by addPieces");
        }

        @Override
        public void spawnOriginalMobs(final net.minecraft.server.level.WorldGenRegion region) {
            throw new UnsupportedOperationException("spawnOriginalMobs not reached by addPieces");
        }

        @Override
        public java.util.concurrent.CompletableFuture<ChunkAccess> fillFromNoise(final Blender blender, final RandomState randomState,
                                                                                  final StructureManager structureManager, final ChunkAccess centerChunk) {
            throw new UnsupportedOperationException("fillFromNoise not reached by addPieces");
        }

        @Override
        public void addDebugScreenInfo(final List<String> result, final RandomState randomState, final BlockPos feetPos) {
            throw new UnsupportedOperationException("addDebugScreenInfo not reached by addPieces");
        }
    }

    // Allocate a JarTemplateManager without running any constructor, then set its fields.
    @SuppressWarnings("unchecked")
    private static StructureTemplateManager makeJarManager(final ZipFile jar, final HolderGetter<Block> blockLookup) throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        Object unsafe = theUnsafe.get(null);
        Method allocateInstance = unsafeClass.getMethod("allocateInstance", Class.class);
        JarTemplateManager manager = (JarTemplateManager) allocateInstance.invoke(unsafe, JarTemplateManager.class);
        setField(JarTemplateManager.class, manager, "jar", jar);
        setField(JarTemplateManager.class, manager, "blockLookup", blockLookup);
        setField(JarTemplateManager.class, manager, "cache", new java.util.HashMap<Identifier, StructureTemplate>());
        return manager;
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

    // Build a RegistryAccess that contains the TEMPLATE_POOL registry (the only registry
    // addPieces dereferences: registryAccess.lookupOrThrow(Registries.TEMPLATE_POOL)).
    private static <X> Registry<X> copyRegistry(final ResourceKey<? extends Registry<X>> key,
                                                final HolderLookup.RegistryLookup<X> source) {
        MappedRegistry<X> registry = new MappedRegistry<>(key, com.mojang.serialization.Lifecycle.stable());
        source.listElements().forEach(holder -> registry.register(holder.key(), holder.value(), RegistrationInfo.BUILT_IN));
        return registry.freeze();
    }

    // Canonical, deterministic element-location string for the TSV. SinglePoolElement (and
    // its subclass LegacySinglePoolElement) expose getTemplateLocation(); ListPoolElement
    // wraps several elements (e.g. towers: watchtower + watchtower_overgrown) — we join
    // their resolved locations with '|'.
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
        if (element instanceof FeaturePoolElement) {
            // FeaturePoolElement has no template location — identify it by its
            // PlacedFeature reference id (stable across runs), matching the C++
            // StructurePoolElement.locationString(): "feature[<id>]".
            try {
                Holder<PlacedFeature> pf = getField(FeaturePoolElement.class, element, "feature");
                String id = pf.unwrapKey().map(k -> k.identifier().toString()).orElse(pf.toString());
                return "feature[" + id + "]";
            } catch (Exception e) {
                return element.toString();
            }
        }
        return element.toString();
    }

    // Ship every template referenced anywhere in the structure's template_pool subtree as
    // base64 of the raw gzip .nbt (from client.jar), so the C++ gate parses identical bytes.
    private static void emitNbt(final java.io.PrintStream out, final String name, final String prefix, final ZipFile jar) throws Exception {
        java.nio.file.Path dir = java.nio.file.Path.of("26.1.2", "data", "minecraft", "worldgen", "template_pool", prefix);
        java.util.LinkedHashSet<String> locs = new java.util.LinkedHashSet<>();
        java.util.regex.Pattern p = java.util.regex.Pattern.compile("\"location\"\\s*:\\s*\"([^\"]+)\"");
        java.util.List<java.nio.file.Path> files;
        try (java.util.stream.Stream<java.nio.file.Path> w = java.nio.file.Files.walk(dir)) {
            files = w.filter(x -> x.toString().endsWith(".json")).sorted().toList();
        }
        for (java.nio.file.Path f : files) {
            java.util.regex.Matcher m = p.matcher(java.nio.file.Files.readString(f));
            while (m.find()) locs.add(m.group(1));
        }
        for (String loc : locs) {
            int colon = loc.indexOf(':');
            String ns = colon < 0 ? "minecraft" : loc.substring(0, colon);
            String path = colon < 0 ? loc : loc.substring(colon + 1);
            String entry = "data/" + ns + "/structure/" + path + ".nbt";
            java.util.zip.ZipEntry ze = jar.getEntry(entry);
            if (ze == null) { System.err.println("missing " + entry); continue; }
            byte[] bytes;
            try (java.io.InputStream in = jar.getInputStream(ze)) { bytes = in.readAllBytes(); }
            out.println("NBT\t" + name + "\t" + loc + "\t" + java.util.Base64.getEncoder().encodeToString(bytes));
        }
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(final String[] args) throws Exception {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        final HolderLookup.Provider holders = VanillaRegistries.createLookup();

        // RegistryAccess carrying TEMPLATE_POOL (copied from the provider so addPieces'
        // registryAccess.lookupOrThrow(Registries.TEMPLATE_POOL) resolves).
        Registry<StructureTemplatePool> poolRegistry =
            copyRegistry(Registries.TEMPLATE_POOL, holders.lookupOrThrow(Registries.TEMPLATE_POOL));
        RegistryAccess registryAccess =
            new RegistryAccess.ImmutableRegistryAccess(List.of(poolRegistry)).freeze();

        // Overworld biome source — supplied to the stub generator's ctor and to the
        // GenerationContext; addPieces never queries it.
        HolderLookup.RegistryLookup<MultiNoiseBiomeSourceParameterList> presets =
            holders.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST);
        BiomeSource biomeSource =
            MultiNoiseBiomeSource.createFromPreset(presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD));

        ChunkGenerator stubGenerator = new StubGenerator(biomeSource);
        LevelHeightAccessor heightAccessor = LevelHeightAccessor.create(WORLD_MIN_Y, WORLD_HEIGHT);

        // Headless template manager over client.jar (kept open for the whole run).
        java.nio.file.Path jarPath = java.nio.file.Path.of("26.1.2", "client.jar");
        ZipFile jar = new ZipFile(jarPath.toFile());
        HolderGetter<Block> blockLookup = BuiltInRegistries.BLOCK;
        StructureTemplateManager templateManager = makeJarManager(jar, blockLookup);

        // ---- shared (structure-independent) setup ----
        // Battery: 0..199, plus large/negative edge seeds.
        java.util.List<Long> seedList = new java.util.ArrayList<>();
        for (long s = 0; s < 200; s++) seedList.add(s);
        for (long s : new long[]{
            -1L, -42L, -200L, 123456789L, -123456789L,
            9223372036854775807L, -9223372036854775808L,
            4503599627370496L, -4503599627370496L, 31415926535L, -31415926535L
        }) seedList.add(s);

        final ChunkPos chunkPos = new ChunkPos(0, 0);

        Method addPieces = JigsawPlacement.class.getDeclaredMethod(
            "addPieces",
            Structure.GenerationContext.class, Holder.class, Optional.class, int.class, BlockPos.class,
            boolean.class, Optional.class, JigsawStructure.MaxDistance.class, PoolAliasLookup.class,
            DimensionPadding.class, LiquidSettings.class);
        addPieces.setAccessible(true);

        // ---- structures to dump (name, registry key, template_pool prefix) ----
        // Each runs the REAL addPieces; rows are tagged by structure name. NBT rows ship every
        // template in the structure's pool subtree; CONFIG carries the per-structure addPieces args.
        record SD(String name, ResourceKey<Structure> key, String prefix) {}
        java.util.List<SD> structs = java.util.List.of(
            new SD("pillager_outpost", net.minecraft.world.level.levelgen.structure.BuiltinStructures.PILLAGER_OUTPOST, "pillager_outpost"),
            new SD("trail_ruins", net.minecraft.world.level.levelgen.structure.BuiltinStructures.TRAIL_RUINS, "trail_ruins"),
            // village_plains exercises feature_pool_element + legacy_single_pool_element +
            // empty_pool_element; its pools live under template_pool/village (prefix "village").
            new SD("village_plains", net.minecraft.world.level.levelgen.structure.BuiltinStructures.VILLAGE_PLAINS, "village"),
            // bastion_remnant: large single-element pools (expansion OFF, no aliases).
            new SD("bastion_remnant", net.minecraft.world.level.levelgen.structure.BuiltinStructures.BASTION_REMNANT, "bastion")
            // NOTE: ancient_city is the only vanilla structure using start_jigsaw_name, but
            // it CANNOT be certified here: its walls/no_corners pool references
            // ancient_city/walls/intact_horizontal_wall_stairs_5.nbt, which is ABSENT from
            // this repo's client.jar AND server.jar (only _1.._4 ship). The real generator
            // would place _5; we must not fabricate it (RULE #0). The startJigsaw anchoring
            // entry path is ported 1:1 in the C++ test but stays gate-uncertified until that
            // asset is available (or another start_jigsaw_name structure exists).
        );

        for (SD sd : structs) {
            JigsawStructure structure = (JigsawStructure) holders.lookupOrThrow(Registries.STRUCTURE)
                .getOrThrow(sd.key()).value();
            Holder<StructureTemplatePool> startPool = getField(JigsawStructure.class, structure, "startPool");
            Optional<Identifier> startJigsawName = getField(JigsawStructure.class, structure, "startJigsawName");
            int maxDepth = getField(JigsawStructure.class, structure, "maxDepth");
            HeightProvider startHeight = getField(JigsawStructure.class, structure, "startHeight");
            boolean useExpansionHack = getField(JigsawStructure.class, structure, "useExpansionHack");
            Optional<Heightmap.Types> projectStartToHeightmap = getField(JigsawStructure.class, structure, "projectStartToHeightmap");
            JigsawStructure.MaxDistance maxDistanceFromCenter = getField(JigsawStructure.class, structure, "maxDistanceFromCenter");
            List<PoolAliasBinding> poolAliases = getField(JigsawStructure.class, structure, "poolAliases");
            DimensionPadding dimensionPadding = getField(JigsawStructure.class, structure, "dimensionPadding");
            LiquidSettings liquidSettings = getField(JigsawStructure.class, structure, "liquidSettings");
            Holder<StructureTemplatePool> startPoolResolved = startPool.unwrapKey()
                .<Holder<StructureTemplatePool>>map(key -> poolRegistry.get(key)
                    .map(h -> (Holder<StructureTemplatePool>) h)
                    .orElse(startPool))
                .orElse(startPool);

            emitNbt(out, sd.name(), sd.prefix(), jar);
            boolean cfgEmitted = false;

            for (final long seed : seedList) {
                RandomState randomState = RandomState.create(holders, NoiseGeneratorSettings.OVERWORLD, seed);
                Structure.GenerationContext context = new Structure.GenerationContext(
                    registryAccess, stubGenerator, biomeSource, randomState, templateManager,
                    seed, chunkPos, heightAccessor, h -> true);
                int height = startHeight.sample(context.random(), new WorldGenerationContext(stubGenerator, heightAccessor));
                BlockPos startPos = new BlockPos(chunkPos.getMinBlockX(), height, chunkPos.getMinBlockZ());
                if (!cfgEmitted) {
                    out.println("CONFIG\t" + sd.name()
                        + "\t" + startPool.unwrapKey().map(k -> k.identifier().toString()).orElse("?")
                        + "\t" + maxDepth
                        + "\t" + startPos.getX() + "\t" + startPos.getY() + "\t" + startPos.getZ()
                        + "\t" + (projectStartToHeightmap.isPresent() ? 1 : 0)
                        + "\t" + maxDistanceFromCenter.horizontal()
                        + "\t" + (useExpansionHack ? 1 : 0)
                        + "\t" + dimensionPadding.bottom() + "\t" + dimensionPadding.top()
                        + "\t" + (startJigsawName.isPresent() ? 1 : 0)
                        + "\t" + startJigsawName.map(Identifier::toString).orElse("-"));
                    cfgEmitted = true;
                }
                PoolAliasLookup poolAliasLookup = PoolAliasLookup.create(poolAliases, startPos, context.seed());
                Optional<Structure.GenerationStub> stub = (Optional<Structure.GenerationStub>) addPieces.invoke(
                    null, context, startPoolResolved, startJigsawName, maxDepth, startPos,
                    useExpansionHack, projectStartToHeightmap, maxDistanceFromCenter,
                    poolAliasLookup, dimensionPadding, liquidSettings);
                if (stub.isEmpty()) { out.println("COUNT\t" + sd.name() + "\t" + seed + "\t0"); continue; }
                StructurePiecesBuilder builder = stub.get().getPiecesBuilder();
                List<StructurePiece> pieces = builder.build().pieces();
                int pieceIndex = 0;
                for (StructurePiece piece : pieces) {
                    if (!(piece instanceof PoolElementStructurePiece poolPiece)) continue;
                    BoundingBox bb = poolPiece.getBoundingBox();
                    BlockPos pos = poolPiece.getPosition();
                    out.println("PIECE\t" + sd.name() + "\t" + seed + "\t" + pieceIndex + "\t" + elementLocation(poolPiece.getElement())
                        + "\t" + poolPiece.getRotation().ordinal()
                        + "\t" + pos.getX() + "\t" + pos.getY() + "\t" + pos.getZ()
                        + "\t" + bb.minX() + "\t" + bb.minY() + "\t" + bb.minZ()
                        + "\t" + bb.maxX() + "\t" + bb.maxY() + "\t" + bb.maxZ()
                        + "\t" + poolPiece.getGroundLevelDelta()
                        + "\t" + poolPiece.getJunctions().size());
                    pieceIndex++;
                }
                out.println("COUNT\t" + sd.name() + "\t" + seed + "\t" + pieceIndex);
            }
        }

        out.flush();
        jar.close();
    }
}
