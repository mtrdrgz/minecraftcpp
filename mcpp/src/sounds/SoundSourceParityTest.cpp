// Parity test for mc::sounds::SoundSource (sounds/SoundSource.h) vs Java ground truth.
// Reads the TSV emitted by SoundSourceParity.java and compares value-for-value.
#include "sounds/SoundSource.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: SoundSourceParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <name()> <getName()>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            const std::string& getName = p[3];

            auto v = static_cast<mc::sounds::SoundSource>(ord);
            bool ok = true;
            if (name != std::string(mc::sounds::soundSourceName(v))) ok = false;
            if (getName != std::string(mc::sounds::soundSourceGetName(v))) ok = false;

            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord
                          << " name=" << name
                          << " (cpp name=" << std::string(mc::sounds::soundSourceName(v)) << ")"
                          << " getName=" << getName
                          << " (cpp getName=" << std::string(mc::sounds::soundSourceGetName(v)) << ")"
                          << "\n";
            }
        } else if (tag == "COUNT") {
            // COUNT <values().length>
            ++cases;
            int count = std::stoi(p[1]);
            if (count != mc::sounds::SOUND_SOURCE_COUNT) {
                ++mism;
                std::cerr << "COUNT mismatch java=" << count
                          << " cpp=" << mc::sounds::SOUND_SOURCE_COUNT << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "SoundSource cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
