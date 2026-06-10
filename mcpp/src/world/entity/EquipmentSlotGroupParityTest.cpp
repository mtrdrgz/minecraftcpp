// Bit-exact parity gate for net.minecraft.world.entity.EquipmentSlotGroup
// (+ the EquipmentSlot facts its predicates consume). Reads ground-truth rows
// from tools/EquipmentSlotGroupParity.java and recomputes with the C++ port
// (world/entity/EquipmentSlotGroup.h), comparing every field exactly.
//
// Row formats (tab-separated):
//   SLOT   <ordinal> <name> <typeOrdinal> <typeName> <id> <serializedName> <isArmor>
//   GROUP  <ordinal> <name> <id> <serializedName>
//   TEST   <groupName> <slotName> <result0or1>
//   SLOTS  <groupName> <count> <slotName0> <slotName1> ...
//   BYID   <queryId> <resultGroupName>
//   BYSLOT <slotName> <resultGroupName>
//
// Run:  mcpp/build/equipment_slot_group_parity.exe --cases mcpp/build/equipment_slot_group.tsv

#include "world/entity/EquipmentSlotGroup.h"

#include <cstdint>
#include <cstdio>
#include <fstream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

using namespace mc::entity;

static std::vector<std::string> split_tabs(const std::string& line) {
   std::vector<std::string> out;
   std::string cur;
   std::istringstream ss(line);
   while (std::getline(ss, cur, '\t')) {
      out.push_back(cur);
   }
   return out;
}

// Java EquipmentSlot.name() -> C++ enum (declaration-order names).
static std::optional<EquipmentSlot> slotByJavaName(const std::string& n) {
   if (n == "MAINHAND") return EquipmentSlot::MAINHAND;
   if (n == "OFFHAND")  return EquipmentSlot::OFFHAND;
   if (n == "FEET")     return EquipmentSlot::FEET;
   if (n == "LEGS")     return EquipmentSlot::LEGS;
   if (n == "CHEST")    return EquipmentSlot::CHEST;
   if (n == "HEAD")     return EquipmentSlot::HEAD;
   if (n == "BODY")     return EquipmentSlot::BODY;
   if (n == "SADDLE")   return EquipmentSlot::SADDLE;
   return std::nullopt;
}

// Java EquipmentSlotGroup.name() -> C++ enum.
static std::optional<EquipmentSlotGroup> groupByJavaName(const std::string& n) {
   if (n == "ANY")      return EquipmentSlotGroup::ANY;
   if (n == "MAINHAND") return EquipmentSlotGroup::MAINHAND;
   if (n == "OFFHAND")  return EquipmentSlotGroup::OFFHAND;
   if (n == "HAND")     return EquipmentSlotGroup::HAND;
   if (n == "FEET")     return EquipmentSlotGroup::FEET;
   if (n == "LEGS")     return EquipmentSlotGroup::LEGS;
   if (n == "CHEST")    return EquipmentSlotGroup::CHEST;
   if (n == "HEAD")     return EquipmentSlotGroup::HEAD;
   if (n == "ARMOR")    return EquipmentSlotGroup::ARMOR;
   if (n == "BODY")     return EquipmentSlotGroup::BODY;
   if (n == "SADDLE")   return EquipmentSlotGroup::SADDLE;
   return std::nullopt;
}

// Java EquipmentSlot.Type.name() -> C++ enum.
static std::optional<EquipmentSlotType> typeByJavaName(const std::string& n) {
   if (n == "HAND")           return EquipmentSlotType::HAND;
   if (n == "HUMANOID_ARMOR") return EquipmentSlotType::HUMANOID_ARMOR;
   if (n == "ANIMAL_ARMOR")   return EquipmentSlotType::ANIMAL_ARMOR;
   if (n == "SADDLE")         return EquipmentSlotType::SADDLE;
   return std::nullopt;
}

