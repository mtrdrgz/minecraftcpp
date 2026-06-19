// Parity test for mcpp/src/client/renderer/EndFlashState.h — the pure, GL-free
// End-dimension flash math of net.minecraft.client.renderer.EndFlashState
// (Minecraft 26.1.2).
//
// Ground truth: tools/EndFlashStateParity.java drives the REAL EndFlashState
// through a deterministic SEQUENCE of clockTimes (reading every private field via
// reflection) and emits TICK + GETI rows. This test replays the SAME sequence on
// the C++ EndFlashState in lockstep: a single live instance is ticked exactly when
// a TICK row appears, then its state is compared to the row; the following GETI
// rows are checked against getIntensity(partialTicks) on that same live instance.
//
//   TICK <clockTime> <flashSeed> <offset> <duration> <xAngleBits> <yAngleBits>
//        <intensityBits> <oldIntensityBits> <started>
//   GETI <clockTime> <partialBits(f)> <getIntensityBits(f)>
//
// Floats compared BIT-FOR-BIT via std::bit_cast; longs/ints decimal; started 0/1.
//
//   end_flash_state_parity --cases mcpp/build/end_flash_state.tsv

#include "client/renderer/EndFlashState.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace efs = mc::client::renderer;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
float    bf(const std::string& s) { return std::bit_cast<float>(static_cast<uint32_t>(std::stoul(s, nullptr, 16))); }
uint32_t fb(float v) { return std::bit_cast<uint32_t>(v); }
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: end_flash_state_parity --cases <tsv>\n"; return 2; }

    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long cases = 0, mismatches = 0;
    std::string line;

    // One live C++ instance, ticked in lockstep with the Java ground truth.
    efs::EndFlashState state;

    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "TICK") {
            // TICK clockTime seed offset duration xAngle yAngle intensity oldIntensity started
            if (p.size() < 10) continue;
            std::int64_t clockTime = std::stoll(p[1]);
            std::int64_t expSeed     = std::stoll(p[2]);
            std::int32_t expOffset   = std::stoi(p[3]);
            std::int32_t expDuration = std::stoi(p[4]);
            uint32_t expXAngle = static_cast<uint32_t>(std::stoul(p[5], nullptr, 16));
            uint32_t expYAngle = static_cast<uint32_t>(std::stoul(p[6], nullptr, 16));
            uint32_t expInt    = static_cast<uint32_t>(std::stoul(p[7], nullptr, 16));
            uint32_t expOldInt = static_cast<uint32_t>(std::stoul(p[8], nullptr, 16));
            int expStarted = std::stoi(p[9]);

            state.tick(clockTime);

            std::int64_t gotSeed     = state.flashSeedValue();
            std::int32_t gotOffset   = state.offsetValue();
            std::int32_t gotDuration = state.durationValue();
            uint32_t gotXAngle = fb(state.getXAngle());
            uint32_t gotYAngle = fb(state.getYAngle());
            uint32_t gotInt    = fb(state.intensityValue());
            uint32_t gotOldInt = fb(state.oldIntensityValue());
            int gotStarted = state.flashStartedThisTick() ? 1 : 0;

            ++cases;
            bool ok = gotSeed == expSeed && gotOffset == expOffset && gotDuration == expDuration
                && gotXAngle == expXAngle && gotYAngle == expYAngle && gotInt == expInt
                && gotOldInt == expOldInt && gotStarted == expStarted;
            if (!ok) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "TICK t=" << clockTime
                              << " seed(e/g)=" << expSeed << "/" << gotSeed
                              << " off=" << expOffset << "/" << gotOffset
                              << " dur=" << expDuration << "/" << gotDuration
                              << " xA=" << p[5] << "/" << std::hex << gotXAngle << std::dec
                              << " yA=" << p[6] << "/" << std::hex << gotYAngle << std::dec
                              << " int=" << p[7] << "/" << std::hex << gotInt << std::dec
                              << " oInt=" << p[8] << "/" << std::hex << gotOldInt << std::dec
                              << " st=" << expStarted << "/" << gotStarted << "\n";
            }
        } else if (tag == "GETI") {
            // GETI clockTime partialBits getIntensityBits
            if (p.size() < 4) continue;
            float partial = bf(p[2]);
            uint32_t expVal = static_cast<uint32_t>(std::stoul(p[3], nullptr, 16));
            uint32_t gotVal = fb(state.getIntensity(partial));
            ++cases;
            if (gotVal != expVal) {
                ++mismatches;
                if (mismatches <= 20)
                    std::cerr << "GETI t=" << p[1] << " partial=" << p[2]
                              << " expect=" << p[3] << " got=" << std::hex << gotVal << std::dec << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "EndFlashState checks=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
