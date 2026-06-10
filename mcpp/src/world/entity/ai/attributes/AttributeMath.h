// 1:1 port of net.minecraft.world.entity.ai.attributes.AttributeInstance.calculateValue
// plus RangedAttribute.sanitizeValue / Mth.clamp(double,double,double).
//
// Source (26.1.2):
//   AttributeInstance.java:
//     private double calculateValue() {
//        double base = this.getBaseValue();
//        for (mod : ADD_VALUE)            base   += modifier.amount();
//        double result = base;
//        for (mod : ADD_MULTIPLIED_BASE)  result += base * modifier.amount();
//        for (mod : ADD_MULTIPLIED_TOTAL) result *= 1.0 + modifier.amount();
//        return this.attribute.value().sanitizeValue(result);
//     }
//   RangedAttribute.java:
//     public double sanitizeValue(final double value) {
//        return Double.isNaN(value) ? this.minValue : Mth.clamp(value, this.minValue, this.maxValue);
//     }
//   Mth.java:
//     public static double clamp(final double value, final double min, final double max) {
//        return value < min ? min : Math.min(value, max);
//     }
//
// Pure double; no registry lookup. base/min/max and the per-operation modifier
// amount lists are supplied directly. The amount lists MUST be supplied in the
// exact order Java iterates each operation's modifier map (the GT tool emits them
// in that observed order) so the floating-point accumulation is bit-identical.
//
// AttributeModifier.Operation ordinals (AttributeModifier.java):
//   ADD_VALUE = 0, ADD_MULTIPLIED_BASE = 1, ADD_MULTIPLIED_TOTAL = 2.
#pragma once

#include <cmath>
#include <vector>

namespace mc::world::entity::ai::attributes {

enum class Operation : int {
   ADD_VALUE = 0,
   ADD_MULTIPLIED_BASE = 1,
   ADD_MULTIPLIED_TOTAL = 2,
};

// Faithful port of java.lang.Math.min(double, double).
// Differs from std::fmin: returns NaN if either arg is NaN, and treats -0.0 < +0.0.
inline double javaMathMin(double a, double b) {
   if (a != a) {
      return a; // a is NaN
   }
   if (b != b) {
      return b; // b is NaN
   }
   if (a == 0.0 && b == 0.0) {
      // Both zero: Math.min returns the negative zero if either is negative zero.
      // Compare via 1/x sign: -0.0 -> -inf, +0.0 -> +inf.
      return (1.0 / a < 1.0 / b) ? a : b;
   }
   return (a <= b) ? a : b;
}

// Mth.clamp(double value, double min, double max):  value < min ? min : Math.min(value, max)
inline double mthClamp(double value, double min, double max) {
   return value < min ? min : javaMathMin(value, max);
}

// RangedAttribute.sanitizeValue(double value):
//   Double.isNaN(value) ? minValue : Mth.clamp(value, minValue, maxValue)
inline double sanitizeValue(double value, double minValue, double maxValue) {
   return std::isnan(value) ? minValue : mthClamp(value, minValue, maxValue);
}

// AttributeInstance.calculateValue, reproduced body.
// addValue / addMulBase / addMulTotal are the per-operation modifier amounts,
// each already in Java's map-iteration order.
inline double calculateValue(double baseValue,
                             double minValue,
                             double maxValue,
                             const std::vector<double>& addValue,
                             const std::vector<double>& addMulBase,
                             const std::vector<double>& addMulTotal) {
   double base = baseValue;

   for (double amount : addValue) {
      base += amount;
   }

   double result = base;

   for (double amount : addMulBase) {
      result += base * amount;
   }

   for (double amount : addMulTotal) {
      result *= 1.0 + amount;
   }

   return sanitizeValue(result, minValue, maxValue);
}

} // namespace mc::world::entity::ai::attributes
