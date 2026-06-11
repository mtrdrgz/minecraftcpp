// Ground-truth generator for the PURE coord/dimension/symmetry helpers of
//   net.minecraft.world.item.crafting.ShapedRecipePattern  (Minecraft 26.1.2)
// plus  net.minecraft.util.Util.isSymmetrical.
//
// Every row is produced by REFLECTIVELY invoking the REAL static methods of the
// shipped classes (so the values come straight from the jar, not a transcription):
//
//   ShapedRecipePattern.firstNonEmpty(String)   (private static int)
//   ShapedRecipePattern.lastNonEmpty(String)    (private static int)
//   ShapedRecipePattern.shrink(List<String>)    (package-private static String[])
//   Util.isSymmetrical(int,int,List<T>)         (public static <T> boolean)
//
// Strings are exchanged base64(UTF-8); shrink results are emitted as the count
// followed by one base64 column per output row.
//
//   TAGS:
//     FNE <lineB64> <firstNonEmpty>
//     LNE <lineB64> <lastNonEmpty>
//     SHRINK <nIn> <inRowB64>... -> <nOut> <outRowB64>...
//     SYM <width> <height> <symmetrical(0|1)> <cellsB64-joined-with-|>
//
// The Ingredient/ItemStack/world-coupled matches(CraftingInput) path is NOT driven
// here (it needs the registries + real ItemStacks); only the self-contained math is.
//
// Run via:
//   mcpp/tools/run_groundtruth.ps1 -Tool ShapedRecipePatternParity -Out mcpp/build/shaped_recipe_pattern.tsv

import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.Base64;
import java.util.List;

public class ShapedRecipePatternParity {
   // Capture stdout at class-load so bootstrap chatter can't pollute the TSV.
   static final java.io.PrintStream O = System.out;
   static final Base64.Encoder B64 = Base64.getEncoder();

   static String b64(String s) { return B64.encodeToString(s.getBytes(StandardCharsets.UTF_8)); }

   static Method mFirst, mLast, mShrink, mSym;

   public static void main(String[] args) throws Exception {
      // Util / ShapedRecipePattern static init touches registry-aware code paths in
      // the surrounding classes; bootstrap to be safe (the helpers themselves are pure).
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> srp = Class.forName("net.minecraft.world.item.crafting.ShapedRecipePattern");
      mFirst  = srp.getDeclaredMethod("firstNonEmpty", String.class);
      mLast   = srp.getDeclaredMethod("lastNonEmpty", String.class);
      mShrink = srp.getDeclaredMethod("shrink", List.class);
      mFirst.setAccessible(true);
      mLast.setAccessible(true);
      mShrink.setAccessible(true);

      Class<?> util = Class.forName("net.minecraft.util.Util");
      mSym = util.getMethod("isSymmetrical", int.class, int.class, List.class);

      // ---- firstNonEmpty / lastNonEmpty battery ----------------------------
      String[] lines = {
         "", " ", "  ", "   ",
         "A", "AB", "ABC",
         " A", "  A", "   A",
         "A ", "A  ", "A   ",
         " A ", "  A  ", " AB ",
         "A B", "A  B", " A B ",
         "XYZ", " XY", "XY ", "  Z",
         "#", " # ", "#  #", "  ##  ",
      };
      for (String line : lines) {
         int f = (Integer) mFirst.invoke(null, line);
         int l = (Integer) mLast.invoke(null, line);
         O.println("FNE\t" + b64(line) + "\t" + f);
         O.println("LNE\t" + b64(line) + "\t" + l);
      }

      // ---- shrink battery (1..3 rows, mirrors real recipe shapes) ----------
      List<List<String>> patterns = new ArrayList<>();
      patterns.add(List.of("X"));
      patterns.add(List.of(" X "));
      patterns.add(List.of("XXX"));
      patterns.add(List.of("X X"));
      patterns.add(List.of("   "));                 // all-blank single row -> empty
      patterns.add(List.of("XX", "XX"));
      patterns.add(List.of(" X", "X "));
      patterns.add(List.of("X X", "XXX", "X X"));
      patterns.add(List.of("   ", " X ", "   "));    // padded single ingredient
      patterns.add(List.of("   ", "   ", "   "));    // all blank -> empty
      patterns.add(List.of(" XX", " XX", "   "));    // trailing blank row
      patterns.add(List.of("   ", " XX", " XX"));    // leading blank row
      patterns.add(List.of("X  ", "   ", "  X"));    // diagonal corners
      patterns.add(List.of(" X ", " X ", " X "));    // vertical bar, blank sides
      patterns.add(List.of("AB ", "CD ", "   "));
      patterns.add(List.of(" AB", " CD", "   "));
      patterns.add(List.of("#"));
      patterns.add(List.of(" # ", "###", " # "));
      patterns.add(List.of("  X", "  X", "  X"));    // right column only
      patterns.add(List.of("X  ", "X  ", "X  "));    // left column only

      for (List<String> p : patterns) {
         @SuppressWarnings("unchecked")
         String[] out = (String[]) mShrink.invoke(null, p);
         StringBuilder sb = new StringBuilder("SHRINK\t");
         sb.append(p.size());
         for (String r : p) sb.append('\t').append(b64(r));
         sb.append('\t').append("->").append('\t').append(out.length);
         for (String r : out) sb.append('\t').append(b64(r));
         O.println(sb.toString());
      }

      // ---- isSymmetrical battery (List<Character> so .equals is value-equality) --
      // Each case: width, height, and a flat cell list (chars). We emit the cells so
      // the C++ side reconstructs the same List and runs its index-equality version.
      String[][] symCases = {
         {"1", "1", "A"},
         {"1", "3", "ABC"},
         {"3", "1", "ABA"},
         {"3", "1", "ABC"},
         {"2", "1", "AA"},
         {"2", "1", "AB"},
         {"3", "3", "ABACBCABA"},     // each row a palindrome -> symmetrical
         {"3", "3", "ABCDEFGHI"},     // not symmetrical
         {"3", "2", "XYXZWZ"},        // rows "XYX","ZWZ" -> symmetrical
         {"3", "2", "XYXZWQ"},        // second row "ZWQ" -> not
         {"4", "1", "ABBA"},          // even width palindrome
         {"4", "1", "ABCA"},          // not
         {"5", "1", "ABCBA"},         // odd width palindrome
         {"5", "2", "ABCBAEDFDE"},    // rows "ABCBA","EDFDE" -> symmetrical
         {"2", "2", "AABB"},          // rows "AA","BB" -> symmetrical
         {"2", "2", "ABBA"},          // rows "AB","BA" -> not (AB != BA mirror)
      };
      for (String[] c : symCases) {
         int w = Integer.parseInt(c[0]);
         int h = Integer.parseInt(c[1]);
         String cells = c[2];
         List<Character> list = new ArrayList<>();
         for (int i = 0; i < cells.length(); i++) list.add(cells.charAt(i));
         boolean sym = (Boolean) mSym.invoke(null, w, h, list);
         O.println("SYM\t" + w + "\t" + h + "\t" + (sym ? 1 : 0) + "\t" + b64(cells));
      }
   }
}
