// Parity test for net.minecraft.world.level.block.NoteBlock.getPitchFromNote
// (26.1.2). Ground truth: tools/NoteBlockPitchParity.java.
//
//   note_block_pitch_parity --cases <noteblock_pitch.tsv>
//
// Each row carries an integer note and the expected pitch as 8-hex raw int bits
// (Float.floatToRawIntBits). The C++ side recomputes getPitchFromNote(note) and
// compares the EXACT float bit pattern (std::bit_cast) — no tolerance.
//
// Row tag:
//   PITCH  <note>  <pitch8>

#include "world/level/block/NoteBlockPitch.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
uint32_t rawBits(float f) { return std::bit_cast<uint32_t>(f); }
uint32_t parseHex(const std::string& s) {
    return static_cast<uint32_t>(std::stoul(s, nullptr, 16));
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: note_block_pitch_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& tag = p[0];
        ++total;

        if (tag == "PITCH") {
            // PITCH <note> <pitch8>
            if (p.size() != 3) { fail("BADROW " + line); continue; }
            int note = std::stoi(p[1]);
            uint32_t want = parseHex(p[2]);
            uint32_t got = rawBits(mc::block_noteblock::getPitchFromNote(note));
            if (got != want) {
                std::ostringstream os;
                os << line << "  (got=" << std::hex << got << ")";
                fail(os.str());
            }
        } else {
            fail("UNKNOWN_TAG " + tag);
        }
    }

    std::cout << "NoteBlockPitch checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
