// Parity test for the value<->name handling of IntegerProperty / BooleanProperty
// / EnumProperty. Ground truth: tools/BlockStatePropsParity.java.
//
// getName outputs are STRINGS compared by exact equality; getValue/parse and
// getInternalIndex outputs are ints/ordinals/bools compared exactly.
//
//   block_state_props_parity --cases mcpp/build/block_state_props.tsv

#include "PropertyValues.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

using mc::block::state::properties::BooleanProperty;
using mc::block::state::properties::EnumProperty;
using mc::block::state::properties::IntegerProperty;

namespace {
// Tab-split that PRESERVES empty fields (including a single trailing empty field),
// so getValue inputs like "" / " " round-trip exactly.
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
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: block_state_props_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    // First pass: collect META rows so probe rows can rebuild the exact objects.
    // We hold the file in memory (small) and process in two passes.
    std::vector<std::string> lines;
    { std::string line; while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (!line.empty()) lines.push_back(line);
    } }

    // Reconstructed properties.
    std::map<int, IntegerProperty> intProps;
    std::map<int, EnumProperty> enumProps;
    BooleanProperty boolProp = BooleanProperty::create("powered");

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& line, const std::string& why) {
        ++mism; if (shown++ < 40) std::cerr << "MISMATCH(" << why << ") " << line << "\n";
    };

    // Pass 1: build properties from META rows.
    for (const auto& line : lines) {
        auto p = split(line);
        if (p[0] == "IMETA") {
            int idx = toI(p[1]); int mn = toI(p[2]); int mx = toI(p[3]);
            intProps.emplace(idx, IntegerProperty::create("p" + std::to_string(idx), mn, mx));
        } else if (p[0] == "EMETA") {
            // EMETA <ei> <numConst> [<ord> <serName>]*numConst <selCount> [<selOrd>]*selCount
            int ei = toI(p[1]);
            int numConst = toI(p[2]);
            std::vector<std::string> allNames(static_cast<std::size_t>(numConst));
            std::size_t pos = 3;
            for (int k = 0; k < numConst; ++k) {
                int ord = toI(p[pos]); std::string nm = p[pos + 1]; pos += 2;
                allNames[static_cast<std::size_t>(ord)] = nm;
            }
            int selCount = toI(p[pos]); ++pos;
            std::vector<int32_t> sel(static_cast<std::size_t>(selCount));
            for (int k = 0; k < selCount; ++k) sel[static_cast<std::size_t>(k)] = toI(p[pos++]);
            enumProps.emplace(ei, EnumProperty::create("e" + std::to_string(ei), allNames, sel));
        }
    }

    // Pass 2: verify every probe row.
    for (const auto& line : lines) {
        auto p = split(line);
        const std::string& tag = p[0];

        if (tag == "IMETA" || tag == "EMETA") continue;  // already consumed

        ++total;

        if (tag == "IVALUES") {
            const IntegerProperty& prop = intProps.at(toI(p[1]));
            int count = toI(p[2]);
            const auto& vals = prop.getPossibleValues();
            bool ok = (static_cast<int>(vals.size()) == count);
            for (int k = 0; ok && k < count; ++k) ok = (vals[static_cast<std::size_t>(k)] == toI(p[3 + k]));
            if (!ok) fail(line, "IVALUES");
        } else if (tag == "IGETNAME") {
            const IntegerProperty& prop = intProps.at(toI(p[1]));
            std::string got = prop.getName(toI(p[2]));
            if (got != p[3]) fail(line, "IGETNAME got=" + got);
        } else if (tag == "IGETIDX") {
            const IntegerProperty& prop = intProps.at(toI(p[1]));
            if (prop.getInternalIndex(toI(p[2])) != toI(p[3])) fail(line, "IGETIDX");
        } else if (tag == "IGETVALUE") {
            const IntegerProperty& prop = intProps.at(toI(p[1]));
            auto r = prop.getValue(p[2]);
            int present = r.has_value() ? 1 : 0;
            int val = r.has_value() ? *r : 0;
            if (present != toI(p[3]) || val != toI(p[4])) fail(line, "IGETVALUE");

        } else if (tag == "BVALUES") {
            int count = toI(p[1]);
            const auto& vals = boolProp.getPossibleValues();
            bool ok = (static_cast<int>(vals.size()) == count);
            for (int k = 0; ok && k < count; ++k) ok = ((vals[static_cast<std::size_t>(k)] ? 1 : 0) == toI(p[2 + k]));
            if (!ok) fail(line, "BVALUES");
        } else if (tag == "BGETNAME") {
            bool b = toI(p[1]) != 0;
            if (boolProp.getName(b) != p[2]) fail(line, "BGETNAME");
        } else if (tag == "BGETIDX") {
            bool b = toI(p[1]) != 0;
            if (boolProp.getInternalIndex(b) != toI(p[2])) fail(line, "BGETIDX");
        } else if (tag == "BGETVALUE") {
            auto r = boolProp.getValue(p[1]);
            int present = r.has_value() ? 1 : 0;
            int val = (r.has_value() && *r) ? 1 : 0;
            if (present != toI(p[2]) || val != toI(p[3])) fail(line, "BGETVALUE");

        } else if (tag == "EVALUES") {
            const EnumProperty& prop = enumProps.at(toI(p[1]));
            int count = toI(p[2]);
            const auto& vals = prop.getPossibleValues();
            bool ok = (static_cast<int>(vals.size()) == count);
            for (int k = 0; ok && k < count; ++k) ok = (vals[static_cast<std::size_t>(k)] == toI(p[3 + k]));
            if (!ok) fail(line, "EVALUES");
        } else if (tag == "EGETNAME") {
            const EnumProperty& prop = enumProps.at(toI(p[1]));
            std::string got = prop.getName(toI(p[2]));
            if (got != p[3]) fail(line, "EGETNAME got=" + got);
        } else if (tag == "EGETIDX") {
            const EnumProperty& prop = enumProps.at(toI(p[1]));
            if (prop.getInternalIndex(toI(p[2])) != toI(p[3])) fail(line, "EGETIDX");
        } else if (tag == "EGETVALUE") {
            const EnumProperty& prop = enumProps.at(toI(p[1]));
            auto r = prop.getValue(p[2]);
            int present = r.has_value() ? 1 : 0;
            int ord = r.has_value() ? *r : -1;
            if (present != toI(p[3]) || ord != toI(p[4])) fail(line, "EGETVALUE");

        } else {
            fail(line, "UNKNOWN_TAG");
        }
    }

    std::cout << "BlockStateProps cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
