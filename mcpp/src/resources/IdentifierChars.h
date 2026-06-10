#pragma once
// Pure char/string validators of net.minecraft.resources.Identifier
// (the class formerly known as ResourceLocation; renamed in 26.1.2).
//
// VERBATIM port of the registry-free predicates from
//   26.1.2/src/net/minecraft/resources/Identifier.java
// Only the side-effect-free char/string validators are ported here; the
// registry/codec-coupled parse/read/StreamCodec is intentionally NOT ported
// (it needs Brigadier StringReader, DataResult, IdentifierException, the
// network StreamCodec, etc.).
//
// Java chars are UTF-16 code units; these predicates only ever accept ASCII
// ([a-z0-9_./-] subset), so a single `char` argument with the high bit unset is
// an exact model. To match Java's `char` (unsigned 16-bit) comparison semantics
// for the full sweep (0..127 and beyond), the C++ side takes an `int` codeunit
// in 0..0xFFFF, mirroring `String.charAt(i)` returning a `char`.
//
// Java source (Identifier.java):
//   216  public static boolean isAllowedInIdentifier(final char c) {
//   217     return c >= '0' && c <= '9' || c >= 'a' && c <= 'z' || c == '_'
//             || c == ':' || c == '/' || c == '.' || c == '-';
//   220  public static boolean isValidPath(final String path) {
//   221     for (int i = 0; i < path.length(); i++)
//   222        if (!validPathChar(path.charAt(i))) return false;
//   227     return true;
//   230  public static boolean isValidNamespace(final String namespace) {
//   231     if (namespace.equals("..")) return false;
//   235     for (int i = 0; i < namespace.length(); i++)
//   236        if (!validNamespaceChar(namespace.charAt(i))) return false;
//   241     return true;
//   252  public static boolean validPathChar(final char c) {
//   253     return c == '_' || c == '-' || c >= 'a' && c <= 'z'
//             || c >= '0' && c <= '9' || c == '/' || c == '.';
//   256  private static boolean validNamespaceChar(final char c) {
//   257     return c == '_' || c == '-' || c >= 'a' && c <= 'z'
//             || c >= '0' && c <= '9' || c == '.';

#include <string>
#include <string_view>

namespace mc::resources {

// Identifier.validPathChar(char) — Identifier.java:252-254.
// `cu` is a UTF-16 code unit (java char) in 0..0xFFFF.
inline bool validPathChar(int cu) {
    char16_t c = static_cast<char16_t>(cu);
    return c == u'_' || c == u'-' || (c >= u'a' && c <= u'z')
        || (c >= u'0' && c <= u'9') || c == u'/' || c == u'.';
}

// Identifier.validNamespaceChar(char) — Identifier.java:256-258.
inline bool validNamespaceChar(int cu) {
    char16_t c = static_cast<char16_t>(cu);
    return c == u'_' || c == u'-' || (c >= u'a' && c <= u'z')
        || (c >= u'0' && c <= u'9') || c == u'.';
}

// Identifier.isAllowedInIdentifier(char) — Identifier.java:216-218.
inline bool isAllowedInIdentifier(int cu) {
    char16_t c = static_cast<char16_t>(cu);
    return (c >= u'0' && c <= u'9') || (c >= u'a' && c <= u'z') || c == u'_'
        || c == u':' || c == u'/' || c == u'.' || c == u'-';
}

// Identifier.isValidPath(String) — Identifier.java:220-228.
// Operates over UTF-16 code units. The test inputs here are ASCII, so we treat
// the std::string as a sequence of bytes 0..255 (each byte == its code unit).
inline bool isValidPath(std::string_view path) {
    for (unsigned char ch : path) {
        if (!validPathChar(static_cast<int>(ch))) return false;
    }
    return true;
}

// Identifier.isValidNamespace(String) — Identifier.java:230-242.
inline bool isValidNamespace(std::string_view ns) {
    if (ns == "..") return false;
    for (unsigned char ch : ns) {
        if (!validNamespaceChar(static_cast<int>(ch))) return false;
    }
    return true;
}

} // namespace mc::resources
