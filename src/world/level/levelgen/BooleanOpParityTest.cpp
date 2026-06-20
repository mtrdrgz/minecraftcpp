// Parity gate for net.minecraft.world.phys.shapes.BooleanOp — verifies the C++
// port (world/phys/shapes/BooleanOp.h) reproduces every named operator's 2x2
// truth table exactly, against ground truth from the real net.minecraft class
// (tools/BooleanOpParity.java). The Java side reflects each BooleanOp lambda
// field and calls the real apply(boolean,boolean); this harness dispatches the
// same op by name and compares the boolean result BIT-for-BIT.
//
//   boolean_op_parity --cases mcpp/build/boolean_op.tsv
#include <bit>
#include <cstdint>
#include <cstdio>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

#include "world/phys/shapes/BooleanOp.h"

using namespace mc;

namespace {

// Map the Java field name to the C++ BooleanOps constant. FALSE/TRUE collide
// with Windows macros, so the port renames them ALWAYS_FALSE / ALWAYS_TRUE
// (BooleanOp.h:26,41); every other name is verbatim.
const BooleanOp& opByName(const std::string& name) {
    if (name == "FALSE") return BooleanOps::ALWAYS_FALSE;
    if (name == "NOT_OR") return BooleanOps::NOT_OR;
    if (name == "ONLY_SECOND") return BooleanOps::ONLY_SECOND;
    if (name == "NOT_FIRST") return BooleanOps::NOT_FIRST;
    if (name == "ONLY_FIRST") return BooleanOps::ONLY_FIRST;
    if (name == "NOT_SECOND") return BooleanOps::NOT_SECOND;
    if (name == "NOT_SAME") return BooleanOps::NOT_SAME;
    if (name == "NOT_AND") return BooleanOps::NOT_AND;
    if (name == "AND") return BooleanOps::AND;
    if (name == "SAME") return BooleanOps::SAME;
    if (name == "SECOND") return BooleanOps::SECOND;
    if (name == "CAUSES") return BooleanOps::CAUSES;
    if (name == "FIRST") return BooleanOps::FIRST;
    if (name == "CAUSED_BY") return BooleanOps::CAUSED_BY;
    if (name == "OR") return BooleanOps::OR;
    if (name == "TRUE") return BooleanOps::ALWAYS_TRUE;
    throw std::invalid_argument("unknown op " + name);
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/boolean_op.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::fprintf(stderr, "BooleanOp: cannot open %s\n", casesPath.c_str());
        return 2;
    }

    long cases = 0;
    long mismatches = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> t = split(line);
        if (t.empty()) continue;

        if (t[0] == "APPLY") {
            // APPLY <opName> <first> <second> <result>
            if (t.size() != 5) {
                std::fprintf(stderr, "BooleanOp: malformed APPLY row\n");
                ++mismatches;
                continue;
            }
            ++cases;
            const BooleanOp& op = opByName(t[1]);
            bool first = (t[2] == "1");
            bool second = (t[3] == "1");
            bool expected = (t[4] == "1");
            bool got = op.apply(first, second);
            // Bit-for-bit on the boolean (single bit, but keep the convention).
            if (std::bit_cast<uint8_t>(got) != std::bit_cast<uint8_t>(expected)) {
                ++mismatches;
                std::fprintf(stderr,
                             "APPLY-MISMATCH %s(%d,%d) got=%d want=%d\n",
                             t[1].c_str(), first ? 1 : 0, second ? 1 : 0,
                             got ? 1 : 0, expected ? 1 : 0);
            }
        }
        // unknown tags ignored
    }

    std::printf("BooleanOp cases=%ld mismatches=%ld\n", cases, mismatches);
    return mismatches == 0 ? 0 : 1;
}
