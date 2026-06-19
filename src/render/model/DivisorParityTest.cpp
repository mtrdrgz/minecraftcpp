// Bit-exact parity gate for com.mojang.math.Divisor (Minecraft 26.1.2).
// Reads the TSV emitted by tools/DivisorParity.java and re-drives the C++
// render/model/Divisor.h iterator for each (numerator, denominator), comparing
// the entire produced int sequence value-for-value (exact, decimal ints).
//
// TSV rows:
//   SEQ  <numerator>  <denominator>  <count>  <v0> <v1> ... <v_{count-1}>
//
//   mcpp/build/divisor_parity.exe --cases mcpp/build/divisor.tsv

#include "render/model/Divisor.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

using mc::render::model::Divisor;

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: divisor_parity --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        std::vector<std::string> p;
        std::stringstream ss(line);
        std::string tok;
        while (std::getline(ss, tok, '\t')) p.push_back(tok);
        if (p.empty()) continue;

        const std::string& tag = p[0];
        if (tag != "SEQ") continue; // forward-compatible: ignore unknown tags

        // p[1]=numerator p[2]=denominator p[3]=count p[4..]=expected values
        if (p.size() < 4) continue;
        int32_t numerator = (int32_t)std::stoll(p[1]);
        int32_t denominator = (int32_t)std::stoll(p[2]);
        long count = std::stol(p[3]);
        if ((long)p.size() != 4 + count) {
            // Malformed row: column count must equal header + count values.
            std::cerr << "MALFORMED row num=" << numerator
                      << " den=" << denominator
                      << " count=" << count
                      << " cols=" << p.size() << "\n";
            ++cases;
            ++mism;
            continue;
        }

        Divisor d(numerator, denominator);
        bool rowMismatch = false;
        for (long k = 0; k < count; ++k) {
            ++cases;
            int32_t expected = (int32_t)std::stoll(p[4 + k]);
            if (!d.hasNext()) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "MISMATCH hasNext()==false early num=" << numerator
                              << " den=" << denominator << " idx=" << k << "\n";
                }
                rowMismatch = true;
                break;
            }
            int32_t got = d.nextInt();
            if (got != expected) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "MISMATCH SEQ num=" << numerator
                              << " den=" << denominator << " idx=" << k
                              << " exp=" << expected << " got=" << got << "\n";
                }
            }
        }

        // Also verify the iterator terminates exactly when GT said (no extra
        // elements). This checks hasNext() against denominator precisely.
        if (!rowMismatch) {
            ++cases;
            if (d.hasNext()) {
                ++mism;
                if (mism <= 20) {
                    std::cerr << "MISMATCH overrun num=" << numerator
                              << " den=" << denominator
                              << " expected exhausted after " << count << "\n";
                }
            }
        }
    }

    std::cout << "Divisor cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
