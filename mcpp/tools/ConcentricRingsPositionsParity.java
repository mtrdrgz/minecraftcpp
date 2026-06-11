// Ground-truth generator for the concentric-rings (stronghold) ring-position
// SKELETON, using the REAL decompiled 26.1.2 code:
//   net.minecraft.world.level.chunk.ChunkGeneratorStructureState
//       .generateRingPositions(Holder<StructureSet>, ConcentricRingsStructurePlacement)
// driven via the public ChunkGeneratorStructureState.getRingPositionsFor(placement).
//
// We never replicate the algorithm here — we call the REAL method. To isolate the
// PURE deterministic skeleton (the part the C++ port reproduces) from the
// world-coupled biome search, we wrap the real overworld BiomeSource in a subclass
// whose findBiomeHorizontal(...) returns null. The real generateRingPositions then
// falls back to `new ChunkPos(initialX, initialZ)` for every ring, i.e. exactly the
// RNG + spread/circle/angle skeleton, computed by the REAL LegacyRandomSource and
// the REAL Math.cos/sin/round.
//
//   tools/run_groundtruth.ps1 -Tool ConcentricRingsPositionsParity -Out mcpp/build/concentric_rings.tsv
//
// TSV rows (leading TAG; ints decimal; longs decimal):
//   PARAMS  <seed>  <distance>  <count>  <spread>
//   POS     <seed>  <i>  <chunkX>  <chunkZ>

import com.mojang.datafixers.util.Pair;
import com.mojang.serialization.MapCodec;
import java.util.List;
import java.util.function.Predicate;
import java.util.stream.Stream;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.HolderLookup;
import net.minecraft.core.registries.Registries;
import net.minecraft.data.registries.VanillaRegistries;
import net.minecraft.util.RandomSource;
import net.minecraft.world.level.ChunkPos;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeSource;
import net.minecraft.world.level.biome.Climate;
import net.minecraft.world.level.biome.MultiNoiseBiomeSource;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterList;
import net.minecraft.world.level.biome.MultiNoiseBiomeSourceParameterLists;
import net.minecraft.world.level.chunk.ChunkGeneratorStructureState;
import net.minecraft.world.level.levelgen.NoiseGeneratorSettings;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.structure.StructureSet;
import net.minecraft.world.level.levelgen.structure.placement.ConcentricRingsStructurePlacement;
import net.minecraft.world.level.levelgen.structure.placement.StructurePlacement;

public class ConcentricRingsPositionsParity {

    // A BiomeSource that delegates everything to a real overworld source but whose
    // findBiomeHorizontal(...) returns null, so generateRingPositions emits the pure
    // ring skeleton (no biome snapping). possibleBiomes() is forwarded so the
    // stronghold structure set still passes hasBiomesForStructureSet.
    private static final class NoSnapBiomeSource extends BiomeSource {
        private final BiomeSource delegate;

        NoSnapBiomeSource(final BiomeSource delegate) {
            this.delegate = delegate;
        }

        @Override
        public java.util.Set<Holder<Biome>> possibleBiomes() {
            return this.delegate.possibleBiomes();
        }

        @Override
        protected Stream<Holder<Biome>> collectPossibleBiomes() {
            return this.delegate.possibleBiomes().stream();
        }

        @Override
        protected MapCodec<? extends BiomeSource> codec() {
            return null; // never invoked on this path
        }

        @Override
        public Holder<Biome> getNoiseBiome(final int quartX, final int quartY, final int quartZ, final Climate.Sampler sampler) {
            return this.delegate.getNoiseBiome(quartX, quartY, quartZ, sampler);
        }

        @Override
        public Pair<BlockPos, Holder<Biome>> findBiomeHorizontal(
            final int x,
            final int y,
            final int z,
            final int searchRadius,
            final Predicate<Holder<Biome>> allowed,
            final RandomSource random,
            final Climate.Sampler sampler
        ) {
            return null; // force the pure new ChunkPos(initialX, initialZ) fallback
        }
    }

