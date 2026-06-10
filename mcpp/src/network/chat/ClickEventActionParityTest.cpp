// Bit/value-exact parity gate for net.minecraft.network.chat.ClickEvent.Action
// (Minecraft 26.1.2), ported in network/chat/ClickEventAction.h. Reads the TSV
// emitted by mcpp/tools/ClickEventActionParity.java and compares value-for-value.
//
// Tags:
//   CONST  <ordinal> <name()> <getSerializedName()> <isAllowedFromServer 0|1>
#include "network/chat/ClickEventAction.h"

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

// Map the Java enum-constant identifier (name()) to the C++ ordinal, so the test
// also asserts the declaration order matches (not just self-consistency).
static int ordFromName(const std::string& n) {
    if (n == "OPEN_URL") return 0;
    if (n == "OPEN_FILE") return 1;
    if (n == "RUN_COMMAND") return 2;
    if (n == "SUGGEST_COMMAND") return 3;
    if (n == "SHOW_DIALOG") return 4;
    if (n == "CHANGE_PAGE") return 5;
    if (n == "COPY_TO_CLIPBOARD") return 6;
    if (n == "CUSTOM") return 7;
    return -1;
}

int main(int argc, char** argv) {
    std::string casesPath;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--cases" && i + 1 < argc) casesPath = argv[++i];
    }
    if (casesPath.empty()) {
        std::cerr << "usage: ClickEventActionParityTest --cases <tsv>\n";
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
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty()) continue;
        std::vector<std::string> p = split_tabs(line);
        if (p.empty()) continue;
        const std::string& tag = p[0];

        if (tag == "CONST") {
            // CONST <ordinal> <name()> <getSerializedName()> <isAllowedFromServer 0|1>
            ++cases;
            int ord = std::stoi(p[1]);
            const std::string& jname = p[2];
            const std::string& serName = p[3];
            int allowed = std::stoi(p[4]);

            bool ok = true;
            // 1) Java ordinal must match the identifier-derived C++ ordinal
            //    (declaration order check).
            int expectOrd = ordFromName(jname);
            if (expectOrd != ord) ok = false;

            if (ord >= 0 && ord < mc::CLICK_EVENT_ACTION_COUNT) {
                auto v = static_cast<mc::ClickEventAction>(ord);
                // 2) getSerializedName() exact string match.
                if (serName != std::string(mc::clickEventActionSerializedName(v))) ok = false;
                // 3) isAllowedFromServer() exact boolean match.
                int gotAllowed = mc::clickEventActionIsAllowedFromServer(v) ? 1 : 0;
                if (gotAllowed != allowed) ok = false;
            } else {
                ok = false;
            }

            if (!ok) {
                ++mism;
                std::cerr << "CONST mismatch ord=" << ord << " name=" << jname
                          << " serName=" << serName << " allowed=" << allowed << "\n";
            }
        } else {
            // Unknown tag — never silently pass; count as a mismatch.
            ++mism;
            std::cerr << "unknown tag: " << tag << "\n";
        }
    }

    std::cout << "ClickEventAction cases=" << cases << " mismatches=" << mism << "\n";
    return mism == 0 ? 0 : 1;
}
