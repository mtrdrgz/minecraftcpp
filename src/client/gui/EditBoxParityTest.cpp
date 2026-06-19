// Parity gate for mc::gui::EditBoxLogic vs the real net.minecraft.client.gui.components.EditBox
// font-independent edit core. Replays the SAME op scenarios as EditBoxParity.java and compares value /
// cursor / highlight / highlighted (UTF-16, as %04x-per-char hex) + word/codepoint query results.
//
//   edit_box_parity --cases mcpp/build/edit_box.tsv

#include "EditBox.h"

#include <cstdio>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace gui = mc::gui;

namespace {
std::vector<std::string> split(const std::string& line) {
    std::vector<std::string> out;
    std::string it;
    std::istringstream ss(line);
    while (std::getline(ss, it, '\t')) out.push_back(it);
    return out;
}
int I(const std::string& s) { return std::stoi(s); }

std::string hex(const std::u16string& s) {
    if (s.empty()) return ".";
    std::string o;
    char buf[8];
    for (char16_t c : s) { std::snprintf(buf, sizeof(buf), "%04x", (unsigned)c); o += buf; }
    return o;
}

struct Op { std::string code; std::u16string s; int a; int b; };
Op maxlen(int n) { return {"maxlen", u"", n, 0}; }
Op setval(std::u16string s) { return {"setval", std::move(s), 0, 0}; }
Op setcur(int n) { return {"setcur", u"", n, 0}; }
Op sethl(int n) { return {"sethl", u"", n, 0}; }
Op movecur(int d, int sh) { return {"movecur", u"", d, sh}; }
Op moveto(int p, int e) { return {"moveto", u"", p, e}; }
Op movestart(int sh) { return {"movestart", u"", sh, 0}; }
Op moveend(int sh) { return {"moveend", u"", sh, 0}; }
Op delchars(int d) { return {"delchars", u"", d, 0}; }
Op delwords(int d) { return {"delwords", u"", d, 0}; }
Op deltopos(int p) { return {"deltopos", u"", p, 0}; }
Op insert(std::u16string s) { return {"insert", std::move(s), 0, 0}; }
Op wordpos(int d) { return {"wordpos", u"", d, 0}; }
Op getcurpos(int d) { return {"getcurpos", u"", d, 0}; }

const std::u16string EMOJI = u"\U0001F600";  // U+1F600 surrogate pair

std::vector<std::vector<Op>> scenarios() {
    return {
        { maxlen(100), setval(u"hello world foo"), movestart(0), wordpos(1), movecur(1, 0),
          moveto(11, 0), delwords(-1), moveend(0), delwords(-1) },
        { maxlen(20), setval(u"abcdef"), setcur(2), sethl(4), insert(u"XYZ"), setcur(2), sethl(5),
          delchars(-1), setcur(0), sethl(0), delchars(1) },
        { maxlen(50), setval(u"a" + EMOJI + u"b"), movestart(0), movecur(1, 0), getcurpos(1),
          movecur(1, 0), movecur(1, 0), movestart(0), moveto(1, 0), delchars(1) },
        { maxlen(3), setval(u"hello"), setcur(0), sethl(0), insert(EMOJI), maxlen(10),
          setcur(3), sethl(3), insert(u"XY" + EMOJI) },
        { maxlen(100), setval(u"foo   bar baz"), movestart(0), wordpos(1), delwords(1),
          moveend(0), delwords(-1), wordpos(-1) },
        { maxlen(5), setval(u"ab"), setcur(2), sethl(2), insert(u"cd" + EMOJI + u"ef") }
    };
}

// Per-step captured state.
struct Rec { bool query; std::string valHex, hlHex; int cursor, highlight, qres; };

std::vector<std::vector<Rec>> replayAll() {
    auto SCEN = scenarios();
    std::vector<std::vector<Rec>> all;
    for (auto& ops : SCEN) {
        gui::EditBoxLogic eb;
        std::vector<Rec> recs;
        for (auto& op : ops) {
            Rec r{};
            if (op.code == "maxlen") eb.setMaxLength(op.a);
            else if (op.code == "setval") eb.setValue(op.s);
            else if (op.code == "setcur") eb.setCursorPosition(op.a);
            else if (op.code == "sethl") eb.setHighlightPos(op.a);
            else if (op.code == "movecur") eb.moveCursor(op.a, op.b != 0);
            else if (op.code == "moveto") eb.moveCursorTo(op.a, op.b != 0);
            else if (op.code == "movestart") eb.moveCursorToStart(op.a != 0);
            else if (op.code == "moveend") eb.moveCursorToEnd(op.a != 0);
            else if (op.code == "delchars") eb.deleteChars(op.a);
            else if (op.code == "delwords") eb.deleteWords(op.a);
            else if (op.code == "deltopos") eb.deleteCharsToPos(op.a);
            else if (op.code == "insert") eb.insertText(op.s);
            else if (op.code == "wordpos") { r.query = true; r.qres = eb.getWordPosition(op.a); }
            else if (op.code == "getcurpos") { r.query = true; r.qres = gui::offsetByCodepoints(eb.value, eb.cursorPos, op.a); }
            if (!r.query) {
                r.valHex = hex(eb.value);
                r.cursor = eb.cursorPos;
                r.highlight = eb.highlightPos;
                r.hlHex = hex(eb.getHighlighted());
            }
            recs.push_back(std::move(r));
        }
        all.push_back(std::move(recs));
    }
    return all;
}
}  // namespace

int main(int argc, char** argv) {
    std::string casesPath;
    for (int a = 1; a < argc; ++a)
        if (std::string(argv[a]) == "--cases" && a + 1 < argc) casesPath = argv[++a];
    if (casesPath.empty()) { std::cerr << "usage: edit_box_parity --cases <tsv>\n"; return 2; }
    std::ifstream in(casesPath);
    if (!in) { std::cerr << "cannot open " << casesPath << "\n"; return 2; }

    auto all = replayAll();
    long long n = 0, bad = 0;
    int shown = 0;
    auto fail = [&](const std::string& w) { ++bad; if (shown++ < 30) std::cerr << "MISMATCH " << w << "\n"; };

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        auto p = split(line);
        const std::string& t = p[0];
        int s = I(p[1]), step = I(p[2]);
        if (s < 0 || s >= (int)all.size() || step < 0 || step >= (int)all[s].size()) { fail("range " + line); continue; }
        const Rec& r = all[s][step];
        if (t == "STATE") {
            ++n;
            if (r.query) { fail("STATE but query s=" + p[1] + " step=" + p[2]); continue; }
            if (r.valHex != p[3] || r.cursor != I(p[4]) || r.highlight != I(p[5]) || r.hlHex != p[6])
                fail("STATE s=" + p[1] + " step=" + p[2] + " val[" + r.valHex + " vs " + p[3] + "] cur[" +
                     std::to_string(r.cursor) + " vs " + p[4] + "] hl[" + std::to_string(r.highlight) +
                     " vs " + p[5] + "] sel[" + r.hlHex + " vs " + p[6] + "]");
        } else if (t == "QUERY") {
            ++n;
            if (!r.query) { fail("QUERY but state s=" + p[1] + " step=" + p[2]); continue; }
            if (r.qres != I(p[3]))
                fail("QUERY s=" + p[1] + " step=" + p[2] + " [" + std::to_string(r.qres) + " vs " + p[3] + "]");
        }
    }
    std::cout << "EditBox checks=" << n << " mismatches=" << bad << "\n";
    return bad == 0 ? 0 : 1;
}
