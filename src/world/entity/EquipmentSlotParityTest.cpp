// Bit-exact parity gate for net.minecraft.world.entity.EquipmentSlot
// (Minecraft 26.1.2), ported in world/entity/EquipmentSlotData.h. Reads the TSV
// emitted by mcpp/tools/EquipmentSlotParity.java and compares values exactly.
//
// Tags:
//   DATA      <ord> <typeOrd> <index> <id> <isArmor> <canXp> <name> <serName>
//   INDEXBASE <ord> <base>   <getIndex(base)>
//   FILTERBIT <ord> <offset> <getFilterBit(offset)>
//   BYID      <id>  <resulting ord>
//   BYNAME    <name> <resulting ord>
#include "world/entity/EquipmentSlotData.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace eq = mc::eqslot;

static std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

static eq::Slot slotFromOrd(int o) { return static_cast<eq::Slot>(o); }

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++cases;

        if (tag == "DATA") {
            // <ord> <typeOrd> <index> <id> <isArmor> <canXp> <name> <serName>
            eq::Slot s = slotFromOrd(std::stoi(p[1]));
            int expTypeOrd = std::stoi(p[2]);
            int expIndex   = std::stoi(p[3]);
            int expId      = std::stoi(p[4]);
            int expArmor   = std::stoi(p[5]);
            int expXp      = std::stoi(p[6]);
            const std::string& expName    = p[7];
            const std::string& expSerName = p[8];

            int gotTypeOrd = static_cast<int>(eq::getType(s));
            int gotIndex   = eq::getIndex(s);
            int gotId      = eq::getId(s);
            int gotArmor   = eq::isArmor(s) ? 1 : 0;
            int gotXp      = eq::canIncreaseExperience(s) ? 1 : 0;
            std::string gotName(eq::getName(s));

            if (gotTypeOrd != expTypeOrd) ++mism;
            else if (gotIndex != expIndex) ++mism;
            else if (gotId != expId) ++mism;
            else if (gotArmor != expArmor) ++mism;
            else if (gotXp != expXp) ++mism;
            else if (gotName != expName) ++mism;
            else if (gotName != expSerName) ++mism; // getSerializedName == getName
        } else if (tag == "INDEXBASE") {
            // <ord> <base> <getIndex(base)>
            eq::Slot s = slotFromOrd(std::stoi(p[1]));
            int base = static_cast<int>(std::stoll(p[2]));
            int exp  = static_cast<int>(std::stoll(p[3]));
            if (eq::getIndex(s, base) != exp) ++mism;
        } else if (tag == "FILTERBIT") {
            // <ord> <offset> <getFilterBit(offset)>
            eq::Slot s = slotFromOrd(std::stoi(p[1]));
            int off = static_cast<int>(std::stoll(p[2]));
            int exp = static_cast<int>(std::stoll(p[3]));
            if (eq::getFilterBit(s, off) != exp) ++mism;
        } else if (tag == "BYID") {
            // <id> <resulting ord>
            int id  = static_cast<int>(std::stoll(p[1]));
            int exp = std::stoi(p[2]);
            int got = static_cast<int>(eq::byId(id));
            if (got != exp) ++mism;
        } else if (tag == "BYNAME") {
            // <name> <resulting ord>
            const std::string& name = p[1];
            int exp = std::stoi(p[2]);
            int got = static_cast<int>(eq::byName(name));
            if (got != exp) ++mism;
        } else {
            // Unknown tag — do not silently pass; count as a mismatch.
            ++mism;
        }
    }

    std::cout << "EquipmentSlot cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
