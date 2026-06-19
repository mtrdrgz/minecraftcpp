// Byte-exact parity for the PURE, string-only surface of net.minecraft.util.FileUtil
// against the REAL net.minecraft class (ground truth: mcpp/tools/FileUtilParity.java).
//
// Replays the TSV produced by the Java GT against the C++ port
// (mcpp/src/util/FileUtil.h, namespace mc::util::fileutil) and compares field-for-field:
//   SANITIZE  -> sanitizeName(in) bytes
//   PORTABLE  -> isPathPartPortable(in)            (reserved-Windows regex predicate)
//   ALLOWED   -> containsAllowedCharactersOnly(in) (private "[-._a-z0-9]+" full match)
//   VALIDSEG  -> isValidPathSegment(in)
//   DECOMPOSE -> decomposePath(in): ok flag + the exact segment list (order + bytes)
//   VALIDPATH -> validatePath(segments...): threw? (1) vs ok (0)
//
// String fields arrive base64(UTF-8); we decode to raw bytes and feed them to the port
// (the port models these byte strings 1:1; only ASCII/UTF-8 inputs are exercised).
//
//   file_util_parity [--cases mcpp/build/file_util.tsv]
//
// Prints exactly:  FileUtil checks=<N> mismatches=<M>
// Returns nonzero iff M>0. A mismatch is a REAL port bug — fix the port, not the test.
#include "FileUtil.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace fu = mc::util::fileutil;

namespace {

// Minimal RFC 4648 base64 decode (GT always pads; '=' terminates).
std::string b64decode(const std::string& in) {
    auto val = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1;  // '=' or stray
    };
    std::string out;
    int buf = 0, bits = 0;
    for (unsigned char c : in) {
        if (c == '=') break;
        int v = val(c);
        if (v < 0) continue;
        buf = (buf << 6) | v;
        bits += 6;
        if (bits >= 8) {
            bits -= 8;
            out.push_back(static_cast<char>((buf >> bits) & 0xFF));
        }
    }
    return out;
}

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> fields;
    std::string cur;
    for (char c : line) {
        if (c == '\t') { fields.push_back(cur); cur.clear(); }
        else cur.push_back(c);
    }
    fields.push_back(cur);
    return fields;
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/file_util.tsv";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }

    std::ifstream in(casesPath, std::ios::binary);
    if (!in) {
        std::cerr << "FileUtil: cannot open cases file: " << casesPath << "\n";
        return 2;
    }

    long checks = 0, mismatches = 0;
    auto fail = [&](const std::string& tag, const std::string& detail) {
        ++mismatches;
        if (mismatches <= 40) {
            std::cerr << "MISMATCH [" << tag << "] " << detail << "\n";
        }
    };

    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> f = split(line);
        const std::string& tag = f[0];

        if (tag == "SANITIZE") {
            // SANITIZE in_b64 out_b64
            std::string inp = b64decode(f[1]);
            std::string want = b64decode(f[2]);
            std::string got = fu::sanitizeName(inp);
            ++checks;
            if (got != want) fail(tag, "in=[" + inp + "] got=[" + got + "] want=[" + want + "]");
        } else if (tag == "PORTABLE") {
            std::string inp = b64decode(f[1]);
            int want = std::stoi(f[2]);
            int got = fu::isPathPartPortable(inp) ? 1 : 0;
            ++checks;
            if (got != want) fail(tag, "in=[" + inp + "] got=" + std::to_string(got) + " want=" + std::to_string(want));
        } else if (tag == "ALLOWED") {
            std::string inp = b64decode(f[1]);
            int want = std::stoi(f[2]);
            int got = fu::containsAllowedCharactersOnly(inp) ? 1 : 0;
            ++checks;
            if (got != want) fail(tag, "in=[" + inp + "] got=" + std::to_string(got) + " want=" + std::to_string(want));
        } else if (tag == "VALIDSEG") {
            std::string inp = b64decode(f[1]);
            int want = std::stoi(f[2]);
            int got = fu::isValidPathSegment(inp) ? 1 : 0;
            ++checks;
            if (got != want) fail(tag, "in=[" + inp + "] got=" + std::to_string(got) + " want=" + std::to_string(want));
        } else if (tag == "DECOMPOSE") {
            // DECOMPOSE in_b64 ok count seg0_b64 seg1_b64 ...
            std::string inp = b64decode(f[1]);
            int wantOk = std::stoi(f[2]);
            int wantCount = std::stoi(f[3]);
            std::vector<std::string> wantSegs;
            for (int i = 0; i < wantCount; ++i) wantSegs.push_back(b64decode(f[4 + i]));

            fu::DecomposeResult r = fu::decomposePath(inp);
            int gotOk = r.ok ? 1 : 0;
            ++checks;
            if (gotOk != wantOk) {
                fail(tag, "in=[" + inp + "] ok got=" + std::to_string(gotOk) + " want=" + std::to_string(wantOk));
            } else if (wantOk == 1) {
                // ok matches and is true: also compare the segment list exactly.
                bool eq = (r.segments.size() == wantSegs.size());
                for (size_t i = 0; eq && i < wantSegs.size(); ++i) eq = (r.segments[i] == wantSegs[i]);
                if (!eq) {
                    std::string gs, ws;
                    for (auto& s : r.segments) gs += "|" + s;
                    for (auto& s : wantSegs) ws += "|" + s;
                    fail(tag, "in=[" + inp + "] segs got=[" + gs + "] want=[" + ws + "]");
                }
            }
        } else if (tag == "VALIDPATH") {
            // VALIDPATH count seg0_b64 ... threw
            int count = std::stoi(f[1]);
            std::vector<std::string> segs;
            for (int i = 0; i < count; ++i) segs.push_back(b64decode(f[2 + i]));
            int wantThrew = std::stoi(f[2 + count]);
            int gotThrew = fu::validatePathOk(segs) ? 0 : 1;  // ok==true => no throw
            ++checks;
            if (gotThrew != wantThrew) {
                std::string ss;
                for (auto& s : segs) ss += "|" + s;
                fail(tag, "path=[" + ss + "] threw got=" + std::to_string(gotThrew) + " want=" + std::to_string(wantThrew));
            }
        }
        // unknown tags ignored
    }

    std::cout << "FileUtil checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