int main(int argc, char** argv) {
   std::string casesPath;
   for (int i = 1; i < argc; ++i) {
      std::string a = argv[i];
      if (a == "--cases" && i + 1 < argc) {
         casesPath = argv[++i];
      }
   }
   if (casesPath.empty()) {
      std::fprintf(stderr, "usage: %s --cases <tsv>\n", argv[0]);
      return 2;
   }

   std::ifstream in(casesPath);
   if (!in) {
      std::fprintf(stderr, "cannot open %s\n", casesPath.c_str());
      return 2;
   }

   long cases = 0;
   long mismatches = 0;
   std::string line;
   while (std::getline(in, line)) {
      if (!line.empty() && line.back() == '\r') {
         line.pop_back();
      }
      if (line.empty()) {
         continue;
      }
      std::vector<std::string> f = split_tabs(line);
      if (f.empty()) {
         continue;
      }

      if (f[0] == "SLOT") {
         // SLOT ordinal name typeOrdinal typeName id serializedName isArmor
         int ordinal = std::stoi(f[1]);
         const std::string& name = f[2];
         int typeOrdinal = std::stoi(f[3]);
         const std::string& typeName = f[4];
         int id = std::stoi(f[5]);
         const std::string& serName = f[6];
         int isArmor = std::stoi(f[7]);

         auto slot = slotByJavaName(name);
         auto expType = typeByJavaName(typeName);
         ++cases;
         bool bad = !slot || !expType;
         if (!bad) {
            int gotOrdinal = static_cast<int>(*slot);
            EquipmentSlotType gotType = slotType(*slot);
            bad = gotOrdinal != ordinal ||
                  gotType != *expType ||
                  static_cast<int>(gotType) != typeOrdinal ||
                  slotId(*slot) != id ||
                  slotName(*slot) != serName ||
                  (slotIsArmor(*slot) ? 1 : 0) != isArmor;
         }
         if (bad) {
            ++mismatches;
            if (mismatches <= 30) {
               std::fprintf(stderr, "MISMATCH SLOT name=%s ordinal=%d type=%s id=%d ser=%s armor=%d\n",
                            name.c_str(), ordinal, typeName.c_str(), id, serName.c_str(), isArmor);
            }
         }
      } else if (f[0] == "GROUP") {
         // GROUP ordinal name id serializedName
         int ordinal = std::stoi(f[1]);
         const std::string& name = f[2];
         int id = std::stoi(f[3]);
         const std::string& serName = f[4];

         auto g = groupByJavaName(name);
         ++cases;
         bool bad = !g;
         if (!bad) {
            bad = static_cast<int>(*g) != ordinal ||
                  groupId(*g) != id ||
                  groupKey(*g) != serName;
         }
         if (bad) {
            ++mismatches;
            if (mismatches <= 30) {
               std::fprintf(stderr, "MISMATCH GROUP name=%s ordinal=%d id=%d ser=%s\n",
                            name.c_str(), ordinal, id, serName.c_str());
            }
         }
      } else if (f[0] == "TEST") {
         // TEST groupName slotName result
         auto g = groupByJavaName(f[1]);
         auto s = slotByJavaName(f[2]);
         int expected = std::stoi(f[3]);
         ++cases;
         bool bad = !g || !s;
         if (!bad) {
            bad = (groupTest(*g, *s) ? 1 : 0) != expected;
         }
         if (bad) {
            ++mismatches;
            if (mismatches <= 30) {
               std::fprintf(stderr, "MISMATCH TEST group=%s slot=%s expected=%d got=%d\n",
                            f[1].c_str(), f[2].c_str(), expected,
                            (g && s) ? (groupTest(*g, *s) ? 1 : 0) : -1);
            }
         }
      } else if (f[0] == "SLOTS") {
         // SLOTS groupName count slot0 slot1 ...
         auto g = groupByJavaName(f[1]);
         int count = std::stoi(f[2]);
         ++cases;
         bool bad = !g;
         std::vector<std::string> expSlots;
         for (int i = 0; i < count; ++i) {
            size_t idx = static_cast<size_t>(3 + i);
            if (idx < f.size()) {
               expSlots.push_back(f[idx]);
            } else {
               bad = true;
            }
         }
         if (!bad) {
            // Recompute slots(): VALUES filtered by predicate, in order.
            std::vector<std::string> got;
            for (const auto& si : EQUIPMENT_SLOTS) {
               if (groupTest(*g, si.slot)) {
                  // Map back to the Java name() for comparison.
                  // Java name() == C++ enum constant name; SLOTS rows carry name().
                  // We compare against the SLOT-name list emitted by GT, so build
                  // the same name from a fixed table keyed by ordinal.
                  static const char* kSlotNames[8] = {
                     "MAINHAND", "OFFHAND", "FEET", "LEGS",
                     "CHEST", "HEAD", "BODY", "SADDLE"};
                  got.push_back(kSlotNames[static_cast<int>(si.slot)]);
               }
            }
            bad = static_cast<int>(got.size()) != count;
            if (!bad) {
               for (int i = 0; i < count; ++i) {
                  if (got[static_cast<size_t>(i)] != expSlots[static_cast<size_t>(i)]) {
                     bad = true;
                     break;
                  }
               }
            }
         }
         if (bad) {
            ++mismatches;
            if (mismatches <= 30) {
               std::fprintf(stderr, "MISMATCH SLOTS group=%s count=%d\n", f[1].c_str(), count);
            }
         }
      } else if (f[0] == "BYID") {
         // BYID queryId resultGroupName
         // Use long long then narrow to int (Java int domain; MIN/MAX included).
         long long qll = std::stoll(f[1]);
         int q = static_cast<int>(qll);
         auto exp = groupByJavaName(f[2]);
         ++cases;
         bool bad = !exp;
         if (!bad) {
            bad = groupById(q) != *exp;
         }
         if (bad) {
            ++mismatches;
            if (mismatches <= 30) {
               std::fprintf(stderr, "MISMATCH BYID id=%d expected=%s\n", q, f[2].c_str());
            }
         }
      } else if (f[0] == "BYSLOT") {
         // BYSLOT slotName resultGroupName
         auto s = slotByJavaName(f[1]);
         auto exp = groupByJavaName(f[2]);
         ++cases;
         bool bad = !s || !exp;
         if (!bad) {
            bad = groupBySlot(*s) != *exp;
         }
         if (bad) {
            ++mismatches;
            if (mismatches <= 30) {
               std::fprintf(stderr, "MISMATCH BYSLOT slot=%s expected=%s\n",
                            f[1].c_str(), f[2].c_str());
            }
         }
      }
   }

   std::printf("EquipmentSlotGroup cases=%ld mismatches=%ld\n", cases, mismatches);
   return mismatches == 0 ? 0 : 1;
}
