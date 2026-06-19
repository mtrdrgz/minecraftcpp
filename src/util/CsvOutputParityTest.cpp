// Parity test for net.minecraft.util.CsvOutput (and its commons-lang3 escapeCsv
// dependency) against ground truth from tools/CsvOutputParity.java.
//
//   csv_output_parity --cases mcpp/build/csv_output.tsv
//
// String fields are encoded as %02x per UTF-8 (ASCII) byte; sentinel "EMPTY" => "".
// For DOC rows the GT emits a document index; we rebuild the SAME headers/rows here
// (mirroring the GT fixtures exactly) and compare the produced text byte-for-byte.

#include "CsvOutput.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

namespace {

using mc::util::CsvOutput;

std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}

// Decode %02x-per-byte (sentinel "EMPTY" => "").
std::string decode(const std::string& field) {
    if (field == "EMPTY") return std::string();
    std::string out;
    out.reserve(field.size() / 2);
    for (size_t p = 0; p + 2 <= field.size(); p += 2) {
        unsigned long v = std::stoul(field.substr(p, 2), nullptr, 16);
        out.push_back(static_cast<char>(static_cast<unsigned char>(v)));
    }
    return out;
}

// Re-encode the same way the GT tool does so we can compare against the expected hex.
std::string encode(const std::string& s) {
    if (s.empty()) return "EMPTY";
    static const char* hexd = "0123456789abcdef";
    std::string out;
    out.reserve(s.size() * 2);
    for (unsigned char c : s) {
        out.push_back(hexd[(c >> 4) & 0xF]);
        out.push_back(hexd[c & 0xF]);
    }
    return out;
}

int32_t i(const std::string& s) { return static_cast<int32_t>(std::stoll(s)); }
bool b(const std::string& s) { return std::stoll(s) != 0; }

// Build the document fixture identical to the GT tool's `docs` list, by index.
// Returns the produced text via the C++ CsvOutput.
std::string buildDoc(int idx) {
    std::vector<std::string> headers;
    std::vector<std::vector<std::string>> rows;
    switch (idx) {
        case 0:
            headers = {"name"};
            rows = {{"alice"}, {"bob"}};
            break;
        case 1:
            headers = {"id", "value"};
            rows = {{"1", "x"}, {"2", "y"}, {"3", "z"}};
            break;
        case 2:
            headers = {"a", "b", "c"};
            rows = {{"p,q", "r\"s", "t\r\nu"}};
            break;
        case 3:
            headers = {"col,1", "col\"2"};
            rows = {{"v1", "v2"}};
            break;
        case 4:
            headers = {"", "h"};
            rows = {{"", ""}, {"x", ""}};
            break;
        case 5:
            headers = {"only", "header", "row"};
            rows = {};
            break;
        case 6:
            headers = {"he said \"hi\""};
            rows = {{"a\"b"}};
            break;
        case 7:
            headers = {"c1", "c2", "c3", "c4", "c5"};
            rows = {{"1", "two", "3,3", "fo\"ur", "5\n5"}};
            break;
        default:
            return std::string("\x01<unknown-doc>");
    }
    CsvOutput::Builder bld = CsvOutput::builder();
    for (const auto& h : headers) bld.addColumn(h);
    CsvOutput csv = bld.build();
    for (const auto& r : rows) csv.writeRow(r);
    return csv.text();
}

}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: csv_output_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    long long n = 0, mism = 0;
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();  // tolerate CRLF tsv
        if (line.empty()) continue;
        auto p = split(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];
        ++n;
        bool bad = false;

        if (tag == "ESCAPE") {  // inHex outHex
            std::string input = decode(p[1]);
            std::string got = CsvOutput::escapeCsv(input);
            bad = encode(got) != p[2];
        } else if (tag == "NULLCELL") {  // inHex outHex  (== escapeCsv("[null]"))
            // getStringValue(null) -> escapeCsv("[null]")
            std::string got = CsvOutput::getStringValue("", /*isNull=*/true);
            bad = encode(got) != p[2];
            // also sanity-check the input field decodes to "[null]"
            if (decode(p[1]) != "[null]") bad = true;
        } else if (tag == "DOC") {  // idx docHex
            int idx = i(p[1]);
            std::string got = buildDoc(idx);
            bad = encode(got) != p[2];
        } else if (tag == "COLCHECK") {  // nCols gotCols threw(0/1)
            int nCols = i(p[1]);
            int gotCols = i(p[2]);
            bool expectThrew = b(p[3]);
            // Build an nCols CsvOutput and feed a gotCols-wide row.
            CsvOutput::Builder bld = CsvOutput::builder();
            for (int k = 0; k < nCols; ++k) bld.addColumn("h" + std::to_string(k));
            CsvOutput csv = bld.build();
            std::vector<std::string> row(gotCols);
            for (int k = 0; k < gotCols; ++k) row[k] = "v" + std::to_string(k);
            bool threw = false;
            try {
                csv.writeRow(row);
            } catch (const std::invalid_argument&) {
                threw = true;
            }
            bad = (threw != expectThrew);
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

    std::cout << "CsvOutput cases=" << n << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
