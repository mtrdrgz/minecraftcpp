// Parity test for net.minecraft.world.phys.shapes.OffsetDoubleList.
//
// VERIFY-EXISTING: the C++ port already lives in world/phys/shapes/DoubleList.h
// (class mc::OffsetDoubleList). This gate exercises that header against ground truth
// from tools/OffsetDoubleListParity.java (the REAL net.minecraft class) and compares
// BIT-FOR-BIT.
//
//   offset_double_list_parity --cases mcpp/build/offset_double_list.tsv
//
// OffsetDoubleList.java (Minecraft 26.1.2):
//   ctor(DoubleList delegate, double offset): stores both verbatim.
//   getDouble(int index) = delegate.getDouble(index) + offset
//   size()              = delegate.size()
//
// The C++ getDouble is delegate_->getDouble(index) + offset_, a plain IEEE-754 add
// (no fma), matching Java's `+`. Offsets arrive as %016x raw long bits, decoded via
// std::bit_cast so the C++ side uses the EXACT same double the Java side added.
//
// Row formats (TAG \t inputs... \t outputs...):
//   ARR_SIZE   listId offsetBits           | size(int, decimal)
//   ARR_GET    listId offsetBits index      | getDouble(double, %016x raw bits)
//   CPR_SIZE   parts  offsetBits            | size(int, decimal)
//   CPR_GET    parts  offsetBits index       | getDouble(double, %016x raw bits)
//   NEST_GET   parts  o1Bits o2Bits index    | getDouble(double, %016x raw bits)

#include "world/phys/shapes/DoubleList.h"

#include <cstdint>
#include <cstdio>
#include <bit>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

using mc::CubePointRange;
using mc::DoubleArrayList;
using mc::DoubleListPtr;
using mc::OffsetDoubleList;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Java ints are signed 32-bit.
int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Parse a %016x raw-bits hex token into the double it encodes (matches the GT side).
double d_from_hex(const std::string& s) {
    uint64_t bits = std::stoull(s, nullptr, 16);
    return std::bit_cast<double>(bits);
}

// Parse a decimal raw long-bits token (offset emitted as Double.doubleToRawLongBits)
// into the exact double — guarantees the C++ offset == the Java offset bit-for-bit.
double d_from_longbits(const std::string& s) {
    // The value is a signed 64-bit decimal; reinterpret its bit pattern as a double.
    long long signedBits = std::stoll(s);
    uint64_t bits = static_cast<uint64_t>(signedBits);
    return std::bit_cast<double>(bits);
}

// The SAME array-backed delegate sample sets as OffsetDoubleListParity.java (listId == index).
const std::vector<std::vector<double>> ARRAYS = {
    { 0.0 },
    { 0.0, 1.0 },
    { 0.0, 0.5, 1.0 },
    { 0.0, 0.0625, 0.125, 0.1875, 0.25, 0.3125, 0.375, 0.4375, 0.5,
      0.5625, 0.625, 0.6875, 0.75, 0.8125, 0.875, 0.9375, 1.0 },
    { -1.0, -0.5, -0.25, 0.0, 0.25, 0.5, 1.0, 2.0, 3.0 },
    { 0.1, 0.2, 0.3, 0.7, 0.9, 1.3, 2.7, 3.14159265358979 },
    { 100.0, 200.5, 300.25, 1000.0, 12345.6789 },
    { -1024.0, -512.0, -1.0, 0.0, 1.0, 512.0, 1024.0, 1e6, 1e9 },
};

bool bitsEqual(double got, double exp) {
    return std::bit_cast<uint64_t>(got) == std::bit_cast<uint64_t>(exp);
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: offset_double_list_parity --cases <tsv>\n";
        return 2;
    }
    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "ARR_SIZE") {  // listId offsetBits | size
            int32_t listId = i(p[1]);
            double offset = d_from_longbits(p[2]);
            auto delegate = std::make_shared<const DoubleArrayList>(ARRAYS[static_cast<size_t>(listId)]);
            OffsetDoubleList off(delegate, offset);
            bad = off.size() != i(p[3]);
        } else if (tag == "ARR_GET") {  // listId offsetBits index | getDouble
            int32_t listId = i(p[1]);
            double offset = d_from_longbits(p[2]);
            int32_t index = i(p[3]);
            auto delegate = std::make_shared<const DoubleArrayList>(ARRAYS[static_cast<size_t>(listId)]);
            OffsetDoubleList off(delegate, offset);
            double got = off.getDouble(index);
            double exp = d_from_hex(p[4]);
            bad = !bitsEqual(got, exp);
        } else if (tag == "CPR_SIZE") {  // parts offsetBits | size
            int32_t parts = i(p[1]);
            double offset = d_from_longbits(p[2]);
            auto delegate = std::make_shared<const CubePointRange>(parts);
            OffsetDoubleList off(delegate, offset);
            bad = off.size() != i(p[3]);
        } else if (tag == "CPR_GET") {  // parts offsetBits index | getDouble
            int32_t parts = i(p[1]);
            double offset = d_from_longbits(p[2]);
            int32_t index = i(p[3]);
            auto delegate = std::make_shared<const CubePointRange>(parts);
            OffsetDoubleList off(delegate, offset);
            double got = off.getDouble(index);
            double exp = d_from_hex(p[4]);
            bad = !bitsEqual(got, exp);
        } else if (tag == "NEST_GET") {  // parts o1Bits o2Bits index | getDouble
            int32_t parts = i(p[1]);
            double o1 = d_from_longbits(p[2]);
            double o2 = d_from_longbits(p[3]);
            int32_t index = i(p[4]);
            DoubleListPtr base = std::make_shared<const CubePointRange>(parts);
            DoubleListPtr inner = std::make_shared<const OffsetDoubleList>(base, o1);
            OffsetDoubleList outer(inner, o2);
            double got = outer.getDouble(index);
            double exp = d_from_hex(p[5]);
            bad = !bitsEqual(got, exp);
        } else {
            std::cerr << "unknown tag: " << tag << "\n";
            ++mism;
            continue;
        }

        if (bad) {
            ++mism;
            if (mism <= 20) std::cerr << "MISMATCH [" << tag << "] line: " << line << "\n";
        }
    }

    std::cout << "OffsetDoubleList cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
