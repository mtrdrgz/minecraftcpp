// Parity test for mc::world::entity::player::ChatVisiblity (ChatVisiblity.h) vs Java
// ground truth. Reads the TSV emitted by ChatVisibilityParity.java and compares
// value-for-value, bit-for-bit.
#include "world/entity/player/ChatVisiblity.h"

#include <cstdint>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace cv = mc::world::entity::player;

static std::vector<std::string> split_tabs(const std::string& line) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream ss(line);
    while (std::getline(ss, cur, '\t')) out.push_back(cur);
    return out;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: ChatVisibilityParityTest --cases <tsv>\n";
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

        if (tag == "CONST") {
            // CONST <ordinal> <name()> <id> <key>
            if (p.size() < 5) { ++mism; std::cerr << "CONST malformed\n"; continue; }
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& name = p[2];
            int id = std::stoi(p[3]);
            const std::string& key = p[4];

            auto v = static_cast<cv::ChatVisiblity>(ord);
            bool ok = true;
            if (name != std::string(cv::chatVisibilityName(v))) ok = false;
            if (id != cv::chatVisibilityId(v)) ok = false;
            if (key != std::string(cv::chatVisibilityKey(v))) ok = false;

            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord
                          << " name=" << name
                          << " (cpp=" << std::string(cv::chatVisibilityName(v)) << ")"
                          << " id=" << id
                          << " (cpp=" << cv::chatVisibilityId(v) << ")"
                          << " key=" << key
                          << " (cpp=" << std::string(cv::chatVisibilityKey(v)) << ")"
                          << "\n";
            }
        } else if (tag == "COUNT") {
            // COUNT <values().length>
            ++cases;
            int count = std::stoi(p[1]);
            if (count != cv::CHAT_VISIBILITY_COUNT) {
                ++mism;
                std::cerr << "COUNT mismatch java=" << count
                          << " cpp=" << cv::CHAT_VISIBILITY_COUNT << "\n";
            }
        } else if (tag == "BYID") {
            // BYID <inputId> <ordinal of BY_ID.apply(inputId)>
            if (p.size() < 3) { ++mism; std::cerr << "BYID malformed\n"; continue; }
            ++cases;
            int inputId = std::stoi(p[1]);
            int expectedOrd = std::stoi(p[2]);

            int gotOrd = static_cast<int>(cv::chatVisibilityById(inputId));
            if (gotOrd != expectedOrd) {
                ++mism;
                std::cerr << "BYID mismatch id=" << inputId
                          << " java_ord=" << expectedOrd
                          << " cpp_ord=" << gotOrd << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "ChatVisibility cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
