// Ground truth for the font-independent text-editing core of net.minecraft.client.gui.components.EditBox.
// Drives the REAL EditBox with a null Font (scrollTo/updateTextPosition become no-ops), replaying a
// shared op script per scenario and emitting state after each step. Indices are UTF-16 code units;
// strings are transmitted as %04x-per-char hex. highlightPos is read via reflection (no public getter);
// getCursorPos's private helper == the public Util.offsetByCodepoints, so queries use that directly.
//
//   tools/run_groundtruth.ps1 -Tool EditBoxParity -Out mcpp/build/edit_box.tsv
//
// Rows:
//   STATE <s> <step> <valueHex> <cursor> <highlight> <highlightedHex>
//   QUERY <s> <step> <result>

import java.lang.reflect.Field;
import net.minecraft.client.gui.Font;
import net.minecraft.client.gui.components.EditBox;
import net.minecraft.network.chat.Component;
import net.minecraft.server.Bootstrap;
import net.minecraft.util.Util;
import net.minecraft.SharedConstants;

public class EditBoxParity {
    static final java.io.PrintStream O = System.out;

    record Op(String code, String s, int a, int b) {}
    static Op maxlen(int n) { return new Op("maxlen", "", n, 0); }
    static Op setval(String s) { return new Op("setval", s, 0, 0); }
    static Op setcur(int n) { return new Op("setcur", "", n, 0); }
    static Op sethl(int n) { return new Op("sethl", "", n, 0); }
    static Op movecur(int d, int sh) { return new Op("movecur", "", d, sh); }
    static Op moveto(int p, int e) { return new Op("moveto", "", p, e); }
    static Op movestart(int sh) { return new Op("movestart", "", sh, 0); }
    static Op moveend(int sh) { return new Op("moveend", "", sh, 0); }
    static Op delchars(int d) { return new Op("delchars", "", d, 0); }
    static Op delwords(int d) { return new Op("delwords", "", d, 0); }
    static Op deltopos(int p) { return new Op("deltopos", "", p, 0); }
    static Op insert(String s) { return new Op("insert", s, 0, 0); }
    static Op wordpos(int d) { return new Op("wordpos", "", d, 0); }
    static Op getcurpos(int d) { return new Op("getcurpos", "", d, 0); }

    static final String EMOJI = "😀";  // U+1F600 surrogate pair

    static final Op[][] SCEN = {
        { maxlen(100), setval("hello world foo"), movestart(0), wordpos(1), movecur(1, 0),
          moveto(11, 0), delwords(-1), moveend(0), delwords(-1) },
        { maxlen(20), setval("abcdef"), setcur(2), sethl(4), insert("XYZ"), setcur(2), sethl(5),
          delchars(-1), setcur(0), sethl(0), delchars(1) },
        { maxlen(50), setval("a" + EMOJI + "b"), movestart(0), movecur(1, 0), getcurpos(1),
          movecur(1, 0), movecur(1, 0), movestart(0), moveto(1, 0), delchars(1) },
        { maxlen(3), setval("hello"), setcur(0), sethl(0), insert(EMOJI), maxlen(10),
          setcur(3), sethl(3), insert("XY" + EMOJI) },
        { maxlen(100), setval("foo   bar baz"), movestart(0), wordpos(1), delwords(1),
          moveend(0), delwords(-1), wordpos(-1) },
        { maxlen(5), setval("ab"), setcur(2), sethl(2), insert("cd" + EMOJI + "ef") }
    };

    static String hex(String s) {
        if (s.isEmpty()) return ".";
        StringBuilder b = new StringBuilder();
        for (int i = 0; i < s.length(); i++) b.append(String.format("%04x", (int) s.charAt(i)));
        return b.toString();
    }

    static int highlightOf(EditBox eb, Field hp) {
        try { return hp.getInt(eb); } catch (IllegalAccessException e) { throw new RuntimeException(e); }
    }

    public static void main(String[] args) throws Exception {
        SharedConstants.tryDetectVersion();
        Bootstrap.bootStrap();
        Field hp = EditBox.class.getDeclaredField("highlightPos");
        hp.setAccessible(true);

        for (int s = 0; s < SCEN.length; s++) {
            EditBox eb = new EditBox((Font) null, 0, 0, 100, 20, Component.empty());
            int step = 0;
            for (Op op : SCEN[s]) {
                boolean query = false;
                int qres = 0;
                switch (op.code()) {
                    case "maxlen" -> eb.setMaxLength(op.a());
                    case "setval" -> eb.setValue(op.s());
                    case "setcur" -> eb.setCursorPosition(op.a());
                    case "sethl" -> eb.setHighlightPos(op.a());
                    case "movecur" -> eb.moveCursor(op.a(), op.b() != 0);
                    case "moveto" -> eb.moveCursorTo(op.a(), op.b() != 0);
                    case "movestart" -> eb.moveCursorToStart(op.a() != 0);
                    case "moveend" -> eb.moveCursorToEnd(op.a() != 0);
                    case "delchars" -> eb.deleteChars(op.a());
                    case "delwords" -> eb.deleteWords(op.a());
                    case "deltopos" -> eb.deleteCharsToPos(op.a());
                    case "insert" -> eb.insertText(op.s());
                    case "wordpos" -> { query = true; qres = eb.getWordPosition(op.a()); }
                    case "getcurpos" -> { query = true; qres = Util.offsetByCodepoints(eb.getValue(), eb.getCursorPosition(), op.a()); }
                }
                if (query) {
                    O.println("QUERY\t" + s + "\t" + step + "\t" + qres);
                } else {
                    O.println("STATE\t" + s + "\t" + step + "\t" + hex(eb.getValue()) + "\t"
                            + eb.getCursorPosition() + "\t" + highlightOf(eb, hp) + "\t" + hex(eb.getHighlighted()));
                }
                step++;
            }
        }
    }
}
