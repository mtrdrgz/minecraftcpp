// BlockStateEnums2.h — 1:1 C++ port of the per-constant ordinal +
// getSerializedName() tables of a second family of StringRepresentable enums
// under net.minecraft.world.level.block.state.properties.
//
// Strict reverse-engineering of Minecraft Java Edition 26.1.2. Every constant,
// its declaration ORDER (== ordinal()) and its serialized-name STRING below is
// taken VERBATIM from the decompiled Java:
//   26.1.2/src/net/minecraft/world/level/block/state/properties/BambooLeaves.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/BellAttachType.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/DripstoneThickness.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/CreakingHeartState.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/SculkSensorPhase.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/SideChainPart.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/StructureMode.java
//   26.1.2/src/net/minecraft/world/level/block/state/properties/TestBlockMode.java
//
// Each enum implements StringRepresentable; getSerializedName() returns the
// per-constant `name` string set via the constructor. The ordinal is the
// zero-based declaration index.
//
// These are pure string tables: no registries, world, Codec or Bootstrap.
//
// Scope of this gate: ordinal -> getSerializedName() for every constant of every
// listed enum. The other (behavioural) methods are listed below for the record
// but are out of scope for this name/ordinal gate:
//   SideChainPart.isConnected()/isConnectionTowards()/isChainEnd()/
//     whenConnectedToTheRight()/whenConnectedToTheLeft()/
//     whenDisconnectedFromTheRight()/whenDisconnectedFromTheLeft();
//   StructureMode.getDisplayName() (Component — needs network/i18n);
//   TestBlockMode.getDisplayName()/getDetailedMessage() (Component);
//   <enum>.toString() (delegates to getSerializedName or `name`).
// TestBlockMode also carries an explicit per-constant int id (0,1,2,3) which
// equals its ordinal for every constant here.

#ifndef MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_BLOCKSTATEENUMS2_H
#define MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_BLOCKSTATEENUMS2_H

#include <cstddef>
#include <string>
#include <vector>

namespace mc::block::state::properties {

// One entry per StringRepresentable enum: the serialized names in ordinal order.
// `serializedNames[ordinal]` == that constant's getSerializedName().
struct BlockStateEnum2Desc {
    std::string javaName;                       // simple class name, for keying
    std::vector<std::string> serializedNames;   // index == ordinal()
};

// ---------------------------------------------------------------------------
// BambooLeaves.java: NONE("none"), SMALL("small"), LARGE("large")
inline const BlockStateEnum2Desc kBambooLeaves{
    "BambooLeaves", {"none", "small", "large"}};

// BellAttachType.java:
//   FLOOR("floor"), CEILING("ceiling"), SINGLE_WALL("single_wall"),
//   DOUBLE_WALL("double_wall")
inline const BlockStateEnum2Desc kBellAttachType{
    "BellAttachType", {"floor", "ceiling", "single_wall", "double_wall"}};

// DripstoneThickness.java:
//   TIP_MERGE("tip_merge"), TIP("tip"), FRUSTUM("frustum"), MIDDLE("middle"),
//   BASE("base")
inline const BlockStateEnum2Desc kDripstoneThickness{
    "DripstoneThickness", {"tip_merge", "tip", "frustum", "middle", "base"}};

// CreakingHeartState.java:
//   UPROOTED("uprooted"), DORMANT("dormant"), AWAKE("awake")
inline const BlockStateEnum2Desc kCreakingHeartState{
    "CreakingHeartState", {"uprooted", "dormant", "awake"}};

// SculkSensorPhase.java:
//   INACTIVE("inactive"), ACTIVE("active"), COOLDOWN("cooldown")
inline const BlockStateEnum2Desc kSculkSensorPhase{
    "SculkSensorPhase", {"inactive", "active", "cooldown"}};

// SideChainPart.java:
//   UNCONNECTED("unconnected"), RIGHT("right"), CENTER("center"), LEFT("left")
inline const BlockStateEnum2Desc kSideChainPart{
    "SideChainPart", {"unconnected", "right", "center", "left"}};

// StructureMode.java:
//   SAVE("save"), LOAD("load"), CORNER("corner"), DATA("data")
inline const BlockStateEnum2Desc kStructureMode{
    "StructureMode", {"save", "load", "corner", "data"}};

// TestBlockMode.java:
//   START(0,"start"), LOG(1,"log"), FAIL(2,"fail"), ACCEPT(3,"accept")
inline const BlockStateEnum2Desc kTestBlockMode{
    "TestBlockMode", {"start", "log", "fail", "accept"}};

// ---------------------------------------------------------------------------
// Lookup of all gated enums by their Java simple class name. The order here is
// irrelevant to correctness (rows are keyed by name), but mirrors the GT tool.
inline const std::vector<const BlockStateEnum2Desc*>& allBlockStateEnums2() {
    static const std::vector<const BlockStateEnum2Desc*> kAll{
        &kBambooLeaves, &kBellAttachType, &kDripstoneThickness,
        &kCreakingHeartState, &kSculkSensorPhase, &kSideChainPart,
        &kStructureMode, &kTestBlockMode};
    return kAll;
}

// getSerializedName2(javaName, ordinal): empty string if not found / out of range.
inline std::string getSerializedName2(const std::string& javaName, int ordinal) {
    for (const BlockStateEnum2Desc* d : allBlockStateEnums2()) {
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

#endif  // MCPP_WORLD_LEVEL_BLOCK_STATE_PROPERTIES_BLOCKSTATEENUMS2_H
