// Byte-exact parity for the STRING-LEVEL surface of
// net.minecraft.resources.Identifier against the REAL net.minecraft class
// (ground truth: mcpp/tools/IdentifierParity.java).
//
// Replays the TSV produced by the Java GT against the C++ port
// (mcpp/src/resources/Identifier.h) and compares field-for-field:
//   tryParse      -> isNull, and (if non-null) namespace/path/toString
//   fromNamespaceAndPath / withDefaultNamespace -> threw flag + result fields
//   tryBuild      -> isNull + result fields
//   compareTo     -> sign of a.compareTo(b) for all ordered pairs (-1/0/1)
//
// String fields arrive base64(UTF-8); we decode to raw bytes and feed them to the
// port (the port models these ASCII byte strings as UTF-16 code units 1:1).
//
//   identifier_parity [--cases mcpp/build/identifier.tsv]
//
// Prints exactly:  Identifier checks=<N> mismatches=<M>
// Returns nonzero iff M>0. A mismatch is a REAL port bug — fix the port, not the test.
#include "Identifier.h"

#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

using mc::resources::Identifier;

namespace {

// Minimal RFC 4648 base64 decode (no padding tolerance needed; GT always pads).
std::string b64decode(const std::string& in) {
    auto val = [](unsigned char c) -> int {
        if (c >= 'A' && c <= 'Z') return c - 'A';
        if (c >= 'a' && c <= 'z') return c - 'a' + 26;
        if (c >= '0' && c <= '9') return c - '0' + 52;
        if (c == '+') return 62;
        if (c == '/') return 63;
        return -1; // '=' or stray
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

// "-" sentinel means absent (used for namespace/path/toString when null/threw).
bool isSentinel(const std::string& s) { return s == "-"; }

} // namespace

int main(int argc, char** argv) {
    std::string casesPath = "mcpp/build/identifier.tsv";
    for (int i = 1; i + 1 < argc; ++i)
        if (std::string(argv[i]) == "--cases") casesPath = argv[i + 1];

    std::ifstream f(casesPath, std::ios::binary);
    if (!f) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    int checks = 0, mismatches = 0;

    // Fixed compareTo table, reconstructed from the CMPID rows so indices align
    // with the Java table without hardcoding it twice.
    std::vector<std::optional<Identifier>> cmpTable;

    auto mismatch = [&](const std::string& msg) {
        ++mismatches;
        std::cerr << "MISMATCH " << msg << "\n";
    };

    // Compare a produced Identifier (optional) against expected fields.
    // wantNull: 1 => expected absent (null/threw). On present, compare ns/path/toString.
    auto checkResult = [&](const std::string& tag,
                           const std::string& key,
                           const std::optional<Identifier>& got,
                           int wantNull,
                           const std::string& wantNsB64,
                           const std::string& wantPathB64,
                           const std::string& wantStrB64) {
        ++checks;
        bool gotNull = !got.has_value();
        if (gotNull != (wantNull != 0)) {
            mismatch(tag + " [" + key + "] isNull got " + (gotNull ? "1" : "0")
                     + " want " + std::to_string(wantNull));
            return;
        }
        if (gotNull) return; // both absent; nothing more to compare
        std::string wantNs = b64decode(wantNsB64);
        std::string wantPath = b64decode(wantPathB64);
        std::string wantStr = b64decode(wantStrB64);
        const Identifier& id = *got;
        if (id.getNamespace() != wantNs)
            mismatch(tag + " [" + key + "] namespace got \"" + id.getNamespace() + "\" want \"" + wantNs + "\"");
        if (id.getPath() != wantPath)
            mismatch(tag + " [" + key + "] path got \"" + id.getPath() + "\" want \"" + wantPath + "\"");
        if (id.toString() != wantStr)
            mismatch(tag + " [" + key + "] toString got \"" + id.toString() + "\" want \"" + wantStr + "\"");
    };

    // A throwing factory invocation: capture whether it threw and the result.
    auto runThrowing = [](auto fn) -> std::optional<Identifier> {
        try {
            return std::optional<Identifier>(fn());
        } catch (const mc::resources::IdentifierException&) {
            return std::nullopt;
        }
    };

    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> fld = split(line);
        const std::string& tag = fld[0];

        if (tag == "PARSE") {
            // PARSE input_b64 isNull ns_b64 path_b64 str_b64
            if (fld.size() != 6) { mismatch("bad PARSE row: " + line); continue; }
            std::string input = b64decode(fld[1]);
            int wantNull = std::stoi(fld[2]);
            std::optional<Identifier> got = Identifier::tryParse(input);
            checkResult("PARSE", input, got, wantNull, fld[3], fld[4], fld[5]);
        } else if (tag == "BUILD") {
            // BUILD method ns_b64 path_b64 threw ns_b64 path_b64 str_b64
            if (fld.size() != 8) { mismatch("bad BUILD row: " + line); continue; }
            const std::string& method = fld[1];
            std::string ns = b64decode(fld[2]);
            std::string path = b64decode(fld[3]);
            int wantThrew = std::stoi(fld[4]);
            std::optional<Identifier> got;
            std::string key;
            if (method == "fromNamespaceAndPath") {
                key = "fromNamespaceAndPath(" + ns + "," + path + ")";
                got = runThrowing([&] { return Identifier::fromNamespaceAndPath(ns, path); });
            } else if (method == "withDefaultNamespace") {
                key = "withDefaultNamespace(" + path + ")";
                got = runThrowing([&] { return Identifier::withDefaultNamespace(path); });
            } else {
                mismatch("unknown BUILD method: " + method);
                continue;
            }
            // threw == isNull in our optional model.
            checkResult("BUILD/" + method, key, got, wantThrew, fld[5], fld[6], fld[7]);
        } else if (tag == "TRYBUILD") {
            // TRYBUILD ns_b64 path_b64 isNull ns_b64 path_b64 str_b64
            if (fld.size() != 7) { mismatch("bad TRYBUILD row: " + line); continue; }
            std::string ns = b64decode(fld[1]);
            std::string path = b64decode(fld[2]);
            int wantNull = std::stoi(fld[3]);
            std::optional<Identifier> got = Identifier::tryBuild(ns, path);
            checkResult("TRYBUILD", "tryBuild(" + ns + "," + path + ")", got, wantNull, fld[4], fld[5], fld[6]);
        } else if (tag == "CMPID") {
            // CMPID index toString_b64 — rebuild the table via parse (toString is "ns:path").
            if (fld.size() != 3) { mismatch("bad CMPID row: " + line); continue; }
            std::size_t idx = static_cast<std::size_t>(std::stoul(fld[1]));
            std::string str = b64decode(fld[2]);
            if (cmpTable.size() <= idx) cmpTable.resize(idx + 1);
            // The fixed table only holds valid identifiers, so parse must succeed.
            std::optional<Identifier> id = Identifier::tryParse(str);
            cmpTable[idx] = id;
            ++checks;
            if (!id.has_value() || id->toString() != str)
                mismatch("CMPID [" + std::to_string(idx) + "] failed to rebuild \"" + str + "\"");
        } else if (tag == "CMP") {
            // CMP a b sign
            if (fld.size() != 4) { mismatch("bad CMP row: " + line); continue; }
            std::size_t a = static_cast<std::size_t>(std::stoul(fld[1]));
            std::size_t b = static_cast<std::size_t>(std::stoul(fld[2]));
            int wantSign = std::stoi(fld[3]);
            ++checks;
            if (a >= cmpTable.size() || b >= cmpTable.size()
                || !cmpTable[a].has_value() || !cmpTable[b].has_value()) {
                mismatch("CMP refers to missing table entry " + std::to_string(a) + "," + std::to_string(b));
                continue;
            }
            int raw = cmpTable[a]->compareTo(*cmpTable[b]);
            int gotSign = (raw > 0) - (raw < 0);
            if (gotSign != wantSign)
                mismatch("CMP [" + std::to_string(a) + " vs " + std::to_string(b) + "] sign got "
                         + std::to_string(gotSign) + " want " + std::to_string(wantSign));
        } else if (tag == "XFORM") {
            // XFORM input withPrefix withSuffix withPath toLangKey toLangKey(p) toLangKey(p,s) toShortLang toShortStr toDebugFile
            if (fld.size() != 11) { mismatch("bad XFORM row: " + line); continue; }
            std::string input = b64decode(fld[1]);
            std::optional<Identifier> got = Identifier::tryParse(input);
            if (!got.has_value()) { mismatch("XFORM input not parseable: " + input); continue; }
            const Identifier& id = *got;
            auto chkStr = [&](const char* what, const std::string& g, const std::string& wantB64) {
                ++checks;
                std::string want = b64decode(wantB64);
                if (g != want) mismatch(std::string("XFORM [") + input + "] " + what + " got \"" + g + "\" want \"" + want + "\"");
            };
            chkStr("withPrefix", id.withPrefix("pre_").toString(), fld[2]);
            chkStr("withSuffix", id.withSuffix("_suf").toString(), fld[3]);
            chkStr("withPath", id.withPath(std::string("newpath")).toString(), fld[4]);
            chkStr("toLanguageKey", id.toLanguageKey(), fld[5]);
            chkStr("toLanguageKey(p)", id.toLanguageKey("block"), fld[6]);
            chkStr("toLanguageKey(p,s)", id.toLanguageKey("block", "desc"), fld[7]);
            chkStr("toShortLanguageKey", id.toShortLanguageKey(), fld[8]);
            chkStr("toShortString", id.toShortString(), fld[9]);
            chkStr("toDebugFileName", id.toDebugFileName(), fld[10]);
        } else if (tag == "ALLOWED") {
            // ALLOWED charCode 0|1
            if (fld.size() != 3) { mismatch("bad ALLOWED row: " + line); continue; }
            int code = std::stoi(fld[1]);
            int want = std::stoi(fld[2]);
            ++checks;
            int gotAllowed = Identifier::isAllowedInIdentifier((char)code) ? 1 : 0;
            if (gotAllowed != want)
                mismatch("ALLOWED [" + std::to_string(code) + "] got " + std::to_string(gotAllowed) + " want " + std::to_string(want));
        } else {
            // Unknown tag — ignore (forward compatible).
            continue;
        }
    }

    std::cout << "Identifier checks=" << checks << " mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
