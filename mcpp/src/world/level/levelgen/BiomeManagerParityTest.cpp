#include "BiomeManager.h"

#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

bool checkEqual(const char* label, int64_t actual, int64_t expected) {
    if (actual == expected) {
        return true;
    }
    std::cerr << label << " expected " << expected << " got " << actual << '\n';
    return false;
}

bool checkNear(const char* label, double actual, double expected) {
    if (std::abs(actual - expected) <= 1.0e-12) {
        return true;
    }
    std::cerr << label << " expected " << expected << " got " << actual << '\n';
    return false;
}

bool checkQuart(const char* label, std::array<int, 3> actual, std::array<int, 3> expected) {
    if (actual == expected) {
        return true;
    }
    std::cerr << label << " expected " << expected[0] << ',' << expected[1] << ',' << expected[2]
              << " got " << actual[0] << ',' << actual[1] << ',' << actual[2] << '\n';
    return false;
}

} // namespace

int main() {
    using mc::levelgen::BiomeManager;

    bool ok = true;
    ok &= checkEqual("obfuscateSeed(0)", BiomeManager::obfuscateSeed(0), 8794265229978523055LL);
    ok &= checkEqual("obfuscateSeed(1)", BiomeManager::obfuscateSeed(1), -6467378160175308932LL);
    ok &= checkEqual("obfuscateSeed(-1)", BiomeManager::obfuscateSeed(-1), 6759447113877070610LL);
    ok &= checkEqual("obfuscateSeed(12345)", BiomeManager::obfuscateSeed(12345), 293737985876514017LL);
    ok &= checkEqual("obfuscateSeed(987654321)", BiomeManager::obfuscateSeed(987654321), -1416572152150867491LL);
    ok &= checkEqual("obfuscateSeed(-98765432123456789)", BiomeManager::obfuscateSeed(-98765432123456789LL), 5149838139034460041LL);

    ok &= checkNear("fiddle(0)", BiomeManager::debugGetFiddle(0), -0.45);
    ok &= checkNear("fiddle(1)", BiomeManager::debugGetFiddle(1), -0.45);
    ok &= checkNear("fiddle(-1)", BiomeManager::debugGetFiddle(-1), 0.44912109375000003);
    ok &= checkNear("fiddle(123456789)", BiomeManager::debugGetFiddle(123456789), -0.44384765625);
    ok &= checkNear("fiddle(-98765432123456789)", BiomeManager::debugGetFiddle(-98765432123456789LL), -0.2548828125);

    const int64_t zoom = BiomeManager::obfuscateSeed(12345);
    ok &= checkNear("fiddledDistance", BiomeManager::debugGetFiddledDistance(zoom, -3, 7, 19, 0.25, -0.5, 0.75), 1.3972174263000490);
    ok &= checkQuart("quart(0,64,0)", BiomeManager::debugSelectQuart(zoom, 0, 64, 0), { -1, 16, -1 });
    ok &= checkQuart("quart(1,64,1)", BiomeManager::debugSelectQuart(zoom, 1, 64, 1), { 0, 15, 0 });
    ok &= checkQuart("quart(-17,70,33)", BiomeManager::debugSelectQuart(zoom, -17, 70, 33), { -5, 17, 8 });
    ok &= checkQuart("quart(128,-32,-65)", BiomeManager::debugSelectQuart(zoom, 128, -32, -65), { 32, -8, -17 });
    ok &= checkQuart("quart(300000,80,-300000)", BiomeManager::debugSelectQuart(zoom, 300000, 80, -300000), { 74999, 20, -75001 });

    if (!ok) {
        return 1;
    }
    std::cout << "BiomeManager parity checks passed\n";
    return 0;
}
