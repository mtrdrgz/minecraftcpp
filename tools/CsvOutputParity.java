import java.io.StringWriter;
import java.util.ArrayList;
import java.util.List;
import net.minecraft.util.CsvOutput;

// Ground truth for mcpp/src/util/CsvOutput.h. Drives the REAL
// net.minecraft.util.CsvOutput through its public API:
//   CsvOutput.builder().addColumn(h)... .build(writer)  // writes the header line
//   csv.writeRow(Object... values)                      // one data line per call
// into a java.io.StringWriter, then dumps the produced text.
//
// CsvOutput formats each cell with org.apache.commons.lang3.StringEscapeUtils.escapeCsv,
// joins cells with ",", and terminates every line with "\r\n". Because the produced text
// contains commas, quotes, CR and LF, every emitted string is encoded as the
// concatenation of %02x per byte of its UTF-8 (ASCII) encoding so it round-trips through
// a tab-separated row. Sentinel "EMPTY" => the empty document/cell.
//
// Row formats (TAG \t inputs... \t outputs...):
//   ESCAPE   inHex    outHex          escapeCsv(in)                       (single cell)
//   DOC      nCols\theaderHex(|...)\trowsSpec   docHex
//                                     full document text for builder(headers)+rows
//   NULLCELL inHex    outHex          getStringValue when the Object ref is null -> "[null]"
//                                     (escapeCsv("[null]"))
//   COLCHECK nCols  gotCols  threw(0/1)  writeRow column-count validation result
@SuppressWarnings("deprecation")  // class-level: a deprecated API usage outside main() still triggers javac's file-level "Note:", which the strict GT runner treats as fatal
public class CsvOutputParity {
    static final java.io.PrintStream O = System.out;

    static String enc(byte[] b) {
        if (b.length == 0) return "EMPTY";
        StringBuilder sb = new StringBuilder();
        for (byte x : b) sb.append(String.format("%02x", x & 0xFF));
        return sb.toString();
    }

    static String enc(String s) {
        return enc(s.getBytes(java.nio.charset.StandardCharsets.UTF_8));
    }

    // escapeCsv via the REAL commons-lang3 used by CsvOutput.getStringValue.
    static String escapeCsv(String s) {
        return org.apache.commons.lang3.StringEscapeUtils.escapeCsv(s);
    }

    // Build a document from headers + a list of rows (each row a String[] of cells)
    // using the real CsvOutput, returning the produced text.
    static String doc(List<String> headers, List<String[]> rows) throws Exception {
        StringWriter w = new StringWriter();
        CsvOutput.Builder bld = CsvOutput.builder();
        for (String h : headers) bld.addColumn(h);
        CsvOutput csv = bld.build(w);
        for (String[] r : rows) {
            // writeRow takes Object...; a String[] is passed straight through.
            csv.writeRow((Object[]) r);
        }
        return w.toString();
    }

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ---- escapeCsv single-cell battery -------------------------------------------
        String[] cells = {
            "",                       // empty -> empty
            "a",                      // plain
            "abc",                    // plain
            "hello world",            // space, no special -> unchanged
            "no_special-chars.123",   // unchanged
            ",",                      // delimiter -> quoted
            "a,b",                    // delimiter -> quoted
            "a,b,c",                  // multiple delimiters
            "\"",                     // a single quote -> "" wrapped + doubled
            "a\"b",                   // embedded quote
            "\"quoted\"",             // leading+trailing quotes
            "\"\"",                   // two quotes
            "a\"\"b",                 // double embedded quote
            "line\r",                 // CR
            "line\n",                 // LF
            "line\r\nmore",           // CRLF
            "x\ry\nz",                // both CR and LF
            "tab\there",              // a literal tab is NOT a search char -> unchanged
            "comma, and \"quote\"",   // comma + quote
            "\",\"",                  // quote+comma+quote
            "[null]",                 // the null-substitute literal (unchanged)
            "trailing,",              // trailing comma
            ",leading",               // leading comma
            "\r",                     // bare CR
            "\n",                     // bare LF
            "\r\n",                   // bare CRLF
            "mix,\"\r\n",             // all four search chars together
            "0123456789",             // digits
            "!@#$%^&*()_+-=[]{}|;:'<>?/~`",  // punctuation, none are search chars
            "  spaces  ",             // surrounding spaces -> unchanged
        };
        for (String c : cells) {
            O.println("ESCAPE\t" + enc(c) + "\t" + enc(escapeCsv(c)));
        }

        // ---- getStringValue null path: escapeCsv("[null]") ---------------------------
        // CsvOutput.getStringValue(null) == escapeCsv("[null]").
        O.println("NULLCELL\t" + enc("[null]") + "\t" + enc(escapeCsv("[null]")));

        // ---- full-document batteries via the real CsvOutput --------------------------
        // Each entry: headers + rows.
        List<Object[]> docs = new ArrayList<>();

        // 1) single column, plain values
        docs.add(new Object[]{
            List.of("name"),
            List.of(new String[]{"alice"}, new String[]{"bob"})
        });
        // 2) two columns, header has no specials, plain rows
        docs.add(new Object[]{
            List.of("id", "value"),
            List.of(new String[]{"1", "x"}, new String[]{"2", "y"}, new String[]{"3", "z"})
        });
        // 3) cells that need quoting (comma, quote, newline)
        docs.add(new Object[]{
            List.of("a", "b", "c"),
            java.util.Collections.<String[]>singletonList(new String[]{"p,q", "r\"s", "t\r\nu"})
        });
        // 4) header itself needs quoting
        docs.add(new Object[]{
            List.of("col,1", "col\"2"),
            java.util.Collections.<String[]>singletonList(new String[]{"v1", "v2"})
        });
        // 5) empty cells and empty header cell
        docs.add(new Object[]{
            List.of("", "h"),
            List.of(new String[]{"", ""}, new String[]{"x", ""})
        });
        // 6) header only, no rows
        docs.add(new Object[]{
            List.of("only", "header", "row"),
            List.<String[]>of()
        });
        // 7) single-column header with quote in it
        docs.add(new Object[]{
            List.of("he said \"hi\""),
            java.util.Collections.<String[]>singletonList(new String[]{"a\"b"})
        });
        // 8) many columns, mixed
        docs.add(new Object[]{
            List.of("c1", "c2", "c3", "c4", "c5"),
            java.util.Collections.<String[]>singletonList(new String[]{"1", "two", "3,3", "fo\"ur", "5\n5"})
        });

        int di = 0;
        for (Object[] d : docs) {
            @SuppressWarnings("unchecked")
            List<String> headers = (List<String>) d[0];
            @SuppressWarnings("unchecked")
            List<String[]> rows = (List<String[]>) d[1];
            String produced = doc(headers, rows);
            O.println("DOC\t" + di + "\t" + enc(produced));
            di++;
        }

        // ---- writeRow column-count validation ----------------------------------------
        // build a 2-column CsvOutput and feed rows of varying widths.
        for (int got = 0; got <= 4; got++) {
            StringWriter w = new StringWriter();
            CsvOutput csv = CsvOutput.builder().addColumn("a").addColumn("b").build(w);
            String[] row = new String[got];
            for (int k = 0; k < got; k++) row[k] = "v" + k;
            int threw = 0;
            try {
                csv.writeRow((Object[]) row);
            } catch (IllegalArgumentException e) {
                threw = 1;
            }
            O.println("COLCHECK\t2\t" + got + "\t" + threw);
        }
    }
}
