// Ground truth for the PURE char/string validators of
// net.minecraft.resources.Identifier (the class formerly named
// ResourceLocation; renamed in 26.1.2). VERBATIM behavior from
//   26.1.2/src/net/minecraft/resources/Identifier.java
//
// Predicates covered (all side-effect-free, registry-free):
//   validPathChar(char)            (public)   Identifier.java:252
//   validNamespaceChar(char)       (PRIVATE)  Identifier.java:256  -> reflection
//   isAllowedInIdentifier(char)    (public)   Identifier.java:216
//   isValidPath(String)            (public)   Identifier.java:220
//   isValidNamespace(String)       (public)   Identifier.java:230
//
// NOTE: there is no isValidResourceLocation(String) in 26.1.2 — it does not
// exist on this class — so it is NOT emitted (see unportedMethods).
//
// Row formats (tab-separated):
//   CHAR  <codeunit-dec>  <validPathChar 0/1>  <validNamespaceChar 0/1>  <isAllowedInIdentifier 0/1>
//   PATH  <input-as-UTF8-HEX>  <isValidPath 0/1>
//   NS    <input-as-UTF8-HEX>  <isValidNamespace 0/1>
// String inputs are emitted as lowercase UTF-8 hex so they survive the ASCII
// TSV transport (the C++ side decodes the hex back to the byte string).
//
// All booleans are decimal 0/1. We call the REAL net.minecraft methods; the one
// private method (validNamespaceChar) is reached via reflection+setAccessible.
import java.lang.reflect.Method;
import java.nio.charset.StandardCharsets;

import net.minecraft.resources.Identifier;

public class ResourceLocationCharsParity {
    static final java.io.PrintStream O = System.out;

    @SuppressWarnings({"unchecked", "deprecation"})
    public static void main(String[] args) throws Exception {
        // These predicates touch no registry, but bootstrapping is cheap and
        // guards against any static-init "Not bootstrapped" surprises.
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // validNamespaceChar is private static — reflect it.
        Method mValidNamespaceChar =
            Identifier.class.getDeclaredMethod("validNamespaceChar", char.class);
        mValidNamespaceChar.setAccessible(true);

        // (A) Char sweep over 0..127 (full ASCII) plus a few > 127 code units
        // (Java chars are unsigned 16-bit; the high code units pin down the
        // comparison logic past the ASCII boundary). Java compares the raw char,
        // so we feed each code unit as (char) cu.
        int[] extras = { 0x80, 0xC4 /*Ä*/, 0xFF /*ÿ*/, 0x100 /*Ā*/, 0x7FFF, 0xFFFF /*max char*/ };
        int[] sweep = new int[128 + extras.length];
        for (int i = 0; i < 128; i++) sweep[i] = i;
        for (int i = 0; i < extras.length; i++) sweep[128 + i] = extras[i];

        for (int cu : sweep) {
            char c = (char) cu;
            boolean vp = Identifier.validPathChar(c);
            boolean vn = (Boolean) mValidNamespaceChar.invoke(null, c);
            boolean ai = Identifier.isAllowedInIdentifier(c);
            O.print("CHAR\t");
            O.print((int) c);   // the actual 16-bit code unit Java compared
            O.print('\t');
            O.print(vp ? 1 : 0);
            O.print('\t');
            O.print(vn ? 1 : 0);
            O.print('\t');
            O.print(ai ? 1 : 0);
            O.print('\n');
        }

        // (B) Fixed-string battery for isValidPath / isValidNamespace.
        String[] strings = {
            "",                         // empty -> valid (loop runs zero times)
            "stone",
            "minecraft",
            "realms",
            "foo/bar/baz",              // '/' valid in path, INVALID in namespace
            "item.cool.down",           // dots ok in both
            "cooldown_group_1",
            "my-mod",
            "mod123",
            "path456",
            "a",
            "A",                        // uppercase -> invalid (both)
            "Stone",                    // uppercase first char
            "with space",               // space -> invalid
            "with:colon",               // ':' -> invalid in path AND namespace
            "..",                       // special-cased FALSE for namespace; for path
                                        // it's two dots -> valid path chars
            ".",
            "...",
            "tilde~",                   // '~' invalid
            "plus+",                    // '+' invalid
            "under_score",
            "dash-",
            "slash/",
            "trailing.",
            "0123456789",
            "abcdefghijklmnopqrstuvwxyz",
            "ABCDEFGHIJKLMNOPQRSTUVWXYZ", // all uppercase -> invalid
            "unicodeÄ",                 // non-ASCII -> invalid
            "tab\tchar",                // control char -> invalid
            "namespace_with_/slash",    // slash invalid in ns
        };

        for (String s : strings) {
            boolean vpath = Identifier.isValidPath(s);
            String hex = hex(s.getBytes(StandardCharsets.UTF_8));
            O.print("PATH\t");
            O.print(hex);
            O.print('\t');
            O.print(vpath ? 1 : 0);
            O.print('\n');
        }

        for (String s : strings) {
            boolean vns = Identifier.isValidNamespace(s);
            String hex = hex(s.getBytes(StandardCharsets.UTF_8));
            O.print("NS\t");
            O.print(hex);
            O.print('\t');
            O.print(vns ? 1 : 0);
            O.print('\n');
        }
    }

    static String hex(byte[] b) {
        StringBuilder sb = new StringBuilder(b.length * 2);
        for (byte x : b) sb.append(String.format("%02x", x & 0xff));
        return sb.toString();
    }
}
