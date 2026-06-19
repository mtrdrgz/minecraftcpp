// Parity test for mc::network::chat::HoverEventAction (network/chat/HoverEventAction.h)
// vs Java ground truth. Reads the TSV emitted by HoverEventActionParity.java and
// compares value-for-value (this enum carries no floats).
#include "network/chat/HoverEventAction.h"

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

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: HoverEventActionParityTest --cases <tsv>\n";
        return 2;
    }

    std::ifstream in(casesPath);
    if (!in) {
        std::cerr << "cannot open " << casesPath << "\n";
        return 2;
    }

    using namespace mc::network::chat;

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
            // CONST <ordinal> <name()> <getSerializedName> <isAllowedFromServer> <toString>
            ++cases;
            if (p.size() < 6) {
                ++mism;
                std::cerr << "CONST malformed row: " << line << "\n";
                continue;
            }
            int ord = std::stoi(p[1]);
            const std::string& enumName = p[2];
            const std::string& serName = p[3];
            int allow = std::stoi(p[4]);
            const std::string& toStr = p[5];

            if (ord < 0 || ord >= HOVER_EVENT_ACTION_COUNT) {
                ++mism;
                std::cerr << "CONST ordinal out of range: " << ord << "\n";
                continue;
            }
            auto v = static_cast<HoverEventAction>(ord);
            bool ok = true;
            if (hoverEventActionOrdinal(v) != ord) ok = false;
            if (enumName != std::string(hoverEventActionName(v))) ok = false;
            if (serName != std::string(hoverEventActionSerializedName(v))) ok = false;
            if ((hoverEventActionIsAllowedFromServer(v) ? 1 : 0) != allow) ok = false;
            if (toStr != hoverEventActionToString(v)) ok = false;
            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord
                          << " name=" << enumName
                          << " serialized=" << serName
                          << " allow=" << allow
                          << " toString=" << toStr << "\n";
            }
        }
        // unknown tags ignored
    }

    std::cout << "HoverEventAction cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
