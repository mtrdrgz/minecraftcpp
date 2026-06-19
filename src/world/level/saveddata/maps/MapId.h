// 1:1 C++ port of net.minecraft.world.level.saveddata.maps.MapId (26.1.2).
//
// Java source (26.1.2/src/net/minecraft/world/level/saveddata/maps/MapId.java):
//   public record MapId(int id) implements TooltipProvider {
//      public static final Codec<MapId> CODEC = Codec.INT.xmap(MapId::new, MapId::id);
//      public static final StreamCodec<ByteBuf, MapId> STREAM_CODEC =
//          ByteBufCodecs.VAR_INT.map(MapId::new, MapId::id);
//      private static final Component LOCKED_TEXT = ...;
//      public String key() { return "maps/" + this.id; }
//      @Override public void addToTooltip(...) { ... }
//   }
//
// This is a trivial single-int record wrapper. The value-typed, dependency-free surface
// is ported here and certified bit-exact against the REAL net.minecraft.MapId via
// tools/MapIdParity.java + MapIdParityTest.cpp:
//
//   * id()                  -- the record accessor (the single int component)
//   * equals(MapId)         -- record-generated equals: compares the int id only
//   * hashCode()            -- record-generated hashCode. The JDK record bootstrap
//                              (java.lang.runtime.ObjectMethods.makeHashCode) folds each
//                              component with  result = result * 31 + comp.hashCode(),
//                              starting from result = 0. For the single Integer component
//                              this is  0 * 31 + Integer.hashCode(id) == id. So
//                              hashCode() == id exactly. (Certified against real GT.)
//   * key()                 -- returns the string "maps/" + id (Java int decimal form,
//                              which std::to_string matches for the full 32-bit range).
//
// NOT PORTED (out of scope for a pure value gate -- registry / component / world /
// network / serialization coupled, listed as unported):
//   * CODEC / STREAM_CODEC  -- Codec & StreamCodec serialization infrastructure.
//   * addToTooltip(...)     -- needs Item.TooltipContext.mapData(this), MapItemSavedData,
//                              DataComponents (CUSTOM_NAME / MAP_POST_PROCESSING),
//                              Component.translatable, ChatFormatting, TooltipFlag -- all
//                              live registry / component / chat objects.
#ifndef MCPP_WORLD_LEVEL_SAVEDDATA_MAPS_MAPID_H
#define MCPP_WORLD_LEVEL_SAVEDDATA_MAPS_MAPID_H

#include <cstdint>
#include <string>

namespace mc::world::level::saveddata::maps {

// public record MapId(int id)
class MapId {
public:
    explicit MapId(int32_t id) : id_(id) {}

    // public int id()  -- the record component accessor.
    int32_t id() const { return id_; }

    // Record-generated equals: two MapId are equal iff their int id components are equal.
    bool equals(const MapId& other) const { return id_ == other.id_; }
    bool operator==(const MapId& other) const { return id_ == other.id_; }
    bool operator!=(const MapId& other) const { return id_ != other.id_; }

    // Record-generated hashCode. ObjectMethods.makeHashCode folds the single Integer
    // component as  0 * 31 + Integer.hashCode(id) == id. Returned as int32_t to mirror
    // Java's signed int hashCode contract exactly.
    int32_t hashCode() const { return id_; }

    // public String key() { return "maps/" + this.id; }
    // Java string-concatenates the int via its decimal form; std::to_string matches for
    // the entire signed 32-bit range (including INT_MIN -> "-2147483648").
    std::string key() const { return "maps/" + std::to_string(id_); }

private:
    int32_t id_;
};

}  // namespace mc::world::level::saveddata::maps

#endif  // MCPP_WORLD_LEVEL_SAVEDDATA_MAPS_MAPID_H
