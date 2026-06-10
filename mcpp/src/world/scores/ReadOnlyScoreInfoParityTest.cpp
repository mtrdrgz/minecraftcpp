// Parity test for net.minecraft.world.scores.ReadOnlyScoreInfo — verifies that the
// PORTABLE subset of the interface contract (its two pure data accessors)
//
//     int value();
//     boolean isLocked();
//
// is reproduced bit-for-bit by reading them THROUGH the interface off the existing
// 1:1 C++ port of its only concrete implementation, world/scores/Score.h
// (mc::world::scores::Score, which `implements ReadOnlyScoreInfo`).
//
//   readonly_score_info_parity --cases mcpp/build/readonly_score_info.tsv
//
// ReadOnlyScoreInfo is an interface; Score.h is a concrete struct, not a virtual
// base, so to genuinely exercise the *interface* dispatch we define a tiny local
// abstract base `ReadOnlyScoreInfo` (the two pure virtuals) plus a thin adapter
// `ScoreView` that forwards to a real mc::world::scores::Score. We then read every
// expected state via a `const ReadOnlyScoreInfo&`, exactly as the Java GT reads via
// the upcast interface. (We add NO logic — value()/isLocked() come verbatim from the
// reused Score.h; this file edits no existing header.)
//
// DELIBERATELY NOT covered (NOT pure — registry/component/network/codec coupled,
// hence NOT ported; see unportedMethods in the gate):
//   - NumberFormat numberFormat()
//   - MutableComponent formatValue(NumberFormat)
//   - static MutableComponent safeFormatValue(info, fmt)
//
// TAGs (see tools/ReadOnlyScoreInfoParity.java):
//   INIT                                   <value> <locked>   fresh Score() via interface
//   SET_V  <arg>                           <value> <locked>   after value(arg), via interface
//   SET_L  <arg>                           <value> <locked>   after setLocked(arg!=0), via interface
//   REPLAY <id> <step> <op> <arg>          <value> <locked>   per-step interface-observed state
//
// value compared as decimal int32; locked compared as 0/1. REPLAY rows are stateful —
// applied in file order to the live instance for that id.

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

// Local 1:1 model of the ReadOnlyScoreInfo interface's pure surface — exactly the
// two methods of net.minecraft.world.scores.ReadOnlyScoreInfo that are portable.
// (numberFormat()/formatValue()/safeFormatValue() are Component/registry-coupled and
// intentionally omitted — see header comment.)
class ReadOnlyScoreInfo {
public:
    virtual ~ReadOnlyScoreInfo() = default;
    virtual int32_t value() const = 0;   // int value();
    virtual bool isLocked() const = 0;   // boolean isLocked();
};

// Adapter: a Score viewed AS a ReadOnlyScoreInfo, forwarding to the reused port's
// accessors verbatim (no added logic). Mirrors `class Score implements ReadOnlyScoreInfo`.
class ScoreView final : public ReadOnlyScoreInfo {
public:
    int32_t value() const override { return s.value(); }
    bool isLocked() const override { return s.isLocked(); }
    scores::Score s;  // the real reused port instance
};

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
        std::cerr << "usage: readonly_score_info_parity --cases <tsv>\n";
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

    // Compare a state read THROUGH the interface against the expected TSV fields.
    auto cmp = [&](const ReadOnlyScoreInfo& r, int32_t expVal, int expLock,
                   const std::string& line) {
        if (r.value() != expVal)
            fail(line + " value got=" + std::to_string(r.value()) +
                 " exp=" + std::to_string(expVal));
        int gotLock = r.isLocked() ? 1 : 0;
        if (gotLock != expLock)
            fail(line + " locked got=" + std::to_string(gotLock) +
                 " exp=" + std::to_string(expLock));
    };

    // Stateful REPLAY instances, keyed by sequence id.
    std::map<int, ScoreView> replayInstances;

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& t = p[0];
        ++total;

        if (t == "INIT") {
            // fields: INIT <value> <locked>
            ScoreView v;  // constructor defaults
            cmp(v, static_cast<int32_t>(std::stoll(p[1])), std::stoi(p[2]), line);
        } else if (t == "SET_V") {
            // fields: SET_V <arg> <value> <locked>
            ScoreView v;
            v.s.value(static_cast<int32_t>(std::stoll(p[1])));
            cmp(v, static_cast<int32_t>(std::stoll(p[2])), std::stoi(p[3]), line);
        } else if (t == "SET_L") {
            // fields: SET_L <arg> <value> <locked>
            ScoreView v;
            v.s.setLocked(std::stoi(p[1]) != 0);
            cmp(v, static_cast<int32_t>(std::stoll(p[2])), std::stoi(p[3]), line);
        } else if (t == "REPLAY") {
            // fields: REPLAY <id> <step> <op> <arg> <value> <locked>
            int id = std::stoi(p[1]);
            const std::string& op = p[3];
            int32_t arg = static_cast<int32_t>(std::stoll(p[4]));
            ScoreView& v = replayInstances[id];  // value-initialized -> defaults on first use
            if (op == "V") {
                v.s.value(arg);
            } else if (op == "L") {
                v.s.setLocked(arg != 0);
            } else {
                fail("UNKNOWN_OP " + op + " in " + line);
                continue;
            }
            cmp(v, static_cast<int32_t>(std::stoll(p[5])), std::stoi(p[6]), line);
        } else {
            fail("UNKNOWN_TAG " + t);
        }
    }

    std::cout << "ReadOnlyScoreInfo cases=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
