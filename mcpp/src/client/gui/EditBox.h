#pragma once

// 1:1 port of the FONT-INDEPENDENT text-editing core of net.minecraft.client.gui.components.EditBox
// (EditBox.java) — cursor / selection / word navigation / insert / delete over the value string. In
// the real class scrollTo()/updateTextPosition() are guarded by `font != null`, so with a null font
// (display-only state) the entire edit surface is pure UTF-16 index logic. Certified by edit_box_parity.
//
// CRITICAL 1:1 detail: indices are Java String char offsets, i.e. UTF-16 CODE UNITS (a surrogate pair
// counts as length 2). This port therefore stores value as std::u16string and treats every index as a
// char16_t offset — matching value.length()/charAt/substring exactly. Ports Util.offsetByCodepoints
// (surrogate-aware codepoint stepping) and StringUtil.filterText (insertText's input filter).
//
// Captured behaviors: getWordPosition (forward indexOf(' ')+strip-spaces / backward strip-then-word),
// setCursorPosition/setHighlightPos clamp to [0,length], insertText replace-selection + maxLength +
// high-surrogate-boundary trim, deleteCharsToPos / deleteWords (selection -> insertText("") else range
// delete), setValue maxLength truncate + cursor-to-end + collapse highlight.

#include <algorithm>
#include <string>

namespace mc::gui {

inline bool u16IsHighSurrogate(char16_t c) { return c >= 0xD800 && c <= 0xDBFF; }
inline bool u16IsLowSurrogate(char16_t c) { return c >= 0xDC00 && c <= 0xDFFF; }

// net.minecraft.util.Util.offsetByCodepoints(String, int pos, int offset).
inline int offsetByCodepoints(const std::u16string& input, int pos, int offset) {
    int length = static_cast<int>(input.size());
    if (offset >= 0) {
        for (int i = 0; pos < length && i < offset; i++) {
            char16_t cur = input[pos++];
            if (u16IsHighSurrogate(cur) && pos < length && u16IsLowSurrogate(input[pos])) pos++;
        }
    } else {
        for (int i = offset; pos > 0 && i < 0; i++) {
            pos--;
            if (u16IsLowSurrogate(input[pos]) && pos > 0 && u16IsHighSurrogate(input[pos - 1])) pos--;
        }
    }
    return pos;
}

// StringUtil.filterText(input, false): keep c where c != 167 && c >= 32 && c != 127.
inline std::u16string filterText(const std::u16string& input) {
    std::u16string out;
    for (char16_t c : input)
        if (c != 167 && c >= 32 && c != 127) out.push_back(c);
    return out;
}

struct EditBoxLogic {
    std::u16string value;
    int maxLength = 32;
    int cursorPos = 0;
    int highlightPos = 0;

    int length() const { return static_cast<int>(value.size()); }
    static int clampI(int v, int lo, int hi) { return std::min(std::max(v, lo), hi); }

    // scrollTo()/updateTextPosition() are font-gated no-ops (font == null) -> omitted.
    void setCursorPosition(int pos) { cursorPos = clampI(pos, 0, length()); }
    void setHighlightPos(int pos) { highlightPos = clampI(pos, 0, length()); }

    void moveCursorTo(int pos, bool extendSelection) {
        setCursorPosition(pos);
        if (!extendSelection) setHighlightPos(cursorPos);
    }
    int getCursorPos(int dir) const { return offsetByCodepoints(value, cursorPos, dir); }
    void moveCursor(int dir, bool shift) { moveCursorTo(getCursorPos(dir), shift); }
    void moveCursorToStart(bool shift) { moveCursorTo(0, shift); }
    void moveCursorToEnd(bool shift) { moveCursorTo(length(), shift); }

    std::u16string getHighlighted() const {
        int start = std::min(cursorPos, highlightPos), end = std::max(cursorPos, highlightPos);
        return value.substr(start, end - start);
    }

    int getWordPosition(int dir) const { return getWordPosition(dir, cursorPos, true); }
    int getWordPosition(int dir, int from, bool stripSpaces) const {
        int result = from;
        bool reverse = dir < 0;
        int absd = dir < 0 ? -dir : dir;
        for (int i = 0; i < absd; i++) {
            if (!reverse) {
                int len = length();
                auto idx = value.find(u' ', result);
                if (idx == std::u16string::npos) {
                    result = len;
                } else {
                    result = static_cast<int>(idx);
                    while (stripSpaces && result < len && value[result] == u' ') result++;
                }
            } else {
                while (stripSpaces && result > 0 && value[result - 1] == u' ') result--;
                while (result > 0 && value[result - 1] != u' ') result--;
            }
        }
        return result;
    }

    void setValue(const std::u16string& v) {
        value = (static_cast<int>(v.size()) > maxLength) ? v.substr(0, maxLength) : v;
        moveCursorToEnd(false);
        setHighlightPos(cursorPos);
    }

    void insertText(const std::u16string& input) {
        int start = std::min(cursorPos, highlightPos), end = std::max(cursorPos, highlightPos);
        int maxInsertionLength = maxLength - length() - (start - end);
        if (maxInsertionLength > 0) {
            std::u16string text = filterText(input);
            int insertionLength = static_cast<int>(text.size());
            if (maxInsertionLength < insertionLength) {
                if (u16IsHighSurrogate(text[maxInsertionLength - 1])) maxInsertionLength--;
                text = text.substr(0, maxInsertionLength);
                insertionLength = maxInsertionLength;
            }
            value = value.substr(0, start) + text + value.substr(end);  // StringBuilder.replace(start,end,text)
            setCursorPosition(start + insertionLength);
            setHighlightPos(cursorPos);
        }
    }

    void deleteCharsToPos(int pos) {
        if (!value.empty()) {
            if (highlightPos != cursorPos) {
                insertText(u"");
            } else {
                int start = std::min(pos, cursorPos), end = std::max(pos, cursorPos);
                if (start != end) {
                    value = value.substr(0, start) + value.substr(end);  // StringBuilder.delete(start,end)
                    setCursorPosition(start);
                    moveCursorTo(start, false);
                }
            }
        }
    }
    void deleteChars(int dir) { deleteCharsToPos(getCursorPos(dir)); }
    void deleteWords(int dir) {
        if (!value.empty()) {
            if (highlightPos != cursorPos) insertText(u"");
            else deleteCharsToPos(getWordPosition(dir));
        }
    }

    void setMaxLength(int ml) {
        maxLength = ml;
        if (length() > maxLength) value = value.substr(0, maxLength);
    }
};

}  // namespace mc::gui
