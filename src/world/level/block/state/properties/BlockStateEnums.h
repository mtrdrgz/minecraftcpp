// BlockStateEnums.h — 1:1 C++ port of the per-constant ordinal + getSerializedName()
// tables of the StringRepresentable enums under
//   net.minecraft.world.level.block.state.properties.
//
// Strict reverse-engineering of Minecraft Java Edition 26.1.2. Every constant,
// its declaration ORDER (== ordinal()) and its serialized-name STRING below is
// taken VERBATIM from the decompiled Java:
//   26.1.2/src/net/minecraft/world/level/block/state/properties/AttachFace.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/Half.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/SlabType.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/StairsShape.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/BedPart.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/ChestType.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/ComparatorMode.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/WallSide.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/DoorHingeSide.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/PistonType.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/RedstoneSide.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/RailShape.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/DoubleBlockHalf.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/Tilt.java
//
// Each enum implements StringRepresentable; getSerializedName() returns the
// per-constant `name` string (for AttachFace/Half/SlabType/StairsShape/BedPart/
// ChestType/ComparatorMode/WallSide/PistonType/RedstoneSide/RailShape/Tilt set
// via the ctor; for DoorHingeSide/DoubleBlockHalf returned by an inline
// `this == X ? "..." : "..."`). The ordinal is the zero-based declaration index.
//
// These are pure string tables: no registries, world, Codec or Bootstrap.
//
// Scope of this gate: ordinal -> getSerializedName() for every constant of every
// listed enum. The other (behavioural) methods are listed below for the record
// but are out of scope for this name/ordinal gate (each is a trivial total
// function over these same constants):
//   ChestType.getOpposite(); RailShape.isSlope()/getName(); RedstoneSide.isConnected();
//   DoubleBlockHalf.getDirectionToOther()/getOtherHalf(); Tilt.causesVibration();
//   <enum>.toString() (delegates to getSerializedName or `name`).

#ifndef MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_BLOCKSTATEENUMS_H
#define MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_BLOCKSTATEENUMS_H

#include <cstddef>
#include <string>
#include <vector>

namespace mc::block::state::properties {

// One entry per StringRepresentable enum: the serialized names in ordinal order.
// `serializedNames[ordinal]` == that constant's getSerializedName().
struct BlockStateEnumDesc {
    std::string javaName;                       // simple class name, for keying
    std::vector<std::string> serializedNames;   // index == ordinal()
};

// ---------------------------------------------------------------------------
// AttachFace.java: FLOOR("floor"), WALL("wall"), CEILING("ceiling")
inline const BlockStateEnumDesc kAttachFace{
    "AttachFace", {"floor", "wall", "ceiling"}};

// Half.java: TOP("top"), BOTTOM("bottom")
inline const BlockStateEnumDesc kHalf{
    "Half", {"top", "bottom"}};

// SlabType.java: TOP("top"), BOTTOM("bottom"), DOUBLE("double")
inline const BlockStateEnumDesc kSlabType{
    "SlabType", {"top", "bottom", "double"}};

// StairsShape.java: STRAIGHT, INNER_LEFT, INNER_RIGHT, OUTER_LEFT, OUTER_RIGHT
inline const BlockStateEnumDesc kStairsShape{
    "StairsShape",
    {"straight", "inner_left", "inner_right", "outer_left", "outer_right"}};

// BedPart.java: HEAD("head"), FOOT("foot")
inline const BlockStateEnumDesc kBedPart{
    "BedPart", {"head", "foot"}};

// ChestType.java: SINGLE("single"), LEFT("left"), RIGHT("right")
inline const BlockStateEnumDesc kChestType{
    "ChestType", {"single", "left", "right"}};

// ComparatorMode.java: COMPARE("compare"), SUBTRACT("subtract")
inline const BlockStateEnumDesc kComparatorMode{
    "ComparatorMode", {"compare", "subtract"}};

// WallSide.java: NONE("none"), LOW("low"), TALL("tall")
inline const BlockStateEnumDesc kWallSide{
    "WallSide", {"none", "low", "tall"}};

// DoorHingeSide.java: LEFT, RIGHT;  getSerializedName() = this==LEFT?"left":"right"
inline const BlockStateEnumDesc kDoorHingeSide{
    "DoorHingeSide", {"left", "right"}};

// PistonType.java: DEFAULT("normal"), STICKY("sticky")
inline const BlockStateEnumDesc kPistonType{
    "PistonType", {"normal", "sticky"}};

// RedstoneSide.java: UP("up"), SIDE("side"), NONE("none")
inline const BlockStateEnumDesc kRedstoneSide{
    "RedstoneSide", {"up", "side", "none"}};

// RailShape.java: NORTH_SOUTH, EAST_WEST, ASCENDING_EAST, ASCENDING_WEST,
//                 ASCENDING_NORTH, ASCENDING_SOUTH, SOUTH_EAST, SOUTH_WEST,
//                 NORTH_WEST, NORTH_EAST
inline const BlockStateEnumDesc kRailShape{
    "RailShape",
    {"north_south", "east_west", "ascending_east", "ascending_west",
     "ascending_north", "ascending_south", "south_east", "south_west",
     "north_west", "north_east"}};

// DoubleBlockHalf.java: UPPER, LOWER;  getSerializedName()=this==UPPER?"upper":"lower"
inline const BlockStateEnumDesc kDoubleBlockHalf{
    "DoubleBlockHalf", {"upper", "lower"}};

// Tilt.java: NONE("none"), UNSTABLE("unstable"), PARTIAL("partial"), FULL("full")
inline const BlockStateEnumDesc kTilt{
    "Tilt", {"none", "unstable", "partial", "full"}};

// ---------------------------------------------------------------------------
// Lookup of all gated enums by their Java simple class name. The order here is
// irrelevant to correctness (rows are keyed by name), but mirrors the GT tool.
inline const std::vector<const BlockStateEnumDesc*>& allBlockStateEnums() {
    static const std::vector<const BlockStateEnumDesc*> kAll{
        &kAttachFace, &kHalf, &kSlabType, &kStairsShape, &kBedPart,
        &kChestType, &kComparatorMode, &kWallSide, &kDoorHingeSide,
        &kPistonType, &kRedstoneSide, &kRailShape, &kDoubleBlockHalf, &kTilt};
    return kAll;
}

// getSerializedName(javaName, ordinal): empty string if not found / out of range.
inline std::string getSerializedName(const std::string& javaName, int ordinal) {
    for (const BlockStateEnumDesc* d : allBlockStateEnums()) {
        if (d->javaName == javaName) {
            if (ordinal < 0 ||
                static_cast<std::size_t>(ordinal) >= d->serializedNames.size()) {
                return std::string();
            }
            return d->serializedNames[static_cast<std::size_t>(ordinal)];
        }
    }
    return std::string();
}

}  // namespace mc::block::state::properties

#endif  // MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_BLOCKSTATEENUMS_H
