// Parity test for the position long-packing codecs (BlockPos / ChunkPos /
// SectionPos). Ground truth: tools/PosCodecParity.java vs the real 26.1.2 classes.
// Integers/longs compared exactly; doubles passed as raw bits.
//
//   pos_codec_parity --cases mcpp/build/pos_codec.tsv

#include "PosCodec.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace pc = mc::poscodec;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out; std::string it; std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int       i(const std::string& s) { return std::stoi(s); }
long long ll(const std::string& s) { return std::stoll(s); }
double    bd(const std::string& s) { return std::bit_cast<double>(static_cast<uint64_t>(std::stoull(s))); }
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a) if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: pos_codec_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long total = 0, mism = 0; int shown = 0;
    auto fail = [&](const std::string& l) { ++mism; if (shown++ < 40) std::cerr << "MISMATCH " << l << "\n"; };
    auto eqI = [&](long long got, const std::string& exp, const std::string& l) { if (got != ll(exp)) { fail(l + " got=" + std::to_string(got)); } };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "BP_ASLONG")      eqI(pc::blockPosAsLong(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        else if (t == "BP_GET") {
            long long n = ll(p[1]);
            if (pc::blockPosGetX(n) != ll(p[2]) || pc::blockPosGetY(n) != ll(p[3]) || pc::blockPosGetZ(n) != ll(p[4])) fail(line);
        }
        else if (t == "BP_FLAT")   eqI(pc::blockPosGetFlatIndex(ll(p[1])), p[2], line);
        else if (t == "BP_OFFSET") eqI(pc::blockPosOffset(ll(p[1]), i(p[2]), i(p[3]), i(p[4])), p[5], line);
        else if (t == "CP_ASLONG") eqI(pc::chunkPosAsLong(i(p[1]), i(p[2])), p[3], line);
        else if (t == "CP_GET") {
            long long k = ll(p[1]);
            if (pc::chunkPosGetX(k) != ll(p[2]) || pc::chunkPosGetZ(k) != ll(p[3])) fail(line);
        }
        else if (t == "CP_CTOR") {
            long long k = ll(p[1]);
            if (pc::chunkPosGetX(k) != ll(p[2]) || pc::chunkPosGetZ(k) != ll(p[3])) fail(line);
        }
        else if (t == "SP_ASLONG") eqI(pc::sectionPosAsLong(i(p[1]), i(p[2]), i(p[3])), p[4], line);
        else if (t == "SP_GET") {
            long long n = ll(p[1]);
            if (pc::sectionPosX(n) != ll(p[2]) || pc::sectionPosY(n) != ll(p[3]) || pc::sectionPosZ(n) != ll(p[4])) fail(line);
        }
        else if (t == "B2S")       eqI(pc::blockToSectionCoord(i(p[1])), p[2], line);
        else if (t == "S2B")       eqI(pc::sectionToBlockCoord(i(p[1])), p[2], line);
        else if (t == "S2B_OFF")   eqI(pc::sectionToBlockCoord(i(p[1]), i(p[2])), p[3], line);
        else if (t == "B2S_D")     eqI(pc::blockToSectionCoord(bd(p[1])), p[2], line);
        else if (t == "SP_REL") {
            int16_t rel = pc::sectionRelativePos(i(p[1]), i(p[2]), i(p[3]));
            if (rel != static_cast<int16_t>(ll(p[4])) ||
                pc::sectionRelativeX(rel) != ll(p[5]) ||
                pc::sectionRelativeY(rel) != ll(p[6]) ||
                pc::sectionRelativeZ(rel) != ll(p[7])) fail(line);
        }
        else { fail("UNKNOWN_TAG " + t); }
    }

    std::cout << "PosCodec cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
