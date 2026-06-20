// Parity gate for DataComponentPatch on the ItemStack wire, VAR_INT integer-component family
// (damage/max_damage/max_stack_size/repair_cost). Certifies the patch framing
// (DataComponentPatch.java:106-141) + the VAR_INT component value codec + DATA_COMPONENT_TYPE
// id resolution. Every GT row is a single-added-component (non-default) patch:
//   ItemStack = VarInt(count) VarInt(itemId)              [ItemStack.OPTIONAL_STREAM_CODEC]
//   patch     = VarInt(1)positive VarInt(0)negative
//               VarInt(componentTypeId) VarInt(value)     [DataComponentType.STREAM_CODEC=registry id, then VAR_INT]
// itemId and componentTypeId both resolved through mc::net::NetworkRegistries (minecraft:item
// and minecraft:data_component_type), never hard-coded.
//
//   tools/run_groundtruth.ps1 -Tool DataComponentPatchParity -Out mcpp/build/dcp_int.tsv
//   dcp_int_parity --cases mcpp/build/dcp_int.tsv [--asset <path>]
//
// Row: ENC \t <itemName> \t <count> \t <componentName> \t <intValue> \t <readableBytes> \t <wireHex>

#include "PacketBuffer.h"
#include "NetworkRegistries.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s; s.reserve(v.size() * 2);
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath, assetPath = "src/assets/network_registries.tsv";
    for (int i = 1; i + 1 < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases") casesPath = argv[++i];
        else if (a == "--asset") assetPath = argv[++i];
    }
    if (casesPath.empty()) { std::cerr << "usage: --cases <tsv> [--asset <path>]\n"; return 2; }

    mc::net::NetworkRegistries reg;
    if (!reg.loadFromFile(assetPath)) { std::cerr << "cannot open asset " << assetPath << "\n"; return 2; }

    std::ifstream f(casesPath);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long cases = 0, mism = 0; int shown = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::stringstream ss(line);
        std::string tag, itemName, countStr, compName, valStr, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, itemName, '\t');
        std::getline(ss, countStr, '\t');
        std::getline(ss, compName, '\t');
        std::getline(ss, valStr, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;
        int count = std::stoi(countStr);
        int value = std::stoi(valStr);
        size_t wantReadable = (size_t)std::stoul(readableStr);

        auto itemId = reg.id("minecraft:item", itemName);
        auto compId = reg.id("minecraft:data_component_type", compName);
        if (!itemId || !compId) {
            ++mism;
            if (shown++ < 25) std::cerr << "REGISTRY-MISS item=" << itemName << "(" << (itemId?"ok":"miss")
                                        << ") comp=" << compName << "(" << (compId?"ok":"miss") << ")\n";
            continue;
        }

        mc::net::PacketBuffer buf;
        buf.writeVarInt(count);     // ItemStack count
        buf.writeVarInt(*itemId);   // item holder id
        buf.writeVarInt(1);         // DataComponentPatch positiveCount (one added component)
        buf.writeVarInt(0);         // negativeCount
        buf.writeVarInt(*compId);   // DataComponentType.STREAM_CODEC = registry(DATA_COMPONENT_TYPE) plain id
        buf.writeVarInt(value);     // VAR_INT component value

        std::string got = hex(buf.data());
        if (got != wireHex || buf.data().size() != wantReadable) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH " << itemName << " " << compName << "=" << value
                          << "\n  got  " << got << "\n  want " << wireHex << "\n";
        }
    }
    std::cout << "DataComponentPatchInt cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
