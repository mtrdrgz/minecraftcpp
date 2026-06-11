// Parity gate for DataComponentPatch over three more component-value families: Component
// (custom_name/item_name -> plain-text root StringTag), enum (rarity -> VarInt(id)), and Unit
// (unbreakable -> zero value bytes). Each GT row is a single-added-component patch on
// diamond_sword (count 1). The C++ resolves item + component-type ids via NetworkRegistries
// and writes the patch + the per-family value codec, byte-matching the real ItemStack codec.
//
//   tools/run_groundtruth.ps1 -Tool DataComponentPatchMiscParity -Out mcpp/build/dcp_misc.tsv
//   dcp_misc_parity --cases mcpp/build/dcp_misc.tsv [--asset <path>]
//
// Row: ENC \t <itemName> \t <componentName> \t <valueKind> \t <valueData> \t <readable> \t <wireHex>

#include "PacketBuffer.h"
#include "NetworkRegistries.h"
#include "../nbt/Tag.h"

#include <bit>
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
std::string fromHexBytes(const std::string& h) {
    std::string out; out.reserve(h.size() / 2);
    for (size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back((char)(uint8_t)std::stoul(h.substr(i, 2), nullptr, 16));
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath, assetPath = "mcpp/src/assets/network_registries.tsv";
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
        std::string tag, itemName, compName, kind, valueData, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, itemName, '\t');
        std::getline(ss, compName, '\t');
        std::getline(ss, kind, '\t');
        std::getline(ss, valueData, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;
        size_t wantReadable = (size_t)std::stoul(readableStr);

        auto itemId = reg.id("minecraft:item", itemName);
        auto compId = reg.id("minecraft:data_component_type", compName);
        if (!itemId || !compId) {
            ++mism;
            if (shown++ < 25) std::cerr << "REGISTRY-MISS item=" << itemName << " comp=" << compName << "\n";
            continue;
        }

        mc::net::PacketBuffer buf;
        buf.writeVarInt(1);          // ItemStack count (diamond_sword x1)
        buf.writeVarInt(*itemId);    // item holder id
        buf.writeVarInt(1);          // patch positiveCount
        buf.writeVarInt(0);          // patch negativeCount
        buf.writeVarInt(*compId);    // DataComponentType id
        // per-family value codec:
        if (kind == "component") {
            buf.writeNbt(mc::nbt::NbtTag::string_(fromHexBytes(valueData)));  // ComponentSerialization plain-text -> StringTag
        } else if (kind == "enumId") {
            buf.writeVarInt(std::stoi(valueData));                            // idMapper VarInt(id)
        } else if (kind == "unit") {
            /* Unit.STREAM_CODEC writes nothing */
        } else if (kind == "identifier") {
            buf.writeString(fromHexBytes(valueData));                         // Identifier.STREAM_CODEC = STRING_UTF8
        } else if (kind == "bool") {
            buf.writeBool(valueData == "1");                                  // ByteBufCodecs.BOOL
        } else if (kind == "lore") {
            // ItemLore.STREAM_CODEC = ComponentSerialization.STREAM_CODEC.apply(list(256)).map(lines).
            // valueData = "<count>:<lineHex1>:<lineHex2>:..." (each line's UTF-8 text).
            std::vector<std::string> parts;
            { std::stringstream ps(valueData); std::string it; while (std::getline(ps, it, ':')) parts.push_back(it); }
            int cnt = parts.empty() ? 0 : std::stoi(parts[0]);
            buf.writeVarInt(cnt);                                            // ByteBufCodecs.list size
            for (int li = 0; li < cnt; ++li)
                buf.writeNbt(mc::nbt::NbtTag::string_(fromHexBytes(parts[1 + li])));  // each Component plain-text NBT
        } else if (kind == "customdata") {
            // CustomData.STREAM_CODEC = ByteBufCodecs.COMPOUND_TAG -> writeNbt(compound) (unnamed root).
            // valueData = "<type s/i/b/l/f>:<nameHex>:<valuePart>" — SINGLE entry (no HashMap-order ambiguity).
            std::vector<std::string> parts;
            { std::stringstream ps(valueData); std::string it; while (std::getline(ps, it, ':')) parts.push_back(it); }
            std::string ty = parts[0], name = fromHexBytes(parts[1]), vp = parts.size() > 2 ? parts[2] : "";
            mc::nbt::NbtCompound c;
            if (ty == "s") c.put(name, mc::nbt::NbtTag::string_(fromHexBytes(vp)));
            else if (ty == "i") c.put(name, mc::nbt::NbtTag::int_(std::stoi(vp)));
            else if (ty == "b") c.put(name, mc::nbt::NbtTag::byte_(static_cast<int8_t>(std::stoi(vp))));
            else if (ty == "l") c.put(name, mc::nbt::NbtTag::long_(std::stoll(vp)));
            else if (ty == "f") c.put(name, mc::nbt::NbtTag::float_(std::bit_cast<float>(static_cast<uint32_t>(std::stoul(vp, nullptr, 16)))));
            buf.writeNbt(mc::nbt::NbtTag::compound(c));
        } else {
            ++mism;
            if (shown++ < 25) std::cerr << "UNKNOWN-KIND " << kind << "\n";
            continue;
        }

        std::string got = hex(buf.data());
        if (got != wireHex || buf.data().size() != wantReadable) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH " << compName << " kind=" << kind << " val=" << valueData
                          << "\n  got  " << got << "\n  want " << wireHex << "\n";
        }
    }
    std::cout << "DataComponentPatchMisc cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