    @SuppressWarnings({"deprecation", "unchecked"})
    public static void main(final String[] args) throws Exception {
        final java.io.PrintStream out = System.out;
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        final HolderLookup.Provider holders = VanillaRegistries.createLookup();
        final HolderLookup.RegistryLookup<StructureSet> setLookup = holders.lookupOrThrow(Registries.STRUCTURE_SET);
        final HolderLookup.RegistryLookup<MultiNoiseBiomeSourceParameterList> presets =
            holders.lookupOrThrow(Registries.MULTI_NOISE_BIOME_SOURCE_PARAMETER_LIST);
        final BiomeSource overworld =
            MultiNoiseBiomeSource.createFromPreset(presets.getOrThrow(MultiNoiseBiomeSourceParameterLists.OVERWORLD));
        final BiomeSource biomeSource = new NoSnapBiomeSource(overworld);

        // Find every concentric_rings structure set holder (stronghold).
        final java.util.List<Holder.Reference<StructureSet>> ringSetHolders = new java.util.ArrayList<>();
        setLookup.listElements().forEach(h -> {
            if (h.value().placement() instanceof ConcentricRingsStructurePlacement) {
                ringSetHolders.add(h);
            }
        });

        // generateRingPositions(...) is private and reads only this.concentricRingsSeed,
        // this.biomeSource and this.randomState.sampler(); it never touches the
        // possibleStructureSets list. We build the state with createForFlat (no biome
        // tag dereference), then reflectively set concentricRingsSeed = seed (what
        // createForNormal would do) and invoke the REAL method directly. The biome
        // search returns null (NoSnapBiomeSource), so each ring is the pure skeleton.
        final java.lang.reflect.Field seedField =
            ChunkGeneratorStructureState.class.getDeclaredField("concentricRingsSeed");
        seedField.setAccessible(true);
        final java.lang.reflect.Method genMethod = ChunkGeneratorStructureState.class.getDeclaredMethod(
            "generateRingPositions", Holder.class, ConcentricRingsStructurePlacement.class);
        genMethod.setAccessible(true);

        final long[] seeds = {
            0L, 1L, -1L, 42L, -42L, 123456789L, -5123456789L, 9876543210L,
            2L, 7L, 100L, 1000L, 31415926535L, -31415926535L,
            4503599627370496L, -4503599627370496L, 9223372036854775807L, -9223372036854775808L,
            1234L, 5678L, 999999999L, -999999999L, 314159L, 271828L, 1618033L, -1618033L,
            777L, -777L, 65536L, -65536L
        };

        for (final long seed : seeds) {
            final RandomState randomState = RandomState.create(holders, NoiseGeneratorSettings.OVERWORLD, seed);
            final ChunkGeneratorStructureState state = ChunkGeneratorStructureState.createForFlat(
                randomState, seed, biomeSource, Stream.empty());
            // concentricRingsSeed is a final field set to 0 by createForFlat; mirror
            // createForNormal's `concentricRingsSeed = levelSeed`.
            seedField.setLong(state, seed);

            for (final Holder.Reference<StructureSet> holder : ringSetHolders) {
                final ConcentricRingsStructurePlacement rings =
                    (ConcentricRingsStructurePlacement) holder.value().placement();
                final Object future;
                try {
                    future = genMethod.invoke(state, holder, rings);
                } catch (final java.lang.reflect.InvocationTargetException e) {
                    throw e.getCause() instanceof RuntimeException re ? re : new RuntimeException(e.getCause());
                }
                @SuppressWarnings("unchecked")
                final List<ChunkPos> positions =
                    ((java.util.concurrent.CompletableFuture<List<ChunkPos>>) future).join();

                out.println("PARAMS\t" + seed + "\t" + rings.distance() + "\t" + rings.count() + "\t" + rings.spread());
                for (int i = 0; i < positions.size(); i++) {
                    final ChunkPos pos = positions.get(i);
                    out.println("POS\t" + seed + "\t" + i + "\t" + pos.x() + "\t" + pos.z());
                }
            }
        }
    }
}
