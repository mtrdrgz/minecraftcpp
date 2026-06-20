// VarInt parity gate vs the REAL net.minecraft.network.VarInt
// (tools/VarIntSizeParity.java ground truth).
//
// Two checks per int value, compared BIT-FOR-BIT against Java:
//   SIZE  — mc::net::varint::getByteSize(value)  == VarInt.getByteSize(value)
//   ENC   — mc::net::varint::write(value) bytes   == VarInt.write(b,value) bytes
//           AND the already-certified mc::net::PacketBuffer::writeVarInt produces
//           the identical bytes (the existing engine encode path), so this gate
//           also pins PacketBuffer's encoder.
//
//   varint_size_parity [--cases mcpp/build/varint_size.tsv]
#include "../../../network/VarIntSize.h"
#include "../../../network/PacketBuffer.h"

#include <bit>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace {
std::string hex(const std::vector<uint8_t>& v) {
    static const char* d = "0123456789abcdef";
    std::string s;
    for (uint8_t b : v) { s.push_back(d[b >> 4]); s.push_back(d[b & 15]); }
    return s;
}
} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "build/varint_size.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];
    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int cases = 0, mismatches = 0;
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream ss(line);
        std::string tag, vstr, expected;
        if (!std::getline(ss, tag, '\t') || !std::getline(ss, vstr, '\t') || !std::getline(ss, expected))
            continue;

        // value is a Java decimal int; parse as long then narrow (two's-complement).
        int32_t value = (int32_t)std::stoll(vstr);

        if (tag == "SIZE") {
            ++cases;
            int got = mc::net::varint::getByteSize(value);
            int want = (int)std::stoll(expected);
            // bit-exact: both are small ints; compare via bit_cast to be explicit.
            if (std::bit_cast<uint32_t>(got) != std::bit_cast<uint32_t>(want)) {
                ++mismatches;
                std::cerr << "SIZE-MISMATCH value=" << value
                          << " got=" << got << " want=" << want << "\n";
            }
        } else if (tag == "ENC") {
            ++cases;
            std::string got = hex(mc::net::varint::write(value));
            if (got != expected) {
                ++mismatches;
                std::cerr << "ENC-MISMATCH value=" << value
                          << "\n  got  " << got << "\n  want " << expected << "\n";
            }
            // Pin the existing engine encoder too.
            mc::net::PacketBuffer pb;
            pb.writeVarInt(value);
            std::string gotPB = hex(pb.data());
            if (gotPB != expected) {
                ++mismatches;
                std::cerr << "ENC-PACKETBUFFER-MISMATCH value=" << value
                          << "\n  got  " << gotPB << "\n  want " << expected << "\n";
            }
        }
    }

    std::cout << "VarIntSizeParity cases=" << cases << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
