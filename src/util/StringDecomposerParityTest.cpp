// Parity test for the PURE, fixed-string methods of net.minecraft.util.StringDecomposer.
// Ground truth: tools/StringDecomposerParity.java vs the real class. Every emitted
// (position, style, codePoint) record is recomputed via util/StringDecomposer.h and
// compared byte-for-byte (as the same encoded stream string the GT tool produces).
//
//   string_decomposer_parity --cases mcpp/build/string_decomposer.tsv
//
// String fields are encoded as concatenated %04x per UTF-16 code unit; "EMPTY" => "".
// Style specs are a sequence of legacy format code chars applied to EMPTY ("EMPTY" => EMPTY).

#include "StringDecomposer.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace sd = mc::util::stringdecomposer;

namespace {

std::vector<std::string> split(const std::string& line, char delim) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, delim)) out.push_back(it);
    return out;
}

int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }

// Decode the %04x-per-code-unit encoding ("EMPTY" => "") into a UTF-16 string.
std::u16string decode(const std::string& field) {
    if (field == "EMPTY") return std::u16string();
    std::u16string out;
    for (size_t p = 0; p + 4 <= field.size(); p += 4) {
        unsigned long v = std::stoul(field.substr(p, 4), nullptr, 16);
        out.push_back(static_cast<char16_t>(v));
    }
    return out;
}

// Re-encode a u16string the same way the GT tool does ("EMPTY" for empty).
std::string encode(const std::u16string& s) {
    if (s.empty()) return "EMPTY";
    static const char* hexd = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() * 4);
    for (char16_t c : s) {
        uint16_t v = static_cast<uint16_t>(c);
        out.push_back(hexd[(v >> 12) & 0xF]);
        out.push_back(hexd[(v >> 8) & 0xF]);
        out.push_back(hexd[(v >> 4) & 0xF]);
        out.push_back(hexd[v & 0xF]);
    }
    return out;
}

// Build a DecomposerStyle by applying each legacy code char (in order) to EMPTY, exactly
// like the GT tool's buildStyle. "EMPTY" => the empty style.
sd::DecomposerStyle buildStyle(const std::string& spec) {
    sd::DecomposerStyle s = sd::EMPTY_STYLE();
    if (spec == "EMPTY") return s;
    for (char ch : spec) {
        sd::Formatting f = sd::getByCode(static_cast<char16_t>(static_cast<unsigned char>(ch)));
        if (f == sd::Formatting::RESET) {
            s = sd::EMPTY_STYLE();
        } else if (f != sd::Formatting::NONE) {
            s = sd::applyLegacyFormat(s, f);
        }
    }
    return s;
}

// A recorder sink that reproduces the GT tool's tuple-stream encoding.
struct Recorder {
    std::string sb;
    int count = 0;

    bool accept(int32_t position, const sd::DecomposerStyle& style, int32_t codepoint) {
        if (count > 0) sb.push_back(';');
        int colorFlag = style.hasColor ? 1 : 0;
        int32_t colorVal = style.hasColor ? style.color : -1;
        sb += std::to_string(position); sb.push_back(':');
        sb += std::to_string(colorFlag); sb.push_back(':');
        sb += std::to_string(colorVal); sb.push_back(':');
        sb += std::to_string(style.bold ? 1 : 0); sb.push_back(':');
        sb += std::to_string(style.italic ? 1 : 0); sb.push_back(':');
        sb += std::to_string(style.underlined ? 1 : 0); sb.push_back(':');
        sb += std::to_string(style.strikethrough ? 1 : 0); sb.push_back(':');
        sb += std::to_string(style.obfuscated ? 1 : 0); sb.push_back(':');
        sb += std::to_string(codepoint);
        count++;
        return true;
    }

    std::string stream() const { return sb.empty() ? std::string("EMPTY") : sb; }
};

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) {
        std::cerr << "usage: string_decomposer_parity --cases <tsv>\n";
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
        auto p = split(line, '\t');
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "IF") {  // caseId inputHex offset curSpec resetSpec ret count stream
            std::u16string s = decode(p[2]);
            int32_t offset = i(p[3]);
            sd::DecomposerStyle cur = buildStyle(p[4]);
            sd::DecomposerStyle rst = buildStyle(p[5]);
            int32_t expRet = i(p[6]);
            int32_t expCount = i(p[7]);
            const std::string& expStream = p[8];
            Recorder rec;
            bool ret = sd::iterateFormatted(
                s, offset, cur, rst,
                [&rec](int32_t pos, const sd::DecomposerStyle& st, int32_t cp) {
                    return rec.accept(pos, st, cp);
                });
            bad = (ret ? 1 : 0) != expRet || rec.count != expCount || rec.stream() != expStream;
        } else if (tag == "IT") {  // caseId inputHex styleSpec ret count stream
            std::u16string s = decode(p[2]);
            sd::DecomposerStyle st = buildStyle(p[3]);
            int32_t expRet = i(p[4]);
            int32_t expCount = i(p[5]);
            const std::string& expStream = p[6];
            Recorder rec;
            bool ret = sd::iterate(
                s, st, [&rec](int32_t pos, const sd::DecomposerStyle& sty, int32_t cp) {
                    return rec.accept(pos, sty, cp);
                });
            bad = (ret ? 1 : 0) != expRet || rec.count != expCount || rec.stream() != expStream;
        } else if (tag == "IB") {  // caseId inputHex styleSpec ret count stream
            std::u16string s = decode(p[2]);
            sd::DecomposerStyle st = buildStyle(p[3]);
            int32_t expRet = i(p[4]);
            int32_t expCount = i(p[5]);
            const std::string& expStream = p[6];
            Recorder rec;
            bool ret = sd::iterateBackwards(
                s, st, [&rec](int32_t pos, const sd::DecomposerStyle& sty, int32_t cp) {
                    return rec.accept(pos, sty, cp);
                });
            bad = (ret ? 1 : 0) != expRet || rec.count != expCount || rec.stream() != expStream;
        } else if (tag == "FBS") {  // caseId inputHex outHex
            std::u16string s = decode(p[2]);
            const std::string& expOut = p[3];
            bad = encode(sd::filterBrokenSurrogates(s)) != expOut;
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

    std::cout << "StringDecomposer cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
