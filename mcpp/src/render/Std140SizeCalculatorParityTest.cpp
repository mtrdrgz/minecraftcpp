// Bit-exact parity gate for com.mojang.blaze3d.buffers.Std140SizeCalculator
// (Minecraft 26.1.2). Reads the TSV emitted by tools/Std140SizeCalculatorParity.java
// and re-drives the C++ render/Std140SizeCalculator.h builder op-by-op for each
// scenario, comparing every running get() size exactly (decimal int32).
//
// TSV rows:
//   SEQ  <n>  <op_0> <size_0>  <op_1> <size_1> ... <op_{n-1}> <size_{n-1}>
//   where <op_k> is "putFloat"/.../"putMat4f" or "align:<N>", and <size_k> is the
//   builder's get() immediately after op_k.
//
//   mcpp/build/std140_size_calculator_parity.exe --cases mcpp/build/std140_size_calculator.tsv

#include "render/Std140SizeCalculator.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::render::Std140SizeCalculator;

namespace {

// Apply one op token to the builder; returns false if the token is unknown.
bool applyOp(Std140SizeCalculator& c, const std::string& op) {
    if (op.rfind("align:", 0) == 0) {
        int32_t a = static_cast<int32_t>(std::stoll(op.substr(6)));
        c.align(a);
        return true;
    }
    if (op == "putFloat") { c.putFloat(); return true; }
    if (op == "putInt")   { c.putInt();   return true; }
    if (op == "putVec2")  { c.putVec2();  return true; }
    if (op == "putIVec2") { c.putIVec2(); return true; }
    if (op == "putVec3")  { c.putVec3();  return true; }
    if (op == "putIVec3") { c.putIVec3(); return true; }
    if (op == "putVec4")  { c.putVec4();  return true; }
    if (op == "putIVec4") { c.putIVec4(); return true; }
    if (op == "putMat4f") { c.putMat4f(); return true; }
    return false;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: std140_size_calculator_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long checks = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];
        if (tag != "SEQ") continue;  // forward-compatible: ignore unknown tags
        if (p.size() < 2) continue;

        long n = std::stol(p[1]);
        if ((long)p.size() != 2 + 2 * n) {
            std::cerr << "MALFORMED row n=" << n << " cols=" << p.size() << "\n";
            ++checks;
            ++mism;
            continue;
        }

        Std140SizeCalculator c;
        for (long k = 0; k < n; ++k) {
            ++checks;
            const std::string& op = p[2 + 2 * k];
            int32_t expected = static_cast<int32_t>(std::stoll(p[2 + 2 * k + 1]));
            if (!applyOp(c, op)) {
                ++mism;
                if (mism <= 20) std::cerr << "UNKNOWN op=" << op << "\n";
                continue;
            }
            int32_t got = c.get();
            if (got != expected) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "MISMATCH step=" << k << " op=" << op
                              << " exp=" << expected << " got=" << got << "\n";
                }
            }
        }
    }

    std::cout << "Std140SizeCalculator checks=" << checks << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
