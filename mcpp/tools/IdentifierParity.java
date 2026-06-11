// Ground truth for the STRING-LEVEL surface of
//   net.minecraft.resources.Identifier
// (26.1.2/src/net/minecraft/resources/Identifier.java — the class formerly named
// ResourceLocation). We drive the REAL net.minecraft.resources.Identifier and emit
// a TSV that mcpp/src/resources/IdentifierParityTest.cpp replays against the port.
//
// Surface covered (parse/build/compare/serialize):
//   tryParse(String)                     -> @Nullable      (return-null on invalid)
//   parse(String)                        -> throws         (IdentifierException)
//   fromNamespaceAndPath(ns, path)       -> throws
//   withDefaultNamespace(path)           -> throws  (validates path only)
//   tryBuild(ns, path)                   -> @Nullable
//   getNamespace()/getPath()/toString()
//   compareTo(Identifier)                -> sign over all ordered pairs
//
// The pure char/string validators (validPathChar/validNamespaceChar/isValidPath/
// isValidNamespace) already have a dedicated gate (rl_chars_parity /
// ResourceLocationCharsParity.java); they are NOT duplicated here.
//
// Row formats (tab-separated). String fields are base64 of the UTF-8 bytes so
// arbitrary inputs survive the ASCII TSV transport; flags are decimal 0/1; the
// compareTo sign is one of -1/0/1.
//
//   PARSE   <input_b64>  <isNull 0/1>  <namespace_b64|->  <path_b64|->  <toString_b64|->
//   BUILD   <method>     <ns_b64>  <path_b64>  <threw 0/1>  <namespace_b64|->  <path_b64|->  <toString_b64|->
//     method in { fromNamespaceAndPath, withDefaultNamespace }
//     (withDefaultNamespace ignores ns_b64 for input, but we still emit it for symmetry)
//   TRYBUILD <ns_b64>  <path_b64>  <isNull 0/1>  <namespace_b64|->  <path_b64|->  <toString_b64|->
//   CMP     <a_index>  <b_index>  <sign -1/0/1>
//   CMPID   <index>  <toString_b64>     (the fixed Identifier table, for reference)

import java.nio.charset.StandardCharsets;
import java.util.Base64;

import net.minecraft.resources.Identifier;

public class IdentifierParity {
    static final java.io.PrintStream O = System.out;

