import java.lang.reflect.Field;
import java.lang.reflect.Method;

import net.minecraft.world.item.DyeColor;
import net.minecraft.world.level.material.MapColor;

// Ground-truth emitter for net.minecraft.world.item.DyeColor (MC 26.1.2).
//
// Emits tab-separated rows:
//   DYE  <id> <name> <textureDiffuseColor> <mapColorId> <mapColorCol> <fireworkColor> <textColor>
//   BYID <queryId> <resultId>
//   BYFW <queryColor> <resultIndex>   (resultIndex = -1 when byFireworkColor returns null)
//
// All ints emitted in decimal; strings raw.
public class DyeColorParity {
   static final java.io.PrintStream O = System.out;

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      DyeColor[] values = DyeColor.values();

      Method getId = DyeColor.class.getMethod("getId");
      Method getName = DyeColor.class.getMethod("getName");
      Method getTextureDiffuseColor = DyeColor.class.getMethod("getTextureDiffuseColor");
      Method getMapColor = DyeColor.class.getMethod("getMapColor");
      Method getFireworkColor = DyeColor.class.getMethod("getFireworkColor");
      Method getTextColor = DyeColor.class.getMethod("getTextColor");

      Field mapColorId = MapColor.class.getField("id");
      Field mapColorCol = MapColor.class.getField("col");

      // Per-color data rows.
      for (DyeColor c : values) {
         int id = (Integer) getId.invoke(c);
         String name = (String) getName.invoke(c);
         int tdc = (Integer) getTextureDiffuseColor.invoke(c);
         MapColor mc = (MapColor) getMapColor.invoke(c);
         int mcId = mapColorId.getInt(mc);
         int mcCol = mapColorCol.getInt(mc);
         int fw = (Integer) getFireworkColor.invoke(c);
         int tc = (Integer) getTextColor.invoke(c);
         O.println("DYE\t" + id + "\t" + name + "\t" + tdc + "\t" + mcId + "\t" + mcCol
                   + "\t" + fw + "\t" + tc);
      }

      // byId over a battery of in-range and out-of-range ints (ZERO strategy).
      int[] idQueries = new int[] {
         -2147483648, -1000, -16, -2, -1, 0, 1, 2, 7, 8, 14, 15, 16, 17, 100, 2147483647
      };
      for (int q : idQueries) {
         DyeColor r = DyeColor.byId(q);
         int rid = (Integer) getId.invoke(r);
         O.println("BYID\t" + q + "\t" + rid);
      }

      // byFireworkColor: every real firework color must round-trip; plus misses.
      // Build the set of raw fireworkColor inputs straight from the field so we
      // query the exact values used as map keys.
      Field fireworkColorField = DyeColor.class.getDeclaredField("fireworkColor");
      fireworkColorField.setAccessible(true);
      for (DyeColor c : values) {
         int raw = fireworkColorField.getInt(c);
         emitByFirework(raw);
      }
      // Deliberate misses (values not equal to any fireworkColor).
      int[] fwMisses = new int[] {0, 1, -1, 16777215, 12345678, 16383998, 2147483647, -2147483648};
      for (int q : fwMisses) {
         emitByFirework(q);
      }
   }

   static void emitByFirework(int q) throws Exception {
      DyeColor r = DyeColor.byFireworkColor(q);
      int idx;
      if (r == null) {
         idx = -1;
      } else {
         idx = r.ordinal();  // ordinal == id for this enum (continuous 0..15)
      }
      O.println("BYFW\t" + q + "\t" + idx);
   }
}
