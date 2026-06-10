// Parity gate for ClientboundContainerSetContentPacket.STREAM_CODEC over the EMPTY +
// NO-COMPONENT ItemStack domain.
//
// Wire order (ClientboundContainerSetContentPacket.java:13-23, verbatim from src):
//   VarInt(containerId)            CONTAINER_ID = FriendlyByteBuf.writeContainerId = VarInt
//   VarInt(stateId)               ByteBufCodecs.VAR_INT
//   VarInt(listSize) + elements   OPTIONAL_LIST_STREAM_CODEC = collection(OPTIONAL_STREAM_CODEC):
//                                   writeCount = VarInt(size), then each element below
//   <element>                     carriedItem via OPTIONAL_STREAM_CODEC
// ItemStack OPTIONAL_STREAM_CODEC element (ItemStack.java:185-193; DataComponentPatch.java:106-109):
//   empty stack        -> VarInt(0)
//   no-component stack  -> VarInt(count) VarInt(itemId) VarInt(0) VarInt(0)
//   itemId = Item.STREAM_CODEC = holderRegistry(ITEM) = plain VarInt(getId), resolved here
//   name -> id through mc::net::NetworkRegistries (gated by network_registries_parity), NOT
//   hard-coded. Component-bearing stacks need the per-component patch codecs (a later wave) and
//   are NOT in this gate.
//
//   tools/run_groundtruth.ps1 -Tool PktContainerSetContentParity -Out mcpp/build/pkt_container_set_content.tsv
//   pkt_container_set_content_parity --cases mcpp/build/pkt_container_set_content.tsv [--asset <path>]
//
// Row: ENC \t containerId \t stateId \t listCount \t perItemSpec \t carriedSpec \t readableBytes \t wireHex
//   perItemSpec = ';'-joined "name|count|empty" (or "" when list empty); carriedSpec = "name|count|empty"

#include "../../PacketBuffer.h"
#include "../../NetworkRegistries.h"

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

struct Spec { std::string name; int count = 0; bool empty = true; };

Spec parseSpec(const std::string& s) {
    // "name|count|empty"
    Spec out;
    std::string field;
    std::stringstream ss(s);
    std::getline(ss, out.name, '|');
    std::getline(ss, field, '|'); out.count = field.empty() ? 0 : std::stoi(field);
    std::getline(ss, field, '|'); out.empty = (field == "1");
    return out;
}

// Append one ItemStack element in OPTIONAL_STREAM_CODEC framing. Returns false (and flags
// missCount) on a registry miss so the caller can count the case as a mismatch — an unresolved
// item id is a hard failure, never silently skipped.
bool writeStack(mc::net::PacketBuffer& buf, const mc::net::NetworkRegistries& reg,
                const Spec& sp, long& missCount, int& shown) {
    if (sp.empty) {
        buf.writeVarInt(0);
        return true;
    }
    auto id = reg.id("minecraft:item", sp.name);
    if (!id) {
        ++missCount;
        if (shown++ < 25) std::cerr << "REGISTRY-MISS minecraft:item " << sp.name << "\n";
        return false;
    }
    buf.writeVarInt(sp.count);  // ItemStack count
    buf.writeVarInt(*id);       // Item.STREAM_CODEC holderRegistry -> plain VarInt(getId)
    buf.writeVarInt(0);         // DataComponentPatch: added count = 0
    buf.writeVarInt(0);         // DataComponentPatch: removed count = 0
    return true;
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
        std::string tag, cidStr, stateStr, listCountStr, perItem, carried, readableStr, wireHex;
        std::getline(ss, tag, '\t');
        if (tag != "ENC") continue;
        std::getline(ss, cidStr, '\t');
        std::getline(ss, stateStr, '\t');
        std::getline(ss, listCountStr, '\t');
        std::getline(ss, perItem, '\t');
        std::getline(ss, carried, '\t');
        std::getline(ss, readableStr, '\t');
        std::getline(ss, wireHex, '\t');
        ++cases;

        int containerId = std::stoi(cidStr);
        int stateId = std::stoi(stateStr);
        int listCount = std::stoi(listCountStr);
        size_t wantReadable = (size_t)std::stoul(readableStr);

        // Parse the list element specs (';'-joined). Empty list -> no elements.
        std::vector<Spec> elems;
        if (!perItem.empty()) {
            std::stringstream ps(perItem);
            std::string e;
            while (std::getline(ps, e, ';')) elems.push_back(parseSpec(e));
        }

        mc::net::PacketBuffer buf;
        bool missed = false;  // a registry miss already counted itself inside writeStack
        // 1) containerId — CONTAINER_ID = plain VarInt
        buf.writeVarInt(containerId);
        // 2) stateId — VAR_INT
        buf.writeVarInt(stateId);
        // 3) items — collection: VarInt(size) then each element via OPTIONAL_STREAM_CODEC
        buf.writeVarInt((int)elems.size());
        for (const auto& sp : elems)
            if (!writeStack(buf, reg, sp, mism, shown)) missed = true;
        // 4) carriedItem — OPTIONAL_STREAM_CODEC
        if (!writeStack(buf, reg, parseSpec(carried), mism, shown)) missed = true;

        // A registry miss already incremented mism inside writeStack (and left bytes short);
        // skip the byte compare in that case to avoid double-counting.
        if (missed) continue;
        std::string got = hex(buf.data());
        if ((int)elems.size() != listCount || got != wireHex || buf.data().size() != wantReadable) {
            ++mism;
            if (shown++ < 25)
                std::cerr << "MISMATCH containerId=" << containerId << " stateId=" << stateId
                          << " listCount=" << listCount << " parsedElems=" << elems.size()
                          << "\n  got  " << got << " (" << buf.data().size() << "B)\n  want "
                          << wireHex << " (" << wantReadable << "B)\n";
        }
    }
    std::cout << "PktContainerSetContent cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
