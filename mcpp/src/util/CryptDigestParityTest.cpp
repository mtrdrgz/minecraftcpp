// Parity test for mc::crypt (util/Crypt.h) vs the PURE byte helpers of
// net.minecraft.util.Crypt (MC 26.1.2).
//
// Reads the TSV emitted by CryptDigestParity.java and compares, BYTE-FOR-BYTE:
//   * the 20-byte SHA-1 digest of the concatenated inputs (Crypt.digestData)
//   * the BigInteger(digest).toString(16) "server id hash" string (login line 182)
//
//   DIGEST  <nInputs> <hexIn0> ... <hexIn{n-1}> <hexDigest20> <serverHash>
#include "util/Crypt.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

// Decode lowercase hex into raw bytes.
static std::vector<uint8_t> from_hex(const std::string& h) {
    std::vector<uint8_t> out;
    out.reserve(h.size() / 2);
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (std::size_t i = 0; i + 1 < h.size(); i += 2)
        out.push_back(static_cast<uint8_t>((nib(h[i]) << 4) | nib(h[i + 1])));
    return out;
}

static std::string to_hex(const std::array<uint8_t, 20>& d) {
    static const char* HEX = "0123456789abcdef";
    std::string s;
    s.reserve(40);
    for (uint8_t b : d) {
        s.push_back(HEX[(b >> 4) & 0xF]);
        s.push_back(HEX[b & 0xF]);
    }
    return s;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: CryptDigestParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    long cases = 0;
    long mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line.back() == '\r') line.pop_back();
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "DIGEST") {
            // DIGEST <nInputs> <hexIn0>...<hexIn{n-1}> <hexDigest20> <serverHash>
            ++cases;
            int n = std::stoi(p[1]);
            // fields: p[0]=tag, p[1]=n, p[2..2+n-1]=inputs, then digest, then hash
            std::vector<std::vector<uint8_t>> inputs;
            inputs.reserve(static_cast<std::size_t>(n));
            for (int k = 0; k < n; ++k) inputs.push_back(from_hex(p[2 + k]));
            const std::string& expDigestHex = p[2 + n];
            const std::string& expServerHash = p[2 + n + 1];

            std::array<uint8_t, 20> got = mc::crypt::digestData(inputs);
            std::string gotDigestHex = to_hex(got);
            std::string gotServerHash = mc::crypt::serverHashHex(got);

            if (gotDigestHex != expDigestHex || gotServerHash != expServerHash) {
                ++mism;
                std::cerr << "DIGEST mismatch n=" << n << "\n"
                          << "  digest  expected=" << expDigestHex
                          << " got=" << gotDigestHex << "\n"
                          << "  srvhash expected=" << expServerHash
                          << " got=" << gotServerHash << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "CryptDigest cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
