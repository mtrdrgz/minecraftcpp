// Parity test for the DATA surface of net.minecraft.world.scores.DisplaySlot
// (Minecraft 26.1.2).
//
// Ground truth: mcpp/tools/DisplaySlotParity.java (calls the REAL net.minecraft
// DisplaySlot / ChatFormatting). This test reconstructs the same facts from the
// ported table in world/scores/DisplaySlot.h and compares them exactly:
//   SLOT  - ordinal(), id(), name(), getSerializedName() per constant
//   COUNT - values().length
//   COLOR - teamColorToSlot(ChatFormatting) for all 22 formats (null -> -1/'-')
//   BYID  - BY_ID continuous(ZERO) lookup over in/out-of-range ids
//   CODEC - CODEC.byName(String) presence + resolved constant name
//
//   display_slot_parity --cases mcpp/build/display_slot.tsv

#include "world/scores/DisplaySlot.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::scores::byId;
using mc::scores::byName;
using mc::scores::ChatFormatting;
using mc::scores::DISPLAY_SLOT_COUNT;
using mc::scores::DisplaySlot;
using mc::scores::id;
using mc::scores::name;
using mc::scores::ordinal;
using mc::scores::teamColorToSlot;

namespace {

// Tab-split preserving empty fields (so CODEC input "" round-trips exactly).
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}

int32_t toI(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Maps a ChatFormatting Java name() to the ported enum. Returns false if the
// name is unknown (should never happen for the 22 real constants).
bool formatByName(const std::string& n, ChatFormatting& out) {
    struct E { const char* n; ChatFormatting v; };
    static const E table[] = {
        {"BLACK", ChatFormatting::BLACK},
        {"DARK_BLUE", ChatFormatting::DARK_BLUE},
        {"DARK_GREEN", ChatFormatting::DARK_GREEN},
        {"DARK_AQUA", ChatFormatting::DARK_AQUA},
        {"DARK_RED", ChatFormatting::DARK_RED},
        {"DARK_PURPLE", ChatFormatting::DARK_PURPLE},
        {"GOLD", ChatFormatting::GOLD},
        {"GRAY", ChatFormatting::GRAY},
        {"DARK_GRAY", ChatFormatting::DARK_GRAY},
        {"BLUE", ChatFormatting::BLUE},
        {"GREEN", ChatFormatting::GREEN},
        {"AQUA", ChatFormatting::AQUA},
        {"RED", ChatFormatting::RED},
        {"LIGHT_PURPLE", ChatFormatting::LIGHT_PURPLE},
        {"YELLOW", ChatFormatting::YELLOW},
        {"WHITE", ChatFormatting::WHITE},
        {"OBFUSCATED", ChatFormatting::OBFUSCATED},
        {"BOLD", ChatFormatting::BOLD},
        {"STRIKETHROUGH", ChatFormatting::STRIKETHROUGH},
        {"UNDERLINE", ChatFormatting::UNDERLINE},
        {"ITALIC", ChatFormatting::ITALIC},
        {"RESET", ChatFormatting::RESET},
    };
    for (const auto& e : table) {
        if (n == e.n) { out = e.v; return true; }
    }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: display_slot_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& line, const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << line << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "SLOT") {
            ++total;
            // SLOT ordinal id name serialized
            int32_t ord = toI(p[1]);
            if (ord < 0 || ord >= DISPLAY_SLOT_COUNT) { fail(line, "SLOT ord range"); continue; }
            DisplaySlot s = static_cast<DisplaySlot>(ord);
            if (ordinal(s) != ord) { fail(line, "SLOT ordinal"); continue; }
            if (id(s) != toI(p[2])) { fail(line, "SLOT id"); continue; }
            if (std::string(name(s)) != p[3]) { fail(line, "SLOT name"); continue; }
            if (std::string(mc::scores::getSerializedName(s)) != p[4]) { fail(line, "SLOT serialized"); continue; }
        } else if (tag == "COUNT") {
            ++total;
            if (toI(p[1]) != DISPLAY_SLOT_COUNT) fail(line, "COUNT");
        } else if (tag == "COLOR") {
            ++total;
            // COLOR colorOrdinal colorName slotOrdinalOrMinus1 slotNameOrDash
            ChatFormatting c;
            if (!formatByName(p[2], c)) { fail(line, "COLOR unknown format " + p[2]); continue; }
            auto slot = teamColorToSlot(c);
            int32_t expOrd = toI(p[3]);
            const std::string& expName = p[4];
            if (expOrd < 0) {
                // Java emitted null -> -1 / "-"
                if (slot.has_value()) { fail(line, "COLOR expected null"); continue; }
                if (expName != "-") { fail(line, "COLOR null name"); continue; }
            } else {
                if (!slot.has_value()) { fail(line, "COLOR expected slot"); continue; }
                if (ordinal(*slot) != expOrd) { fail(line, "COLOR ordinal"); continue; }
                if (std::string(name(*slot)) != expName) { fail(line, "COLOR name"); continue; }
            }
        } else if (tag == "BYID") {
            ++total;
            // BYID query resultOrdinal resultName
            int32_t q = toI(p[1]);
            DisplaySlot r = byId(q);
            if (ordinal(r) != toI(p[2])) { fail(line, "BYID ordinal"); continue; }
            if (std::string(name(r)) != p[3]) { fail(line, "BYID name"); continue; }
        } else if (tag == "CODEC") {
            ++total;
            // CODEC input present resolvedName
            const std::string& input = p[1];
            int present = toI(p[2]);
            const std::string& resolved = p[3];
            auto r = byName(input);
            int gotPresent = r.has_value() ? 1 : 0;
            std::string gotResolved = r.has_value() ? std::string(name(*r)) : "-";
            if (gotPresent != present || gotResolved != resolved)
                fail(line, "CODEC present=" + std::to_string(gotPresent) + " resolved=" + gotResolved);
        } else {
            ++total;
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "DisplaySlot cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
