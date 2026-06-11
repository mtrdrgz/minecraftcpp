import java.lang.reflect.Method;

import net.minecraft.world.item.DyeColor;

// Ground-truth emitter for the pure static variant math of
// net.minecraft.world.entity.animal.fish.TropicalFish (MC 26.1.2).
//
// Drives the REAL class + its REAL Pattern/Base enums via reflection (the helpers
// are package/private static), so every emitted value is what the shipping jar
// computes. NO TropicalFish instance is constructed — only pure static helpers
// and enum accessors are invoked.
//
// Methods exercised:
//   Pattern.getPackedId()                 -> base.id | index << 8
//   Pattern.byId(int)                     -> ByIdMap.sparse(..., KOB)
//   TropicalFish.packVariant(Pattern, DyeColor, DyeColor)  (private)
//   TropicalFish.getBaseColor(int)        -> DyeColor.byId(packed >> 16 & 0xFF)
//   TropicalFish.getPatternColor(int)     -> DyeColor.byId(packed >> 24 & 0xFF)
//   TropicalFish.getPattern(int)          -> Pattern.byId(packed & 65535)
//
// Tab-separated rows (all ints decimal):
//   PATTERN   <ordinal> <packedId> <baseId>
//   PATTERNBYID <queryPackedId> <resultOrdinal>
//   PACK      <patternOrdinal> <baseColorId> <patternColorId> <packedVariant>
//   BASECOLOR <packedVariant> <baseColorId>
//   PATCOLOR  <packedVariant> <patternColorId>
//   GETPAT    <packedVariant> <resultPatternOrdinal>
public class TropicalFishVariantParity {
   static final java.io.PrintStream O = System.out;

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> fishClass = Class.forName("net.minecraft.world.entity.animal.fish.TropicalFish");
      Class<?> patternClass =
          Class.forName("net.minecraft.world.entity.animal.fish.TropicalFish$Pattern");

      @SuppressWarnings("unchecked")
      Enum<?>[] patterns = ((Class<? extends Enum<?>>) patternClass).getEnumConstants();

      Method getPackedId = patternClass.getMethod("getPackedId");
      Method patternBase = patternClass.getMethod("base");
      Method patternById = patternClass.getMethod("byId", int.class);

      Method baseIdField; // Base.id is private; read via the Base enum
      Class<?> baseClass =
          Class.forName("net.minecraft.world.entity.animal.fish.TropicalFish$Base");

      Method packVariant =
          fishClass.getDeclaredMethod("packVariant", patternClass, DyeColor.class, DyeColor.class);
      packVariant.setAccessible(true);
      Method getBaseColor = fishClass.getMethod("getBaseColor", int.class);
      Method getPatternColor = fishClass.getMethod("getPatternColor", int.class);
      Method getPattern = fishClass.getMethod("getPattern", int.class);

      // ---- PATTERN: ordinal, packedId, baseId for every Pattern constant. ----
      for (Enum<?> p : patterns) {
         int packed = (Integer) getPackedId.invoke(p);
         Enum<?> base = (Enum<?>) patternBase.invoke(p);
         int baseId = readBaseId(baseClass, base);
         O.println("PATTERN\t" + p.ordinal() + "\t" + packed + "\t" + baseId);
      }

      // ---- PATTERNBYID over a representative battery of int keys. ----
      // In-range packed ids (0..5, 256..261), boundary/out-of-range, negatives,
      // and extremes. ByIdMap.sparse falls back to KOB for any unknown key.
      int[] keyQueries = new int[] {
         Integer.MIN_VALUE, -261, -1, 0, 1, 2, 3, 4, 5, 6, 7,
         255, 256, 257, 258, 259, 260, 261, 262, 512, 65535, 65536,
         16777216, Integer.MAX_VALUE
      };
      for (int q : keyQueries) {
         Enum<?> r = (Enum<?>) patternById.invoke(null, q);
         O.println("PATTERNBYID\t" + q + "\t" + r.ordinal());
      }

      // ---- PACK: pack every (pattern x baseColor x patternColor). ----
      DyeColor[] dyes = DyeColor.values();
      for (Enum<?> p : patterns) {
         for (DyeColor base : dyes) {
            for (DyeColor pat : dyes) {
               int packed = (Integer) packVariant.invoke(null, p, base, pat);
               O.println("PACK\t" + p.ordinal() + "\t" + base.getId() + "\t" + pat.getId()
                         + "\t" + packed);
            }
         }
      }

      // ---- BASECOLOR / PATCOLOR / GETPAT over a representative battery of
      // packed variants: every real PACK plus adversarial raw ints that stress
      // the arithmetic-shift / mask / sparse-fallback paths. ----
      java.util.List<Integer> packedQueries = new java.util.ArrayList<>();
      // All real packed variants.
      for (Enum<?> p : patterns) {
         for (DyeColor base : dyes) {
            for (DyeColor pat : dyes) {
               packedQueries.add((Integer) packVariant.invoke(null, p, base, pat));
            }
         }
      }
      // Adversarial raw packed ints: negatives (high bit set -> arithmetic >>),
      // out-of-table colour bytes (16..255 -> DyeColor.byId ZERO -> white),
      // out-of-table pattern low-words (-> Pattern.byId KOB), and extremes.
      int[] rawQueries = new int[] {
         0, 1, 5, 256, 261,
         0x000F0000, 0x000F0F00, 0x0F0F0005, 0x0F0F0105,
         0x80000000, 0xFF000000, 0x00FF0000, 0xFFFF0105, 0xABCD0105,
         0x10000000, 0x00100000, 0x00001000, -1, -256, -65536,
         Integer.MIN_VALUE, Integer.MAX_VALUE
      };
      for (int r : rawQueries) packedQueries.add(r);

      for (int packed : packedQueries) {
         DyeColor base = (DyeColor) getBaseColor.invoke(null, packed);
         DyeColor pat = (DyeColor) getPatternColor.invoke(null, packed);
         Enum<?> patName = (Enum<?>) getPattern.invoke(null, packed);
         O.println("BASECOLOR\t" + packed + "\t" + base.getId());
         O.println("PATCOLOR\t" + packed + "\t" + pat.getId());
         O.println("GETPAT\t" + packed + "\t" + patName.ordinal());
      }
   }

   // Base.id is a private final int; read it reflectively for the given constant.
   private static int readBaseId(Class<?> baseClass, Enum<?> base) throws Exception {
      java.lang.reflect.Field f = baseClass.getDeclaredField("id");
      f.setAccessible(true);
      return f.getInt(base);
   }
}