    public static void main(String[] args) throws Exception {
        net.minecraft.SharedConstants.tryDetectVersion();
        net.minecraft.server.Bootstrap.bootStrap();

        // ---- (A) tryParse / parse over a wide battery of inputs ----
        String[] parseInputs = {
            "minecraft:stone",          // explicit default ns
            "stone",                    // bare path -> default ns
            "ns:path",                  // custom ns
            "realms:foo",
            "a:b",
            "Ns:Path",                  // uppercase ns AND path -> invalid
            "NS:path",                  // uppercase ns -> invalid
            "ns:Path",                  // uppercase path -> invalid
            "a:b:c",                    // multi-colon: ns="a", path="b:c" -> ':' illegal in path
            ":path",                    // leading colon -> default ns, path="path"
            ":",                        // leading colon, empty path -> "minecraft:"  (empty path valid)
            "ns:",                      // empty path with ns -> valid (empty path)
            "",                         // empty -> default ns, empty path -> valid
            "minecraft:",               // empty path -> valid
            " minecraft:stone",         // leading space -> ns=" minecraft" invalid
            "minecraft:stone ",         // trailing space in path -> invalid
            "foo/bar",                  // '/' valid in path
            "ns:foo/bar/baz",
            "item.cool.down",           // dots ok
            "my-mod:thing-1",           // '-' ok
            "under_score:a_b_c",        // '_' ok
            "mod123:path456",           // digits ok
            "ns:#hash",                 // '#' illegal in path
            "ns:with space",            // space illegal in path
            "#ns:path",                 // '#' illegal in ns
            "ns#:path",                 // '#' illegal in ns
            "ns:path#",                 // '#' illegal in path
            "..:path",                  // ns ".." special-cased invalid
            "ns:..",                    // path ".." -> valid (two dots)
            ".:.",                      // ns ".", path "." -> valid
            "minecraft:tnt",
            "minecraft:air",
            "0123456789:0123456789",
            "abcdefghijklmnopqrstuvwxyz:slash/dot.dash-under_",
            "ABC:def",                  // uppercase ns
            "tab\tns:path",             // control char in ns
            "ns:tab\tpath",             // control char in path
            "café:x",                   // non-ascii in ns
            "x:café",                   // non-ascii in path
            "minecraft:foo:bar",        // ns="minecraft", path="foo:bar" -> ':' illegal in path
        };

        for (String in : parseInputs) {
            Identifier id = Identifier.tryParse(in);
            emitParse(in, id);
        }

        // ---- (B) fromNamespaceAndPath / withDefaultNamespace (THROW on invalid) ----
        // pairs of (namespace, path)
        String[][] buildPairs = {
            {"minecraft", "stone"},
            {"ns", "path"},
            {"realms", "foo"},
            {"my-mod", "thing/sub.thing-1"},
            {"NS", "path"},             // uppercase ns -> throws in fromNamespaceAndPath
            {"ns", "Path"},             // uppercase path -> throws
            {"..", "path"},             // ns ".." -> throws
            {"ns", "with space"},       // space in path -> throws
            {"ns", "foo:bar"},          // ':' in path -> throws
            {"ns", ""},                 // empty path -> ok
            {"", "path"},               // empty ns -> ok (empty namespace is valid)
            {"minecraft", "with/slash.and-dash_0"},
            {"ns", "café"},             // non-ascii path -> throws
            {"café", "path"},           // non-ascii ns -> throws
        };

        for (String[] p : buildPairs) {
            // fromNamespaceAndPath(ns, path)
            emitBuild("fromNamespaceAndPath", p[0], p[1], () -> Identifier.fromNamespaceAndPath(p[0], p[1]));
            // withDefaultNamespace(path) — validates path only; ns input field is p[1] semantics,
            // but we record the input ns as p[0] purely for the row's symmetry (it is ignored).
            emitBuild("withDefaultNamespace", p[0], p[1], () -> Identifier.withDefaultNamespace(p[1]));
            // tryBuild(ns, path)
            Identifier tb = Identifier.tryBuild(p[0], p[1]);
            emitTryBuild(p[0], p[1], tb);
        }

        // ---- (C) compareTo over all ordered pairs of a fixed valid table ----
        // Hand-picked to exercise BOTH branches of compareTo: equal paths with
        // differing namespaces (forces the namespace tie-break) and differing paths.
        Identifier[] table = {
            Identifier.fromNamespaceAndPath("minecraft", "stone"),
            Identifier.fromNamespaceAndPath("minecraft", "air"),
            Identifier.fromNamespaceAndPath("minecraft", "dirt"),
            Identifier.fromNamespaceAndPath("realms", "stone"),     // same path as [0], diff ns
            Identifier.fromNamespaceAndPath("aaa", "stone"),        // same path as [0]/[3], diff ns
            Identifier.fromNamespaceAndPath("zzz", "air"),          // same path as [1], diff ns
            Identifier.fromNamespaceAndPath("minecraft", "stones"), // path prefix relation w/ "stone"
            Identifier.fromNamespaceAndPath("minecraft", "ston"),
            Identifier.fromNamespaceAndPath("a", "a"),
            Identifier.fromNamespaceAndPath("b", "a"),              // same path as [8], diff ns
            Identifier.fromNamespaceAndPath("a", "b"),
        };

        for (int i = 0; i < table.length; i++) {
            O.print("CMPID\t");
            O.print(i);
            O.print('\t');
            O.print(b64(table[i].toString()));
            O.print('\n');
        }

        for (int i = 0; i < table.length; i++) {
            for (int j = 0; j < table.length; j++) {
                int sign = Integer.signum(table[i].compareTo(table[j]));
                O.print("CMP\t");
                O.print(i);
                O.print('\t');
                O.print(j);
                O.print('\t');
                O.print(sign);
                O.print('\n');
            }
        }
    }

    interface IdSupplier { Identifier get(); }

    static void emitParse(String in, Identifier id) {
        O.print("PARSE\t");
        O.print(b64(in));
        O.print('\t');
        if (id == null) {
            O.print("1\t-\t-\t-");
        } else {
            O.print("0\t");
            O.print(b64(id.getNamespace()));
            O.print('\t');
            O.print(b64(id.getPath()));
            O.print('\t');
            O.print(b64(id.toString()));
        }
        O.print('\n');
    }

    static void emitBuild(String method, String ns, String path, IdSupplier sup) {
        boolean threw = false;
        Identifier id = null;
        try {
            id = sup.get();
        } catch (RuntimeException e) {
            // IdentifierException extends RuntimeException; assert errors only fire
            // with -ea, which we do not pass, so the throw path is the validating one.
            threw = true;
        }
        O.print("BUILD\t");
        O.print(method);
        O.print('\t');
        O.print(b64(ns));
        O.print('\t');
        O.print(b64(path));
        O.print('\t');
        if (threw) {
            O.print("1\t-\t-\t-");
        } else {
            O.print("0\t");
            O.print(b64(id.getNamespace()));
            O.print('\t');
            O.print(b64(id.getPath()));
            O.print('\t');
            O.print(b64(id.toString()));
        }
        O.print('\n');
    }

    static void emitTryBuild(String ns, String path, Identifier id) {
        O.print("TRYBUILD\t");
        O.print(b64(ns));
        O.print('\t');
        O.print(b64(path));
        O.print('\t');
        if (id == null) {
            O.print("1\t-\t-\t-");
        } else {
            O.print("0\t");
            O.print(b64(id.getNamespace()));
            O.print('\t');
            O.print(b64(id.getPath()));
            O.print('\t');
            O.print(b64(id.toString()));
        }
        O.print('\n');
    }

    static String b64(String s) {
        return Base64.getEncoder().encodeToString(s.getBytes(StandardCharsets.UTF_8));
    }
}
