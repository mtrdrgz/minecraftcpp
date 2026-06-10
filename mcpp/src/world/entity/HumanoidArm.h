// 1:1 C++ port of net.minecraft.world.entity.HumanoidArm (Minecraft 26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/entity/HumanoidArm.java):
//
//   public enum HumanoidArm implements StringRepresentable {
//      LEFT(0, "left", "options.mainHand.left"),
//      RIGHT(1, "right", "options.mainHand.right");
//
//      private final int id;
//      private final String name;
//      private final Component caption;
//
//      HumanoidArm(final int id, final String name, final String translationKey) {
//         this.id = id;
//         this.name = name;
//         this.caption = Component.translatable(translationKey);
//      }
//
//      public HumanoidArm getOpposite() {
//         return switch (this) {
//            case LEFT -> RIGHT;
//            case RIGHT -> LEFT;
//         };
//      }
//
//      public Component caption() { return this.caption; }
//
//      @Override
//      public String getSerializedName() { return this.name; }
//   }
//
// Ported here (verbatim from the Java):
//   - declaration order / ordinal()  (LEFT=0, RIGHT=1)
//   - name()                          (verbatim Java constant names)
//   - getSerializedName()             (the `name` field: "left"/"right")  Java:13-14,41-43
//   - id                              (the `id` field: 0/1)               Java:13-14,24
//   - getOpposite()                   (LEFT<->RIGHT)                       Java:29-34
//
// NOT ported (component/registry-coupled, hard-absent here):
//   - caption()  — returns a net.minecraft.network.chat.Component built from
//     Component.translatable(translationKey). The translation keys are
//     "options.mainHand.left" / "options.mainHand.right" (Java:13-14), but the
//     Component type is not modelled in this port, so caption() is omitted
//     rather than faked.
//   - CODEC / BY_ID / STREAM_CODEC — codec surface (StringRepresentable.fromEnum,
//     ByIdMap.continuous, ByteBufCodecs.idMapper) is not ported here.
//
// The parity gate (HumanoidArmParityTest) checks each ordinal + name +
// serialized name + id + opposite against the REAL net.minecraft enum.

#ifndef MCPP_WORLD_ENTITY_HUMANOIDARM_H
#define MCPP_WORLD_ENTITY_HUMANOIDARM_H

#include <array>
#include <cstddef>
#include <string_view>

namespace mc {

// Underlying values are the Java ordinals (declaration order, 0-based).
// (HumanoidArm.java:13-14)
enum class HumanoidArm : int {
    LEFT = 0,
    RIGHT = 1,
};

// Java enum's implicit values() array, in declaration order.
inline constexpr std::array<HumanoidArm, 2> HUMANOID_ARM_VALUES = {
    HumanoidArm::LEFT,
    HumanoidArm::RIGHT,
};

// HumanoidArm.ordinal() — the position in the declaration order.
inline constexpr int ordinal(HumanoidArm a) { return static_cast<int>(a); }

// HumanoidArm.name() — the verbatim Java constant name.
inline constexpr std::string_view name(HumanoidArm a) {
    switch (a) {
        case HumanoidArm::LEFT:  return "LEFT";
        case HumanoidArm::RIGHT: return "RIGHT";
    }
    return ""; // unreachable for the two defined constants
}

// HumanoidArm.getSerializedName() — the `name` field (HumanoidArm.java:41-43).
// LEFT -> "left", RIGHT -> "right" (the 2nd ctor arg, Java:13-14).
inline constexpr std::string_view getSerializedName(HumanoidArm a) {
    switch (a) {
        case HumanoidArm::LEFT:  return "left";
        case HumanoidArm::RIGHT: return "right";
    }
    return ""; // unreachable
}

// HumanoidArm.id — the `id` field (the 1st ctor arg, Java:13-14,24).
// Note: the enum is private; the parity gate reflects it from the real class.
inline constexpr int humanoidArmId(HumanoidArm a) {
    switch (a) {
        case HumanoidArm::LEFT:  return 0;
        case HumanoidArm::RIGHT: return 1;
    }
    return 0; // unreachable
}

// HumanoidArm.getOpposite() — LEFT<->RIGHT (HumanoidArm.java:29-34).
inline constexpr HumanoidArm getOpposite(HumanoidArm a) {
    switch (a) {
        case HumanoidArm::LEFT:  return HumanoidArm::RIGHT;
        case HumanoidArm::RIGHT: return HumanoidArm::LEFT;
    }
    return a; // unreachable
}

} // namespace mc

#endif // MCPP_WORLD_ENTITY_HUMANOIDARM_H
