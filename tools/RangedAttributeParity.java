// Ground-truth emitter for net.minecraft.world.entity.ai.attributes.RangedAttribute.
//
// Verifies the existing C++ port mcpp/src/world/entity/ai/attributes/AttributeMath.h
//   sanitizeValue(value, minValue, maxValue) == RangedAttribute.sanitizeValue(value)
//   getDefaultValue identity (constructor-stored defaultValue round-trip).
//
// Calls the REAL net.minecraft RangedAttribute:
//   - constructor (descriptionId, defaultValue, minValue, maxValue) is public
//   - sanitizeValue(double)  is a public override
//   - getDefaultValue()      is public (inherited from Attribute)
//
// Constraints enforced by the real constructor (so we only feed legal bounds):
//   minValue <= maxValue,  minValue <= defaultValue <= maxValue.
//
// Rows (tab-separated):
//   SANITIZE <min:016x> <max:016x> <value:016x> <result:016x>
//   DEFAULT  <default:016x> <getDefaultValue:016x>
//
// Doubles are emitted as 16-hex of Double.doubleToRawLongBits.
// FINITE / PHYSICAL inputs only (no NaN/Inf/-0.0).

import java.lang.reflect.Constructor;

public class RangedAttributeParity {
   static final java.io.PrintStream O = System.out;

   static String h(double d) {
      return String.format("%016x", Double.doubleToRawLongBits(d));
   }

   public static void main(String[] args) throws Exception {
      net.minecraft.SharedConstants.tryDetectVersion();
      net.minecraft.server.Bootstrap.bootStrap();

      Class<?> cls = Class.forName(
            "net.minecraft.world.entity.ai.attributes.RangedAttribute");
      Constructor<?> ctor = cls.getConstructor(
            String.class, double.class, double.class, double.class);
      ctor.setAccessible(true);

      java.lang.reflect.Method mSan = cls.getMethod("sanitizeValue", double.class);
      mSan.setAccessible(true);
      java.lang.reflect.Method mDef = cls.getMethod("getDefaultValue");
      mDef.setAccessible(true);

      // A spread of legal (min, max) bound pairs covering: equal bounds, negative
      // ranges, asymmetric ranges, large magnitudes, fractional bounds, and the
      // kind of ranges real attributes use (0..1, 0..1024, generic_max_health, etc.).
      double[][] bounds = {
            {0.0, 0.0},
            {0.0, 1.0},
            {-1.0, 1.0},
            {1.0, 1024.0},
            {0.0, 1024.0},
            {0.0, 16.0},
            {-100.0, 100.0},
            {0.0, 2048.0},
            {0.5, 0.5},
            {-2.0, 4.0},
            {0.1, 0.9},
            {-0.5, 0.5},
            {1.0, 2.0},
            {0.0, 65535.0},
            {-1024.0, 1024.0},
            {0.0, 3.4028234663852886e38},   // ~Float.MAX_VALUE as a finite huge bound
            {-3.0, 3.0},
            {0.0, 0.30000001192092896},     // a float-derived bound
            {10.0, 20.0},
            {-50.0, -10.0},
      };

      // Probe values to sanitize, designed to land below/inside/above each range
      // plus exact boundary hits, fractional points, and large magnitudes.
      double[] probes = {
            -1.0e9, -1024.0, -100.0, -50.0, -10.0, -3.0, -2.0, -1.0,
            -0.5, -0.1, 0.0, 0.1, 0.25, 0.3, 0.5, 0.7, 0.9, 1.0,
            1.5, 2.0, 3.0, 4.0, 10.0, 16.0, 20.0, 100.0, 1000.0,
            1024.0, 2048.0, 65535.0, 1.0e9, 1.0e30,
            0.30000001192092896, 0.09999999999999998, 0.8999999999999999,
            12.345, -12.345, 1.0000000000000002, 0.9999999999999999,
      };

      for (double[] b : bounds) {
         double min = b[0];
         double max = b[1];
         // Default must satisfy min <= default <= max. Use a few legal defaults.
         double[] defaults;
         if (min == max) {
            defaults = new double[]{min};
         } else {
            double mid = min + (max - min) * 0.5;
            defaults = new double[]{min, max, mid};
         }

         for (double def : defaults) {
            Object inst = ctor.newInstance("attribute.test", def, min, max);

            // DEFAULT row: getDefaultValue must equal the constructed default.
            double gotDef = (Double) mDef.invoke(inst);
            O.println("DEFAULT\t" + h(def) + "\t" + h(gotDef));

            for (double v : probes) {
               double res = (Double) mSan.invoke(inst, v);
               O.println("SANITIZE\t" + h(min) + "\t" + h(max) + "\t"
                     + h(v) + "\t" + h(res));
            }
         }
      }
   }
}
