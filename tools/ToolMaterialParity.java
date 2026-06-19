// Ground-truth generator for net.minecraft.world.item.ToolMaterial (26.1.2).
//
// Emits one tab-separated row per vanilla static ToolMaterial instance, read by
// REFLECTION from the REAL class (so the values come straight from the jar, not
// a transcription). The ToolMaterial record's four scalar fields are exchanged:
//   durability       int    (decimal)
//   speed            float  (%08x raw IEEE-754 bits)
//   attackDamageBonus float (%08x raw IEEE-754 bits)
//   enchantmentValue int    (decimal)
//
//   TAG:
//     MAT <name> <durability> <speedBits> <attackDamageBonusBits> <enchantmentValue>
//
// The two TagKey fields (incorrectBlocksForDrops, repairItems) are registry-coupled
// and intentionally NOT emitted — they are not part of the ported scalar data.
//
// Run via mcpp/tools/run_groundtruth.ps1 -Tool ToolMaterialParity -Out mcpp/build/tool_material.tsv

import java.lang.reflect.Field;
import java.lang.reflect.Method;

public class ToolMaterialParity {
   // Capture stdout at class-load so bootstrap chatter can't pollute the TSV.
   static final java.io.PrintStream O = System.out;

   static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

   public static void main(String[] args) throws Exception {
      // ToolMaterial's static initializers reference BlockTags/ItemTags, which
      // require the registries to be bootstrapped.
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> cls = Class.forName("net.minecraft.world.item.ToolMaterial");

      // Record accessor methods for the four scalar components.
      Method mDurability  = cls.getMethod("durability");
      Method mSpeed       = cls.getMethod("speed");
      Method mAttackBonus = cls.getMethod("attackDamageBonus");
      Method mEnchant     = cls.getMethod("enchantmentValue");

      // Emit in declaration order (ToolMaterial.java:23-31).
      String[] names = { "WOOD", "STONE", "COPPER", "IRON", "DIAMOND", "GOLD", "NETHERITE" };
      for (String name : names) {
         Field fld = cls.getField(name);
         Object mat = fld.get(null);
         int   durability = (Integer) mDurability.invoke(mat);
         float speed      = (Float)   mSpeed.invoke(mat);
         float atkBonus   = (Float)   mAttackBonus.invoke(mat);
         int   enchant    = (Integer) mEnchant.invoke(mat);
         O.println("MAT\t" + name + "\t" + durability + "\t" + f(speed) + "\t" + f(atkBonus) + "\t" + enchant);
      }
   }
}
