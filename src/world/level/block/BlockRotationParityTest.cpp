// Parity gate for net.minecraft.world.level.block.Rotation.
//
// VERIFY-EXISTING: the functional Rotation port already lives (certified) in
//   world/level/levelgen/structure/templatesystem/StructureTransforms.h
// as the enum mc::levelgen::structure::Rotation plus the free functions
//   rotationGetRotated(Rotation,Rotation), rotationRotate(Rotation,Direction),
//   rotationRotateInt(Rotation,int,int).
// This test re-includes that header (no duplication, no edits) and checks it
// against ground truth from tools/BlockRotationParity.java, which calls the
// REAL net.minecraft Rotation enum.
//
// The existing header does not carry Rotation's `id`/`index` fields, so the
// serialized-name + getIndex() values are verified here against the literal
// constants copied VERBATIM from Rotation.java:18-21 (index, id per constant).
//
//   rotation_block_parity --cases mcpp/build/rotation_block.tsv
//
// All comparisons are exact integer equality (every value here is an int/ordinal
// or a fixed string — no floats), which is bit-for-bit by construction.

#include "../levelgen/structure/templatesystem/StructureTransforms.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::Direction;
using mc::levelgen::structure::Rotation;
using mc::levelgen::structure::rotationGetRotated;
using mc::levelgen::structure::rotationRotate;
using mc::levelgen::structure::rotationRotateInt;

namespace {

std::vector<std::string> splitTabs(const std::string& line) {
    std::vector<std::string> out;
    std::string item;
    std::istringstream ss(line);
    while (std::getline(ss, item, '\t')) out.push_back(item);
    return out;
}

int toi(const std::string& s) { return std::stoi(s); }

Rotation rot(int i) { return static_cast<Rotation>(i); }
Direction dir(int i) { return static_cast<Direction>(i); }
int idx(Rotation r) { return static_cast<int>(r); }
int idx(Direction d) { return static_cast<int>(d); }

// Verbatim from Rotation.java:18-21 — { index, serialized-id } per ordinal.
struct RotMeta { int index; const char* id; };
constexpr RotMeta kRotMeta[4] = {
    {0, "none"},            // NONE
    {1, "clockwise_90"},    // CLOCKWISE_90
    {2, "180"},             // CLOCKWISE_180
    {3, "counterclockwise_90"}, // COUNTERCLOCKWISE_90
};

// Java enum constant names (Rotation.name()).
constexpr const char* kRotName[4] = {
    "NONE", "CLOCKWISE_90", "CLOCKWISE_180", "COUNTERCLOCKWISE_90",
};

} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i)
        if (std::string(argv[i]) == "--cases" && i + 1 < argc) casesPath = argv[++i];
    if (casesPath.empty()) {
        std::cerr << "usage: rotation_block_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    std::string line;

    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mism;
        if (shown++ < 20) std::cerr << "MISMATCH [" << tag << "] " << detail << "\n";
    };

    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto f = splitTabs(line);
        if (f.empty()) continue;
        const std::string& tag = f[0];
        ++total;

        if (tag == "META") {
            // META  ord  name  serializedName  index
            int ord = toi(f[1]);
            const std::string& name = f[2];
            const std::string& serialized = f[3];
            int index = toi(f[4]);
            if (ord < 0 || ord > 3) { fail(tag, "ord out of range " + f[1]); continue; }
            // Ordinal must equal the enum's underlying int value.
            if (idx(rot(ord)) != ord) fail(tag, "ordinal mismatch " + f[1]);
            if (name != kRotName[ord]) fail(tag, "name " + name + " != " + kRotName[ord]);
            if (serialized != kRotMeta[ord].id)
                fail(tag, "serialized " + serialized + " != " + kRotMeta[ord].id);
            if (index != kRotMeta[ord].index)
                fail(tag, "index " + std::to_string(index) + " != "
                              + std::to_string(kRotMeta[ord].index));
        } else if (tag == "ROTATED") {
            // ROTATED  selfOrd  rotOrd  resultOrd
            int got = idx(rotationGetRotated(rot(toi(f[1])), rot(toi(f[2]))));
            int want = toi(f[3]);
            if (got != want)
                fail(tag, "self=" + f[1] + " rot=" + f[2] + " got=" + std::to_string(got)
                              + " want=" + f[3]);
        } else if (tag == "RDIR") {
            // RDIR  selfOrd  dirOrd  resultDirOrd
            int got = idx(rotationRotate(rot(toi(f[1])), dir(toi(f[2]))));
            int want = toi(f[3]);
            if (got != want)
                fail(tag, "self=" + f[1] + " dir=" + f[2] + " got=" + std::to_string(got)
                              + " want=" + f[3]);
        } else if (tag == "RINT") {
            // RINT  selfOrd  rotation  steps  result
            int got = rotationRotateInt(rot(toi(f[1])), toi(f[2]), toi(f[3]));
            int want = toi(f[4]);
            if (got != want)
                fail(tag, "self=" + f[1] + " rotation=" + f[2] + " steps=" + f[3]
                              + " got=" + std::to_string(got) + " want=" + f[4]);
        } else {
            --total; // unknown tag — ignore, don't count
        }
    }

    std::cout << "BlockRotation cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
