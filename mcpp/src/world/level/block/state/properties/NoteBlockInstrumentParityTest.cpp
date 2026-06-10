// Parity test for mc::block::state::properties::NoteBlockInstrument
// (world/level/block/state/properties/NoteBlockInstrument.h) vs Java ground truth.
//
// Reads the TSV emitted by NoteBlockInstrumentParity.java and compares
// value-for-value (ordinal -> serializedName / isTunable / hasCustomSound /
// worksAboveNoteBlock). Names are STRINGS compared by exact equality; the
// predicates are bools compared exactly.
//
//   note_block_instrument_parity --cases mcpp/build/note_block_instrument.tsv

#include "NoteBlockInstrument.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::block::state::properties::NoteBlockInstrument;

namespace {
std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { out.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    out.push_back(cur);
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: note_block_instrument_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    int shown = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // CRLF safety
        if (line.empty()) continue;
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <serializedName> <isTunable> <hasCustomSound> <worksAboveNoteBlock>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            int tunable = std::stoi(p[3]);
            int customSound = std::stoi(p[4]);
            int worksAbove = std::stoi(p[5]);

            auto v = static_cast<NoteBlockInstrument>(ord);
            bool ok = true;
            if (name != std::string(mc::block::state::properties::noteBlockInstrumentSerializedName(v))) ok = false;
            if ((mc::block::state::properties::noteBlockInstrumentIsTunable(v) ? 1 : 0) != tunable) ok = false;
            if ((mc::block::state::properties::noteBlockInstrumentHasCustomSound(v) ? 1 : 0) != customSound) ok = false;
            if ((mc::block::state::properties::noteBlockInstrumentWorksAboveNoteBlock(v) ? 1 : 0) != worksAbove) ok = false;

            if (!ok) {
                ++mism;
                if (shown++ < 40)
                    std::cerr << "CONST mismatch ord=" << ord << " name=" << name
                              << " tunable=" << tunable << " customSound=" << customSound
                              << " worksAbove=" << worksAbove << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "NoteBlockInstrument cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
