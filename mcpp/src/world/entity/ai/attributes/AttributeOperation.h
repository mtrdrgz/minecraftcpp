// Enum-metadata accessors for net.minecraft.world.entity.ai.attributes
//   .AttributeModifier.Operation (MC 26.1.2).
//
// The numeric value-combine (AttributeInstance.calculateValue) and the bare
// Operation ordinals already live in the certified header
//   world/entity/ai/attributes/AttributeMath.h
// (enum class Operation { ADD_VALUE=0, ADD_MULTIPLIED_BASE=1, ADD_MULTIPLIED_TOTAL=2 }).
// That header is reused as-is and is NOT modified. This header only adds the
// per-constant string/id accessors that AttributeMath.h does not expose, so the
// Operation enum can be gated for id()/name()/getSerializedName() parity too.
//
// Source (AttributeModifier.java:38-64):
//   public enum Operation implements StringRepresentable {
//      ADD_VALUE("add_value", 0),
//      ADD_MULTIPLIED_BASE("add_multiplied_base", 1),
//      ADD_MULTIPLIED_TOTAL("add_multiplied_total", 2);
//      private final String name;
//      private final int id;
//      Operation(final String name, final int id) { this.name = name; this.id = id; }
//      public int id() { return this.id; }
//      @Override public String getSerializedName() { return this.name; }
//   }
//
// Java's enum.name() is the declared constant identifier ("ADD_VALUE", ...);
// enum.ordinal() is the declaration index (0,1,2). For this enum id()==ordinal()
// for all three constants, but they are independent fields in the source so we
// expose id() separately rather than aliasing ordinal.
#pragma once

#include "world/entity/ai/attributes/AttributeMath.h"

namespace mc::world::entity::ai::attributes {

// AttributeModifier.Operation.id() — the explicit `id` constructor argument.
inline int operationId(Operation op) {
   switch (op) {
      case Operation::ADD_VALUE:            return 0;
      case Operation::ADD_MULTIPLIED_BASE:  return 1;
      case Operation::ADD_MULTIPLIED_TOTAL: return 2;
   }
   return -1; // unreachable for valid enum values
}

// java.lang.Enum.ordinal() — declaration index.
inline int operationOrdinal(Operation op) {
   switch (op) {
      case Operation::ADD_VALUE:            return 0;
      case Operation::ADD_MULTIPLIED_BASE:  return 1;
      case Operation::ADD_MULTIPLIED_TOTAL: return 2;
   }
   return -1;
}

// java.lang.Enum.name() — the declared constant identifier.
inline const char* operationName(Operation op) {
   switch (op) {
      case Operation::ADD_VALUE:            return "ADD_VALUE";
      case Operation::ADD_MULTIPLIED_BASE:  return "ADD_MULTIPLIED_BASE";
      case Operation::ADD_MULTIPLIED_TOTAL: return "ADD_MULTIPLIED_TOTAL";
   }
   return "";
}

// StringRepresentable.getSerializedName() — the `name` constructor argument.
inline const char* operationSerializedName(Operation op) {
   switch (op) {
      case Operation::ADD_VALUE:            return "add_value";
      case Operation::ADD_MULTIPLIED_BASE:  return "add_multiplied_base";
      case Operation::ADD_MULTIPLIED_TOTAL: return "add_multiplied_total";
   }
   return "";
}

} // namespace mc::world::entity::ai::attributes
