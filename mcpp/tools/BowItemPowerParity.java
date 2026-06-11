// Ground-truth generator for net.minecraft.world.item.BowItem.getPowerForTime
// (26.1.2).
//
// Emits tab-separated rows comparing the REAL static helper against the C++ port
// (mcpp/src/world/item/BowItemPower.h). The single float result is exchanged as a
// raw IEEE-754 bit pattern (%08x) so the gate is bit-for-bit exact.
//
//   TAG:
//     POW <timeHeld:int> <resultBits:%08x>
//        -> BowItem.getPowerForTime(timeHeld)
//
// getPowerForTime is a PURE static helper (int -> float, no world/level/entity/
// registry/GL access), so it is driven reflectively WITHOUT constructing any
// world-coupled instance. We still run the standard bootstrap so the BowItem
// class resolves (its static initializer / superclasses touch registries).
//
// Run via mcpp/tools/run_groundtruth.ps1 -Tool BowItemPowerParity -Out mcpp/build/bow_item_power.tsv

import java.lang.reflect.Method;

public class BowItemPowerParity {
   // Capture stdout at class-load so bootstrap chatter can't pollute the TSV.
   static final java.io.PrintStream O = System.out;

   static String f(float v) { return String.format("%08x", Float.floatToRawIntBits(v)); }

   public static void main(String[] args) throws Exception {
      // Bootstrap so net.minecraft classes resolve.
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> bowItem = Class.forName("net.minecraft.world.item.BowItem");
      Method getPowerForTime = bowItem.getDeclaredMethod("getPowerForTime", int.class);
      getPowerForTime.setAccessible(true);

      // ── Representative battery ────────────────────────────────────────────
      // Covers every regime of the formula:
      //   * 0 .. 20  : the linear-ish rise from 0 to the full-charge point. 20 is
      //                the canonical "fully drawn" tick count where pow first hits
      //                exactly 1.0F (BowItem.releaseUsing uses 1.0F => full power).
      //   * 21 .. 72000 : past full charge — exercises the `pow > 1.0F` clamp; 72000
      //                is BowItem.getUseDuration() (the max a bow can be held).
      //   * negative   : no lower clamp exists, so negative ticks still produce a
      //                positive pow via the quadratic; pins that the port does NOT
      //                add a phantom `max(pow, 0)`.
      //   * large mags : stress the int->float widening and the clamp saturation.
      java.util.LinkedHashSet<Integer> cases = new java.util.LinkedHashSet<>();
      for (int t = 0; t <= 30; t++) cases.add(t);                 // every tick across the rise + just past
      int[] extra = {
         35, 40, 50, 60, 72, 100, 200, 1000, 7200, 20000, 71999, 72000, 72001,
         100000, 1000000, 16777215, 16777216, 16777217,           // float-exact / first-inexact int boundaries
         Integer.MAX_VALUE,
         -1, -2, -3, -5, -10, -19, -20, -21, -40, -100, -1000, -72000,
         -16777216, -16777217, Integer.MIN_VALUE
      };
      for (int t : extra) cases.add(t);

      for (int t : cases) {
         float out = (Float) getPowerForTime.invoke(null, t);
         O.println("POW\t" + t + "\t" + f(out));
      }
   }
}
