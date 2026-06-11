import java.lang.reflect.Method;

import net.minecraft.util.RandomSource;
import net.minecraft.world.entity.animal.panda.Panda;
import net.minecraft.world.level.levelgen.PositionalRandomFactory;

// Ground-truth emitter for net.minecraft.world.entity.animal.panda.Panda.Gene
// (MC 26.1.2). Drives the REAL enum and its three pure static helpers:
//   byId(int)                      -> ByIdMap.continuous ZERO
//   getVariantFromGenes(g, g)      -> recessive-gene resolution (private)
//   getRandom(RandomSource)        -> nextInt(16) branch ladder
//
// getVariantFromGenes and getRandom are private; we reach them by reflection.
// getRandom is exercised by a stub RandomSource that returns a FIXED value from
// nextInt(int) so each branch (0..15) is hit deterministically.
//
// Emits tab-separated rows (all ints decimal):
//   GENE     <id> <name> <isRecessive 0|1>
//   BYID     <queryId> <resultId>
//   VARIANT  <mainId> <hiddenId> <resultId>
//   RANDOM   <draw> <resultId>
public class PandaGeneParity {
   static final java.io.PrintStream O = System.out;

   // Minimal RandomSource whose nextInt(int) always returns `fixed`. Only
   // nextInt(int) is invoked by Panda.Gene.getRandom; everything else throws so
   // a hidden dependency would surface loudly rather than silently pass.
   static final class FixedRandom implements RandomSource {
      private final int fixed;
      FixedRandom(int fixed) { this.fixed = fixed; }
      @Override public int nextInt(int bound) { return this.fixed; }

      @Override public RandomSource fork() { throw new UnsupportedOperationException(); }
      @Override public PositionalRandomFactory forkPositional() { throw new UnsupportedOperationException(); }
      @Override public void setSeed(long seed) { throw new UnsupportedOperationException(); }
      @Override public int nextInt() { throw new UnsupportedOperationException(); }
      @Override public long nextLong() { throw new UnsupportedOperationException(); }
      @Override public boolean nextBoolean() { throw new UnsupportedOperationException(); }
      @Override public float nextFloat() { throw new UnsupportedOperationException(); }
      @Override public double nextDouble() { throw new UnsupportedOperationException(); }
      @Override public double nextGaussian() { throw new UnsupportedOperationException(); }
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> geneClass = Class.forName("net.minecraft.world.entity.animal.panda.Panda$Gene");
      @SuppressWarnings("unchecked")
      Enum<?>[] genes = ((Class<? extends Enum<?>>) geneClass).getEnumConstants();

      Method getId = geneClass.getMethod("getId");
      Method isRecessive = geneClass.getMethod("isRecessive");
      Method getSerializedName = geneClass.getMethod("getSerializedName");
      Method byId = geneClass.getMethod("byId", int.class);
      Method getRandom = geneClass.getMethod("getRandom", RandomSource.class);
      Method getVariantFromGenes = geneClass.getDeclaredMethod("getVariantFromGenes", geneClass, geneClass);
      getVariantFromGenes.setAccessible(true);

      // Per-gene data.
      for (Enum<?> g : genes) {
         int id = (Integer) getId.invoke(g);
         String name = (String) getSerializedName.invoke(g);
         boolean rec = (Boolean) isRecessive.invoke(g);
         O.println("GENE\t" + id + "\t" + name + "\t" + (rec ? 1 : 0));
      }

      // byId over in-range + out-of-range ints (ZERO strategy: else -> NORMAL).
      int[] idQueries = new int[] {
         Integer.MIN_VALUE, -1000, -7, -2, -1, 0, 1, 2, 3, 4, 5, 6, 7, 8, 100, Integer.MAX_VALUE
      };
      for (int q : idQueries) {
         Enum<?> r = (Enum<?>) byId.invoke(null, q);
         O.println("BYID\t" + q + "\t" + (Integer) getId.invoke(r));
      }

      // getVariantFromGenes over the full 7x7 cross product.
      for (Enum<?> main : genes) {
         for (Enum<?> hidden : genes) {
            Enum<?> r = (Enum<?>) getVariantFromGenes.invoke(null, main, hidden);
            O.println("VARIANT\t" + (Integer) getId.invoke(main) + "\t"
                      + (Integer) getId.invoke(hidden) + "\t" + (Integer) getId.invoke(r));
         }
      }

      // getRandom for every possible nextInt(16) draw 0..15.
      for (int draw = 0; draw < 16; draw++) {
         Enum<?> r = (Enum<?>) getRandom.invoke(null, new FixedRandom(draw));
         O.println("RANDOM\t" + draw + "\t" + (Integer) getId.invoke(r));
      }
   }
}
