// Parity test for net.minecraft.world.scores.Score — verifies the C++ port in
// world/scores/Score.h reproduces the REAL Score's stateful int `value` and
// boolean `locked` (default true), driven through value(int)/setLocked(bool),
// bit-for-bit against tools/ScoreParity.java ground truth.
//
//   score_parity --cases mcpp/build/score.tsv
//
// TAGs (see ScoreParity.java):
//   INIT                                   <value> <locked>   fresh Score()
//   SET_V  <arg>                           <value> <locked>   after value(arg)
//   SET_L  <arg>                           <value> <locked>   after setLocked(arg!=0)
//   REPLAY <id> <step> <op> <arg>          <value> <locked>   per-step state of a sequence
//
// value compared as decimal int32; locked compared as 0/1. The REPLAY rows are
// stateful — they must be applied in file order to the live instance for that id.

#include "Score.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace scores = mc::world::scores;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: score_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& msg) {
        ++mism;
        if (shown++ < 30) std::cerr << "MISMATCH " << msg << "\n";
    };

    // Compare a freshly-computed (value, locked) against the expected TSV fields.
    auto cmp = [&](const scores::Score& s, int32_t expVal, int expLock, const std::string& line) {
        if (s.value() != expVal)
            fail(line + " value got=" + std::to_string(s.value()) + " exp=" + std::to_string(expVal));
        int gotLock = s.isLocked() ? 1 : 0;
        if (gotLock != expLock)
            fail(line + " locked got=" + std::to_string(gotLock) + " exp=" + std::to_string(expLock));
    };

    // Stateful REPLAY instances, keyed by sequence id.
    std::map<int, scores::Score> replayInstances;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "INIT") {
            // fields: INIT <value> <locked>
            scores::Score s;  // constructor defaults
            cmp(s, static_cast<int32_t>(std::stoll(p[1])), std::stoi(p[2]), line);
        } else if (t == "SET_V") {
            // fields: SET_V <arg> <value> <locked>
            scores::Score s;
            s.value(static_cast<int32_t>(std::stoll(p[1])));
            cmp(s, static_cast<int32_t>(std::stoll(p[2])), std::stoi(p[3]), line);
        } else if (t == "SET_L") {
            // fields: SET_L <arg> <value> <locked>
            scores::Score s;
            s.setLocked(std::stoi(p[1]) != 0);
            cmp(s, static_cast<int32_t>(std::stoll(p[2])), std::stoi(p[3]), line);
        } else if (t == "REPLAY") {
            // fields: REPLAY <id> <step> <op> <arg> <value> <locked>
            int id = std::stoi(p[1]);
            const std::string& op = p[3];
            int32_t arg = static_cast<int32_t>(std::stoll(p[4]));
            scores::Score& s = replayInstances[id];  // value-initialized -> defaults on first use
            if (op == "V") {
                s.value(arg);
            } else if (op == "L") {
                s.setLocked(arg != 0);
            } else {
                fail("UNKNOWN_OP " + op + " in " + line);
                continue;
            }
            cmp(s, static_cast<int32_t>(std::stoll(p[5])), std::stoi(p[6]), line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "Score cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
