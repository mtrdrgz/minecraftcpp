// StructureStartsGenParity — IN-PROCESS assembly-stage oracle for structure starts.
//
// Runs the REAL ChunkGenerator.createStructures algorithm (STRUCTURE_STARTS stage)
// for every chunk in a region, with the real NoiseBasedChunkGenerator + overworld
// MultiNoiseBiomeSource + RandomState for the given seed, and dumps every VALID
// StructureStart in the same TSV format as StructureStartsDump:
//
//   S\t<structureId>\t<chunkX>\t<chunkZ>\t<references>\t<childCount>
//   C\t<pieceId>\t<minX>\t<minY>\t<minZ>\t<maxX>\t<maxY>\t<maxZ>\t<O>\t<GD>
//
// Unlike StructureStartsDump (which reads the server's saved .mca and therefore
// mixes ASSEMBLY-time bounding boxes for pre-FEATURES chunks with PLACEMENT-
// adjusted boxes for fully generated chunks — template structures like ocean
// ruins / shipwrecks / buried treasure move their templatePosition in
// postProcess), this oracle is ALWAYS assembly-time: it is exactly what the
// server computes at STRUCTURE_STARTS, for any region, with no chunk-status
// ambiguity and no server run. Piece rows are produced by serializing each
// start with the REAL StructureStart.createTag, so ids/BB/O/GD encoding is
// byte-identical to the server's NBT.
//
// The block-placement stage (postProcess reading real terrain) is verified
// separately against the server world via ServerChunkDump block TSVs.
//
// Usage (from repo root, after tools/provision_runtime.sh --parity):
//   CP="26.1.2/parity_classes:26.1.2/client.jar:26.1.2/libs/*"
//   26.1.2/jdk25/bin/javac -cp "$CP" -d 26.1.2/parity_classes tools/StructureStartsGenParity.java
//   26.1.2/jdk25/bin/java -cp "$CP" StructureStartsGenParity <seed> <fromCx> <fromCz> <toCx> <toCz>
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.util.List;
import java.util.Optional;
import java.util.zip.ZipFile;

import net.minecraft.core.Holder;
import net.minecraft.core.HolderGetter;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.HolderSet;
import net.minecraft.core.MappedRegistry;
import net.minecraft.core.Registry;
import net.minecraft.core.RegistrationInfo;
import net.minecraft.core.RegistryAccess;
import net.minecraft.core.registries.BuiltInRegistries;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.nbt.CompoundTag;
import net.minecraft.nbt.ListTag;
import net.minecraft.nbt.NbtIo;
import net.minecraft.resources.Identifier;
import net.minecraft.resources.ResourceKey;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.Level;
import net.minecraft.world.level.LevelHeightAccessor;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.levelgen.LegacyRandomSource;
import net.minecraft.world.level.levelgen.NoiseBasedChunkGenerator;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.WorldgenRandom;
import net.minecraft.world.level.levelgen.structure.Structure;
import net.minecraft.world.level.levelgen.structure.StructureSet;
import net.minecraft.world.level.levelgen.structure.StructureStart;
import net.minecraft.world.level.levelgen.structure.pieces.StructurePieceSerializationContext;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplate;
import net.minecraft.world.level.levelgen.structure.templatesystem.StructureTemplateManager;

public class StructureStartsGenParity {
    // Real stderr, captured at class load BEFORE Bootstrap.bootStrap() wraps System.err.
    private static final java.io.PrintStream ERR = System.err;

    // ---- headless StructureTemplateManager backed by client.jar ----
    // Same pattern as JigsawPlacementParity.JarTemplateManager: allocated without a
    // constructor so the parent's ResourceManager/DataFixer fields stay null; the two
    // methods pool elements / template structures call are overridden to read the
    // template .nbt straight out of client.jar.
    private static final class JarTemplateManager extends StructureTemplateManager {
        private ZipFile jar;
        private HolderGetter<Block> blockLookup;
        private java.util.Map<Identifier, StructureTemplate> cache = new java.util.HashMap<>();

