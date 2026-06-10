// 1:1 C++ port of net.minecraft.util.CsvOutput (Minecraft 26.1.2).
//
// CsvOutput is a tiny CSV writer: a fixed header row, then any number of data
// rows, joined with the field separator "," and terminated with "\r\n".
// Each cell is run through org.apache.commons.lang3.StringEscapeUtils.escapeCsv
// (commons-lang3 3.19.0), whose exact, verbatim semantics are reproduced here.
//
// Source (26.1.2/src/net/minecraft/util/CsvOutput.java):
//   private static final String LINE_SEPARATOR = "\r\n";
//   private static final String FIELD_SEPARATOR = ",";          // unused: writeLine joins with the literal ","
//   private void writeLine(Stream<...> values):
//     output.write(values.map(CsvOutput::getStringValue).collect(Collectors.joining(",")) + "\r\n");
//   private static String getStringValue(Object value):
//     return StringEscapeUtils.escapeCsv(value != null ? value.toString() : "[null]");
//   writeRow(Object... values): throws IllegalArgumentException if values.length != columnCount.
//   Builder.addColumn(header) appends; build(writer) constructs CsvOutput which immediately
//     writes the header row via writeLine(headers.stream()).
//
// escapeCsv (commons-lang3 StringEscapeUtils$CsvEscaper, disassembled verbatim):
//   CSV_SEARCH_CHARS = { ',' , '"' , '\r' , '\n' }   (decimal 44,34,13,10)
//   if (StringUtils.containsNone(input, CSV_SEARCH_CHARS)) -> write input unchanged
//   else -> write '"' , write input with every '"' replaced by '""' (plain, case-sensitive,
//           Strings.CS.replace) , write '"'.
//   (null input -> null result; never hit by CsvOutput since getStringValue substitutes "[null]".)
//
// ASCII-only port: CsvOutput is a pure string formatter; we model the values as
// std::string (the parity battery uses ASCII rows only, matching the assignment).

#ifndef MCPP_UTIL_CSV_OUTPUT_H
#define MCPP_UTIL_CSV_OUTPUT_H

#include <stdexcept>
#include <string>
#include <vector>

namespace mc::util {

class CsvOutput {
public:
    // The four characters that force quoting, in the exact order of CSV_SEARCH_CHARS.
    static constexpr char CSV_DELIMITER = ',';   // 44
    static constexpr char CSV_QUOTE = '"';       // 34
    static constexpr char CR = '\r';             // 13
    static constexpr char LF = '\n';             // 10

    static const std::string LINE_SEPARATOR;   // "\r\n"
    static const std::string FIELD_SEPARATOR;  // ","

    // org.apache.commons.lang3.StringUtils.containsNone(cs, searchChars):
    // returns true iff none of the search chars occur in cs. (For our fixed
    // 4-char set we inline the membership test.)
    static bool containsNoneSearchChars(const std::string& cs) {
        for (char c : cs) {
            if (c == CSV_DELIMITER || c == CSV_QUOTE || c == CR || c == LF) return false;
        }
        return true;
    }

    // Plain, case-sensitive replace of all occurrences of `from` with `to`
    // (org.apache.commons.lang3.Strings.CS.replace). `from` here is always "\"".
    static std::string replaceAll(const std::string& s, const std::string& from,
                                  const std::string& to) {
        if (from.empty()) return s;
        std::string out;
        out.reserve(s.size());
        size_t pos = 0;
        for (;;) {
            size_t hit = s.find(from, pos);
            if (hit == std::string::npos) {
                out.append(s, pos, std::string::npos);
                break;
            }
            out.append(s, pos, hit - pos);
            out.append(to);
            pos = hit + from.size();
        }
        return out;
    }

    // StringEscapeUtils.escapeCsv(input) — verbatim CsvEscaper semantics.
    static std::string escapeCsv(const std::string& input) {
        if (containsNoneSearchChars(input)) {
            return input;  // write the string unchanged
        }
        // "" + replace('"' -> "\"\"") + ""
        std::string out;
        out.push_back(CSV_QUOTE);
        out += replaceAll(input, "\"", "\"\"");
        out.push_back(CSV_QUOTE);
        return out;
    }

    // getStringValue(value): value!=null ? escapeCsv(value.toString()) : escapeCsv("[null]").
    // The boolean `isNull` models the @Nullable Object reference being null.
    static std::string getStringValue(const std::string& value, bool isNull = false) {
        return escapeCsv(isNull ? std::string("[null]") : value);
    }

    // Builder: collects headers in order; build() emits the header line.
    class Builder {
    public:
        Builder& addColumn(const std::string& header) {
            headers_.push_back(header);
            return *this;
        }

        // Returns the constructed CsvOutput, whose `output` buffer already holds the
        // header line (constructor writes writeLine(headers.stream())).
        CsvOutput build() const { return CsvOutput(headers_); }

    private:
        std::vector<std::string> headers_;
    };

    static Builder builder() { return Builder(); }

    // writeRow(Object... values) — values modeled as (string, isNull) pairs.
    void writeRow(const std::vector<std::pair<std::string, bool>>& values) {
        if (static_cast<int>(values.size()) != columnCount_) {
            throw std::invalid_argument("Invalid number of columns, expected " +
                                        std::to_string(columnCount_) + ", but got " +
                                        std::to_string(values.size()));
        }
        writeLine(values);
    }

    // Convenience: a row of non-null string cells.
    void writeRow(const std::vector<std::string>& cells) {
        std::vector<std::pair<std::string, bool>> v;
        v.reserve(cells.size());
        for (const auto& c : cells) v.emplace_back(c, false);
        writeRow(v);
    }

    // The accumulated output text (the Writer's contents).
    const std::string& text() const { return output_; }
    int columnCount() const { return columnCount_; }

private:
    explicit CsvOutput(const std::vector<std::string>& headers)
        : columnCount_(static_cast<int>(headers.size())) {
        std::vector<std::pair<std::string, bool>> v;
        v.reserve(headers.size());
        for (const auto& h : headers) v.emplace_back(h, false);
        writeLine(v);
    }

    void writeLine(const std::vector<std::pair<std::string, bool>>& values) {
        // map(getStringValue).collect(joining(",")) + "\r\n"
        std::string line;
        for (size_t k = 0; k < values.size(); ++k) {
            if (k != 0) line += FIELD_SEPARATOR;
            line += getStringValue(values[k].first, values[k].second);
        }
        line += LINE_SEPARATOR;
        output_ += line;
    }

    std::string output_;
    int columnCount_;
};

inline const std::string CsvOutput::LINE_SEPARATOR = "\r\n";
inline const std::string CsvOutput::FIELD_SEPARATOR = ",";

}  // namespace mc::util

#endif  // MCPP_UTIL_CSV_OUTPUT_H
