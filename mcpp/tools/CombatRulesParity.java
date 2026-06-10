// Ground-truth generator for net.minecraft.world.damagesource.CombatRules (26.1.2).
//
// Emits tab-separated rows comparing the REAL static armor/magic damage formulas
// against the C++ port (mcpp/src/world/damagesource/CombatRules.h). Floats are
// exchanged as raw IEEE-754 bit patterns (%08x) so the gate is bit-for-bit.
//
//   TAGs:
//     ABSORB <damageBits> <totalArmorBits> <armorToughnessBits> <resultBits>
//        -> CombatRules.getDamageAfterAbsorb(null victim, damage, source, totalArmor, armorToughness)
//           with a DamageSource whose getWeaponItem() == null (no direct entity),
//           so the EnchantmentHelper branch is NOT taken and `victim` is unused.
//     MAGIC  <damageBits> <totalMagicArmorBits> <resultBits>
//        -> CombatRules.getDamageAfterMagicAbsorb(damage, totalMagicArmor)
//
// Run via mcpp/tools/run_groundtruth.ps1 -Tool CombatRulesParity -Out mcpp/build/combat_rules.tsv

import java.lang.reflect.Constructor;
import java.lang.reflect.Method;

public class CombatRulesParity {
   // Capture stdout at class-load so bootstrap chatter can't pollute the TSV.
   static final java.io.PrintStream O = System.out;

   static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

   public static void main(String[] args) throws Exception {
      // Bootstrap so net.minecraft classes resolve (DamageSource pulls registries).
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> combatRules = Class.forName("net.minecraft.world.damagesource.CombatRules");
      Class<?> livingEntity = Class.forName("net.minecraft.world.entity.LivingEntity");
      Class<?> damageSource = Class.forName("net.minecraft.world.damagesource.DamageSource");
      Class<?> entity = Class.forName("net.minecraft.world.entity.Entity");
      Class<?> holder = Class.forName("net.minecraft.core.Holder");
      Class<?> vec3 = Class.forName("net.minecraft.world.phys.Vec3");

      // getDamageAfterAbsorb(LivingEntity, float, DamageSource, float, float)
      Method absorb = combatRules.getDeclaredMethod(
         "getDamageAfterAbsorb", livingEntity, float.class, damageSource, float.class, float.class);
      absorb.setAccessible(true);

      // getDamageAfterMagicAbsorb(float, float)
      Method magic = combatRules.getDeclaredMethod(
         "getDamageAfterMagicAbsorb", float.class, float.class);
      magic.setAccessible(true);

      // Build a DamageSource with NO direct entity so getWeaponItem() returns null
      // (DamageSource.java:67-68). We use the private 4-arg constructor
      // (Holder<DamageType>, directEntity, causingEntity, Vec3) with all-null entity
      // args; getWeaponItem() only reads directEntity, so the type Holder is unused
      // on the path under test and may be null.
      Constructor<?> dsCtor = damageSource.getDeclaredConstructor(holder, entity, entity, vec3);
      dsCtor.setAccessible(true);
      Object source = dsCtor.newInstance(null, null, null, null);

      // Sanity: confirm getWeaponItem() == null so the EnchantmentHelper branch is
      // provably skipped (else the GT would dereference a null victim and throw).
      Method getWeapon = damageSource.getMethod("getWeaponItem");
      Object weapon = getWeapon.invoke(source);
      if (weapon != null) {
         throw new IllegalStateException("getWeaponItem() != null; enchant branch would dereference null victim");
      }

      // ── Exhaustive battery ──────────────────────────────────────────────────
      // Spans every regime of the formula: realArmor clamped to the lower bound
      // (totalArmor*0.2), to the upper bound (20), or left in the linear middle;
      // negative / zero / fractional damage, armor and toughness; large values.
      float[] damages   = { 0f, 0.5f, 1f, 2f, 3.7f, 5f, 8f, 10f, 13.25f, 20f, 40f, 100f, 1000f, -1f, -10f, 0.001f };
      float[] armors    = { 0f, 0.2f, 1f, 2f, 4f, 6.5f, 8f, 10f, 12f, 15f, 18f, 20f, 25f, 40f, 100f, -5f, -0.5f };
      float[] toughness = { 0f, 0.5f, 1f, 2f, 3f, 4f, 6f, 8f, 12f, 16f, 20f, 30f, -2f, -1f, 0.25f };

      for (float dmg : damages) {
         for (float arm : armors) {
            for (float tuf : toughness) {
               float out = (Float) absorb.invoke(null, null, dmg, source, arm, tuf);
               O.println("ABSORB\t" + f(dmg) + "\t" + f(arm) + "\t" + f(tuf) + "\t" + f(out));
            }
         }
      }

      // getDamageAfterMagicAbsorb(damage, totalMagicArmor)
      float[] magicArmors = { 0f, 0.2f, 1f, 2f, 5f, 8f, 10f, 12.5f, 15f, 18f, 19.99f, 20f, 20.0001f,
                              25f, 40f, 100f, -1f, -5f, -0.0001f };
      for (float dmg : damages) {
         for (float ma : magicArmors) {
            float out = (Float) magic.invoke(null, dmg, ma);
            O.println("MAGIC\t" + f(dmg) + "\t" + f(ma) + "\t" + f(out));
         }
      }
   }
}