        private JarTemplateManager(final ZipFile jar, final HolderGetter<Block> blockLookup) {
            super(null, null, null, null); // never called — Unsafe.allocateInstance
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
            try {
                var entry = this.jar.getEntry(entryName);
                if (entry == null) throw new IllegalStateException("template not in jar: " + entryName);
                try (var in = this.jar.getInputStream(entry)) {
                    CompoundTag nbt = NbtIo.readCompressed(in, net.minecraft.nbt.NbtAccounter.unlimitedHeap());
                    StructureTemplate template = new StructureTemplate();
                    template.load(this.blockLookup, nbt);
                    return template;
                }
            } catch (java.io.IOException e) {
                throw new RuntimeException("failed to read " + entryName, e);
            }
        }
    }

    private static StructureTemplateManager makeJarManager(final ZipFile jar, final HolderGetter<Block> blockLookup) throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field theUnsafe = unsafeClass.getDeclaredField("theUnsafe");
        theUnsafe.setAccessible(true);
        Object unsafe = theUnsafe.get(null);
        Method allocateInstance = unsafeClass.getMethod("allocateInstance", Class.class);
        JarTemplateManager manager = (JarTemplateManager) allocateInstance.invoke(unsafe, JarTemplateManager.class);
        setField(manager, "jar", jar);
        setField(manager, "blockLookup", blockLookup);
        setField(manager, "cache", new java.util.HashMap<Identifier, StructureTemplate>());
        return manager;
    }

    private static void setField(final Object target, final String name, final Object value) throws Exception {
        Field f = JarTemplateManager.class.getDeclaredField(name);
        f.setAccessible(true);
        f.set(target, value);
    }

    // ---- biome tag binding ----
    // VanillaRegistries.createLookup() is the DATAGEN lookup: biome Holder.References
    // carry no tags and every structure's `biomes()` HolderSet.Named is an unbound
    // NamedSet ("can't be dereferenced during construction"). The generate path needs
    // both: Named.contains(holder) delegates to holder.is(tag) (isValidBiome), and
    // hasBiomesForStructureSet/concentric rings stream the NamedSet contents. Bind them
    // from the SHIPPED tag JSON (26.1.2/data/minecraft/tags/worldgen/biome/**), exactly
    // what the real server loads.
    private static java.util.Map<String, java.util.List<String>> loadRawBiomeTags() throws Exception {
        java.nio.file.Path root = java.nio.file.Path.of("26.1.2", "data", "minecraft", "tags", "worldgen", "biome");
        java.util.Map<String, java.util.List<String>> raw = new java.util.HashMap<>();
        try (var stream = java.nio.file.Files.walk(root)) {
            for (java.nio.file.Path file : stream.filter(p -> p.toString().endsWith(".json")).toList()) {
                var obj = com.google.gson.JsonParser.parseString(java.nio.file.Files.readString(file)).getAsJsonObject();
                java.util.List<String> entries = new java.util.ArrayList<>();
                for (var value : obj.getAsJsonArray("values")) {
                    if (value.isJsonObject()) entries.add(value.getAsJsonObject().get("id").getAsString());
                    else entries.add(value.getAsString());
                }
                String rel = root.relativize(file).toString().replace(java.io.File.separatorChar, '/');
                raw.put("minecraft:" + rel.substring(0, rel.length() - ".json".length()), entries);
            }
        }
        return raw;
    }

    private static void resolveBiomeTag(final String id,
                                        final java.util.Map<String, java.util.List<String>> raw,
                                        final java.util.Map<String, java.util.LinkedHashSet<String>> resolved,
                                        final java.util.Set<String> visiting) {
        if (resolved.containsKey(id) || !visiting.add(id)) return;
        java.util.LinkedHashSet<String> outSet = new java.util.LinkedHashSet<>();
        for (String entry : raw.getOrDefault(id, java.util.List.of())) {
            if (entry.startsWith("#")) {
                String nested = entry.substring(1);
                if (!nested.contains(":")) nested = "minecraft:" + nested;
                resolveBiomeTag(nested, raw, resolved, visiting);
                outSet.addAll(resolved.getOrDefault(nested, new java.util.LinkedHashSet<>()));
            } else {
                outSet.add(entry.contains(":") ? entry : "minecraft:" + entry);
            }
        }
        resolved.put(id, outSet);
    }

    // The datagen lookup hands out HolderSet.emptyNamed anonymous sets whose contents()
    // ALWAYS throws (even after bind), so binding in place is impossible — build a real
    // HolderSet.Named for the same tag, bind it from the shipped JSON, and swap it into
    // the owning object's field. StructureSettings is a record (final fields immune to
    // Field.set), so the swap goes through Unsafe.putObject.
    private static Object theUnsafe;
    private static Method objectFieldOffset, putObject;

    private static void initUnsafe() throws Exception {
        Class<?> unsafeClass = Class.forName("sun.misc.Unsafe");
        Field f = unsafeClass.getDeclaredField("theUnsafe");
        f.setAccessible(true);
        theUnsafe = f.get(null);
        objectFieldOffset = unsafeClass.getMethod("objectFieldOffset", Field.class);
        putObject = unsafeClass.getMethod("putObject", Object.class, long.class, Object.class);
    }

    private static void swapField(final Object owner, final Class<?> ownerClass, final String fieldName,
                                  final Object value) throws Exception {
        Field f = ownerClass.getDeclaredField(fieldName);
        long offset = (Long) objectFieldOffset.invoke(theUnsafe, f);
        putObject.invoke(theUnsafe, owner, offset, value);
    }

    @SuppressWarnings("unchecked")
    private static HolderSet<Biome> boundReplacement(final HolderSet<Biome> set,
                                                     final java.util.Map<String, java.util.LinkedHashSet<String>> tags,
                                                     final java.util.Map<String, Holder<Biome>> biomesById) throws Exception {
        if (!(set instanceof HolderSet.Named<Biome> named) || named.isBound()) return null;
        String tagId = named.key().location().toString();
        java.util.List<Holder<Biome>> contents = new java.util.ArrayList<>();
        for (String biomeId : tags.getOrDefault(tagId, new java.util.LinkedHashSet<>())) {
            Holder<Biome> holder = biomesById.get(biomeId);
            if (holder != null) contents.add(holder);
        }
        java.lang.reflect.Constructor<HolderSet.Named> ctor = HolderSet.Named.class.getDeclaredConstructor(
            net.minecraft.core.HolderOwner.class, net.minecraft.tags.TagKey.class);
        ctor.setAccessible(true);
        Field ownerField = HolderSet.Named.class.getDeclaredField("owner");
        ownerField.setAccessible(true);
        HolderSet.Named<Biome> replacement =
            ctor.newInstance(ownerField.get(named), named.key());
        Method bind = HolderSet.Named.class.getDeclaredMethod("bind", List.class);
        bind.setAccessible(true);
        bind.invoke(replacement, contents);
        return replacement;
    }

    private static void bindBiomeTags(final HolderLookup.Provider provider,
                                      final Registry<Structure> structureRegistry,
                                      final HolderLookup.RegistryLookup<StructureSet> setLookup) throws Exception {
        java.util.Map<String, java.util.List<String>> raw = loadRawBiomeTags();
        java.util.Map<String, java.util.LinkedHashSet<String>> tags = new java.util.HashMap<>();
        for (String id : raw.keySet()) resolveBiomeTag(id, raw, tags, new java.util.LinkedHashSet<>());

        java.util.Map<String, Holder<Biome>> biomesById = new java.util.HashMap<>();
        java.util.Map<String, java.util.List<net.minecraft.tags.TagKey<Biome>>> tagsPerBiome = new java.util.HashMap<>();
        provider.lookupOrThrow(Registries.BIOME).listElements().forEach(holder ->
            biomesById.put(holder.key().identifier().toString(), holder));
        for (var e : tags.entrySet()) {
            var tagKey = net.minecraft.tags.TagKey.create(Registries.BIOME, Identifier.parse(e.getKey()));
            for (String biomeId : e.getValue()) {
                tagsPerBiome.computeIfAbsent(biomeId, k -> new java.util.ArrayList<>()).add(tagKey);
            }
        }
        // Holder.Reference.bindTags is package-private → reflection.
        Method bindTags = Holder.Reference.class.getDeclaredMethod("bindTags", java.util.Collection.class);
        bindTags.setAccessible(true);
        for (var e : biomesById.entrySet()) {
            bindTags.invoke(e.getValue(), tagsPerBiome.getOrDefault(e.getKey(), java.util.List.of()));
        }
        // Replace the unbound NamedSets the generate path dereferences: each structure's
        // settings.biomes and each concentric-rings placement's preferredBiomes.
        initUnsafe();
        // StructureSettings is a record (Unsafe refuses record fields), so build a NEW
        // settings record with the bound biome set and swap the `settings` field of the
        // (regular) Structure class instead.
        Field settingsField = Structure.class.getDeclaredField("settings");
        settingsField.setAccessible(true);
        for (Structure structure : structureRegistry) {
            HolderSet<Biome> replacement = boundReplacement(structure.biomes(), tags, biomesById);
            if (replacement != null) {
                Structure.StructureSettings newSettings = new Structure.StructureSettings(
                    replacement, structure.spawnOverrides(), structure.step(), structure.terrainAdaptation());
                swapField(structure, Structure.class, "settings", newSettings);
            }
        }
        setLookup.listElements().forEach(setHolder -> {
            if (setHolder.value().placement() instanceof
                    net.minecraft.world.level.levelgen.structure.placement.ConcentricRingsStructurePlacement rings) {
                try {
                    HolderSet<Biome> replacement = boundReplacement(rings.preferredBiomes(), tags, biomesById);
                    if (replacement != null) {
                        swapField(rings, rings.getClass(), "preferredBiomes", replacement);
                    }
                } catch (Exception ex) {
                    throw new RuntimeException(ex);
                }
            }
        });
    }

    private static <X> Registry<X> copyRegistry(final ResourceKey<? extends Registry<X>> key,
                                                final HolderLookup.RegistryLookup<X> source) {
        MappedRegistry<X> registry = new MappedRegistry<>(key, com.mojang.serialization.Lifecycle.stable());
        source.listElements().forEach(holder -> registry.register(holder.key(), holder.value(), RegistrationInfo.BUILT_IN));
        return registry.freeze();
    }

    public static void main(final String[] args) {
        try {
            run(args);
        } catch (Throwable t) {
            t.printStackTrace(ERR);
            System.exit(1);
        }
    }

    private static void run(final String[] args) throws Exception {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        final long seed = args.length > 0 ? Long.parseLong(args[0]) : 1L;
        final int fromCx = args.length > 1 ? Integer.parseInt(args[1]) : -5;
        final int fromCz = args.length > 2 ? Integer.parseInt(args[2]) : -5;
        final int toCx   = args.length > 3 ? Integer.parseInt(args[3]) : 90;
        final int toCz   = args.length > 4 ? Integer.parseInt(args[4]) : 30;

        HolderLookup.Provider provider = VanillaRegistries.createLookup();

        // RegistryAccess with the registries the generate path dereferences:
        // TEMPLATE_POOL (JigsawPlacement.addPieces) and STRUCTURE
        // (StructureStart.createTag writes the id via STRUCTURE.getKey).
        Registry<net.minecraft.world.level.levelgen.structure.pools.StructureTemplatePool> poolRegistry =
            copyRegistry(Registries.TEMPLATE_POOL, provider.lookupOrThrow(Registries.TEMPLATE_POOL));
        Registry<Structure> structureRegistry =
            copyRegistry(Registries.STRUCTURE, provider.lookupOrThrow(Registries.STRUCTURE));
        RegistryAccess registryAccess =
            new RegistryAccess.ImmutableRegistryAccess(List.of(poolRegistry, structureRegistry)).freeze();

        // Real overworld generator stack for this seed.
        Holder<NoiseGeneratorSettings> settingsHolder =
            provider.lookupOrThrow(Registries.NOISE_SETTINGS).getOrThrow(NoiseGeneratorSettings.OVERWORLD);
        NoiseGeneratorSettings settings = settingsHolder.value();
        Holder<MultiNoiseBiomeSourceParameterList> overworldPreset =
            provider.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST)
                .getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD);
        BiomeSource biomeSource = MultiNoiseBiomeSource.createFromPreset(overworldPreset);
        NoiseBasedChunkGenerator generator = new NoiseBasedChunkGenerator(biomeSource, settingsHolder);
        RandomState randomState = RandomState.create(provider, NoiseGeneratorSettings.OVERWORLD, seed);
        LevelHeightAccessor heightAccessor =
            LevelHeightAccessor.create(settings.noiseSettings().minY(), settings.noiseSettings().height());

        bindBiomeTags(provider, structureRegistry, provider.lookupOrThrow(Registries.STRUCTURE_SET));

        ChunkGeneratorStructureState state = ChunkGeneratorStructureState.createForNormal(
            randomState, seed, biomeSource, provider.lookupOrThrow(Registries.STRUCTURE_SET));

        ZipFile jar = new ZipFile(java.nio.file.Path.of("26.1.2", "client.jar").toFile());
        StructureTemplateManager templateManager = makeJarManager(jar, BuiltInRegistries.BLOCK);
        StructurePieceSerializationContext serializationContext =
            new StructurePieceSerializationContext(null, registryAccess, templateManager);

        // --probe mode: dump the biome-gate inputs for one chunk (heights at the
        // chunk middle for both WG heightmaps + the noise biome at those stubs).
        if (args.length > 5 && "--probe".equals(args[5])) {
            int cx = fromCx, cz = fromCz;
            int mx = cx * 16 + 8, mz = cz * 16 + 8;
            int ws = generator.getFirstOccupiedHeight(mx, mz,
                net.minecraft.world.level.levelgen.Heightmap.Types.WORLD_SURFACE_WG, heightAccessor, randomState);
            int of = generator.getFirstOccupiedHeight(mx, mz,
                net.minecraft.world.level.levelgen.Heightmap.Types.OCEAN_FLOOR_WG, heightAccessor, randomState);
            var sampler = randomState.sampler();
            var biomeWs = biomeSource.getNoiseBiome(mx >> 2, ws >> 2, mz >> 2, sampler);
            var biomeOf = biomeSource.getNoiseBiome(mx >> 2, of >> 2, mz >> 2, sampler);
            ERR.println("probe chunk (" + cx + "," + cz + ") mid=(" + mx + "," + mz + ")");
            ERR.println("  WORLD_SURFACE_WG occupied = " + ws + "  biome@stub = " + biomeWs.unwrapKey().map(k -> k.identifier().toString()).orElse("?"));
            ERR.println("  OCEAN_FLOOR_WG occupied  = " + of + "  biome@stub = " + biomeOf.unwrapKey().map(k -> k.identifier().toString()).orElse("?"));
            return;
        }

        java.util.TreeMap<String, Integer> tally = new java.util.TreeMap<>();
        int totalStarts = 0;

        // Per chunk: the EXACT ChunkGenerator.createStructures dispatch (no existing
        // starts, references=0 — a fresh world at STRUCTURE_STARTS). Within a chunk the
        // sets run in possibleStructureSets order; starts print in generation order.
        for (int cz = fromCz; cz <= toCz; cz++) {
            for (int cx = fromCx; cx <= toCx; cx++) {
                ChunkPos chunkPos = new ChunkPos(cx, cz);
                for (Holder<StructureSet> setHolder : state.possibleStructureSets()) {
                    StructurePlacement placement = setHolder.value().placement();
                    List<StructureSet.StructureSelectionEntry> structures = setHolder.value().structures();
                    if (!placement.isStructureChunk(state, cx, cz)) continue;

                    StructureStart produced = null;
                    if (structures.size() == 1) {
                        produced = tryGenerate(structures.get(0), registryAccess, generator, biomeSource,
                            randomState, templateManager, seed, chunkPos, heightAccessor);
                    } else {
                        java.util.ArrayList<StructureSet.StructureSelectionEntry> options =
                            new java.util.ArrayList<>(structures);
                        WorldgenRandom random = new WorldgenRandom(new LegacyRandomSource(0L));
                        random.setLargeFeatureSeed(seed, cx, cz);
                        int total = 0;
                        for (StructureSet.StructureSelectionEntry option : options) total += option.weight();
                        while (!options.isEmpty()) {
                            int choice = random.nextInt(total);
                            int index = 0;
                            for (StructureSet.StructureSelectionEntry option : options) {
                                choice -= option.weight();
                                if (choice < 0) break;
                                index++;
                            }
                            StructureSet.StructureSelectionEntry selected = options.get(index);
                            produced = tryGenerate(selected, registryAccess, generator, biomeSource,
                                randomState, templateManager, seed, chunkPos, heightAccessor);
                            if (produced != null) break;
                            options.remove(index);
                            total -= selected.weight();
                        }
                    }

                    if (produced != null) {
                        CompoundTag tag = produced.createTag(serializationContext, chunkPos);
                        String id = tag.getStringOr("id", "");
                        ListTag children = tag.getListOrEmpty("Children");
                        out.println("S\t" + id + "\t" + cx + "\t" + cz + "\t0\t" + children.size());
                        for (int ci = 0; ci < children.size(); ci++) {
                            CompoundTag piece = children.getCompoundOrEmpty(ci);
                            String pid = piece.getStringOr("id", "");
                            int[] bb = piece.getIntArray("BB").orElse(new int[6]);
                            int o = piece.getIntOr("O", 0);
                            int gd = piece.getIntOr("GD", 0);
                            StringBuilder sb = new StringBuilder("C\t").append(pid);
                            for (int k = 0; k < 6; k++) sb.append('\t').append(k < bb.length ? bb[k] : 0);
                            sb.append('\t').append(o).append('\t').append(gd);
                            out.println(sb);
                        }
                        tally.merge(id, 1, Integer::sum);
                        totalStarts++;
                    }
                }
            }
        }

        System.err.println("totalStarts=" + totalStarts);
        for (var e : tally.entrySet()) System.err.println("  " + e.getKey() + " = " + e.getValue());
        jar.close();
    }

    private static StructureStart tryGenerate(
            final StructureSet.StructureSelectionEntry selected,
            final RegistryAccess registryAccess,
            final NoiseBasedChunkGenerator generator,
            final BiomeSource biomeSource,
            final RandomState randomState,
            final StructureTemplateManager templateManager,
            final long seed,
            final ChunkPos chunkPos,
            final LevelHeightAccessor heightAccessor) {
        Structure structure = selected.structure().value();
        HolderSet<Biome> allowed = structure.biomes();
        java.util.function.Predicate<Holder<Biome>> biomePredicate = allowed::contains;
        StructureStart start = structure.generate(
            selected.structure(), Level.OVERWORLD, registryAccess, generator, biomeSource, randomState,
            templateManager, seed, chunkPos, 0, heightAccessor, biomePredicate);
        return start.isValid() ? start : null;
    }
}
