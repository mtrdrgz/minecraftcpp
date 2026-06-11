// Parity test for net.minecraft.util.SequencedPriorityIterator<T>.
// Ground truth: tools/SequencedPriorityIteratorParity.java vs the real class. The TSV is
// a stream of ADD / NEXT ops tagged by sequence id; we keep one iterator per seq and
// replay the SAME ops in order, comparing the observable result of every NEXT (hasNext
// flag + yielded value) exactly. Payloads are Integers, so values compare as decimal.
//
//   sequenced_priority_iterator_parity --cases mcpp/build/sequenced_priority_iterator.tsv
//
// A mismatch on a NEXT row means the C++ priority/FIFO ordering, the highestPrio cache
// movement, the signed/wrapping comparisons, or the Guava-AbstractIterator DONE latch
// diverged from real Minecraft — a real port bug, never to be papered over.

#include "util/SequencedPriorityIterator.h"

#include <iostream>
#include <fstream>
#include <map>
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
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: sequenced_priority_iterator_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    // One persistent iterator per sequence id.
    std::map<int, mc::util::SequencedPriorityIterator<int>> instances;

    long long total = 0, mism = 0;
    int shown = 0;
    auto fail = [&](const std::string& l, const std::string& why) {
        ++mism;
        if (shown++ < 40) std::cerr << "MISMATCH " << why << " | " << l << "\n";
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        ++total;

        if (t == "ADD") {
            // ADD seq value priority
            int seq = std::stoi(p[1]);
            int value = std::stoi(p[2]);
            int priority = std::stoi(p[3]);  // may be INT32_MIN/MAX; stoi handles full range
            instances[seq].add(value, priority);
        } else if (t == "NEXT") {
            // NEXT seq hasNext value
            int seq = std::stoi(p[1]);
            int expHas = std::stoi(p[2]);
            long long expVal = std::stoll(p[3]);
            auto r = instances[seq].nextOrEnd();
            int gotHas = r.has_value() ? 1 : 0;
            if (gotHas != expHas) {
                fail(line, std::string("hasNext got=") + std::to_string(gotHas));
            } else if (gotHas == 1 && static_cast<long long>(*r) != expVal) {
                fail(line, std::string("value got=") + std::to_string(*r));
            }
        } else {
            fail(line, "UNKNOWN_TAG " + t);
        }
    }

    std::cout << "SequencedPriorityIterator checks=" << total << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
