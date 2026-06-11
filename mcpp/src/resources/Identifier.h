#pragma once
// 1:1 C++ port of the STRING-LEVEL surface of
//   net.minecraft.resources.Identifier
// (26.1.2/src/net/minecraft/resources/Identifier.java — the repo's name for the
// class formerly known as ResourceLocation).
//
// VERBATIM translation of the parse/build/compare/serialize surface:
//   - DEFAULT_NAMESPACE = "minecraft", NAMESPACE_SEPARATOR = ':'
//   - fromNamespaceAndPath / withDefaultNamespace / parse         (THROW on invalid)
//   - tryParse / tryBuild / tryBySeparator                        (return null/nullopt)
//   - getNamespace / getPath / toString  ("namespace:path")
//   - compareTo  (PATH first, then NAMESPACE — Identifier.java:142-149)
//   - equals / hashCode  (Java String.hashCode semantics)
//
// The char/string validators (validPathChar/validNamespaceChar/isValidPath/
// isValidNamespace, incl. the namespace ".." special-case) are NOT re-ported
// here; we reuse the already-certified port in IdentifierChars.h (gate
// rl_chars_parity). isAllowedInIdentifier / StringReader-based read()/readGreedy
// are Brigadier-coupled and intentionally NOT ported (noted below).
//
// Java source mapping:
//   28   private Identifier(String namespace, String path)            (asserts validity)
//   35   createUntrusted(ns, path) = new Identifier(assertValidNamespace, assertValidPath)
//   39   fromNamespaceAndPath(ns, path) = createUntrusted(ns, path)    -> throws
//   43   parse(id)                = bySeparator(id, ':')               -> throws
//   47   withDefaultNamespace(path) = new Identifier("minecraft", assertValidPath(...))
//   51   tryParse(id)             = tryBySeparator(id, ':')            -> null
//   55   tryBuild(ns, path)       = isValid... ? new Identifier : null
//   59   bySeparator(id, sep)                                          -> throws
//   74   tryBySeparator(id, sep)                                       -> null
//  124   toString()  = namespace + ":" + path
//  128   equals(o)
//  137   hashCode()  = 31 * namespace.hashCode() + path.hashCode()
//  142   compareTo(o): path.compareTo first, then namespace.compareTo
//  244   assertValidNamespace -> throws IdentifierException on invalid
//  260   assertValidPath      -> throws IdentifierException on invalid
//
// UNPORTED (hard no-op / absent, by design — registry/Brigadier coupled):
//   CODEC, STREAM_CODEC, read(String)->DataResult, read(StringReader),
//   readNonEmpty(StringReader), readGreedy, isAllowedInIdentifier(char),
//   resolveAgainst(Path). These are deliberately omitted, not stubbed true.

#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>

#include "IdentifierChars.h" // mc::resources::isValidPath / isValidNamespace (certified)

namespace mc::resources {

// Mirror of net.minecraft.IdentifierException (a RuntimeException). Thrown by the
// validating constructors (parse / fromNamespaceAndPath / withDefaultNamespace).
// NOTE: the Java ctor runs StringEscapeUtils.escapeJava on the message; we keep
// the raw (pre-escape) message text because the parity gate only checks the
// threw/not-threw flag and the resulting ns/path/toString, never the message.
class IdentifierException : public std::runtime_error {
public:
    explicit IdentifierException(const std::string& message) : std::runtime_error(message) {}
};

// Java String.hashCode(): h = 31*h + charAt(i), over UTF-16 code units, 32-bit
// signed wraparound. Our inputs are ASCII byte strings (one byte == one code
// unit), matching how the certified char-validators model these strings.
inline int32_t javaStringHashCode(std::string_view s) {
    int32_t h = 0;
    for (unsigned char ch : s) {
        h = static_cast<int32_t>(static_cast<uint32_t>(h) * 31u + static_cast<uint32_t>(ch));
    }
    return h;
}

// Java String.compareTo(String): lexicographic over UTF-16 code units, returning
// the difference of the first differing code unit, else len(this)-len(other).
// ASCII bytes -> code units, so a byte-wise compare with the same return shape.
inline int javaStringCompareTo(std::string_view a, std::string_view b) {
    std::size_t lim = a.size() < b.size() ? a.size() : b.size();
    for (std::size_t i = 0; i < lim; ++i) {
        int ca = static_cast<unsigned char>(a[i]);
        int cb = static_cast<unsigned char>(b[i]);
        if (ca != cb) return ca - cb;
    }
    return static_cast<int>(a.size()) - static_cast<int>(b.size());
}

class Identifier {
public:
    // Identifier.java:21-22.
    static constexpr char NAMESPACE_SEPARATOR = ':';
    static constexpr std::string_view DEFAULT_NAMESPACE = "minecraft";
    static constexpr std::string_view REALMS_NAMESPACE = "realms";

    const std::string& getNamespace() const { return namespace_; } // :103
    const std::string& getPath() const { return path_; }           // :99

    // toString() — Identifier.java:124. "namespace:path".
    std::string toString() const { return namespace_ + ":" + path_; }

    // equals(Object) — Identifier.java:128-135.
    bool operator==(const Identifier& o) const {
        return namespace_ == o.namespace_ && path_ == o.path_;
    }
    bool operator!=(const Identifier& o) const { return !(*this == o); }

    // hashCode() — Identifier.java:137-140.
    int32_t hashCode() const {
        return static_cast<int32_t>(static_cast<uint32_t>(31) * static_cast<uint32_t>(javaStringHashCode(namespace_))
                                    + static_cast<uint32_t>(javaStringHashCode(path_)));
    }

    // compareTo(Identifier) — Identifier.java:142-149. PATH first, then NAMESPACE.
    int compareTo(const Identifier& o) const {
        int result = javaStringCompareTo(path_, o.path_);
        if (result == 0) {
            result = javaStringCompareTo(namespace_, o.namespace_);
        }
        return result;
    }

    // ---- Throwing factories (mirror IdentifierException on invalid) ----

    // fromNamespaceAndPath(ns, path) -> createUntrusted -> assertValid* (throws).
    // Identifier.java:39-41 / 35-37.
    static Identifier fromNamespaceAndPath(const std::string& namespace_, const std::string& path) {
        return Identifier(assertValidNamespace(namespace_, path), assertValidPath(namespace_, path));
    }

    // withDefaultNamespace(path) = new Identifier("minecraft", assertValidPath(...)).
    // Identifier.java:47-49. Validates ONLY the path (namespace is the trusted constant).
    static Identifier withDefaultNamespace(const std::string& path) {
        return Identifier(std::string(DEFAULT_NAMESPACE), assertValidPath(std::string(DEFAULT_NAMESPACE), path));
    }

    // parse(identifier) = bySeparator(identifier, ':'). Identifier.java:43-45.
    static Identifier parse(const std::string& identifier) {
        return bySeparator(identifier, ':');
    }

    // bySeparator(identifier, separator) — Identifier.java:59-72. THROWS on invalid.
    static Identifier bySeparator(const std::string& identifier, char separator) {
        std::size_t sep = identifier.find(separator);
        if (sep != std::string::npos) {
            std::string path = identifier.substr(sep + 1);
            if (sep != 0) {
                std::string ns = identifier.substr(0, sep);
                return Identifier(assertValidNamespace(ns, path), assertValidPath(ns, path));
            } else {
                return withDefaultNamespace(path);
            }
        } else {
            return withDefaultNamespace(identifier);
        }
    }

    // ---- Nullable factories (return std::nullopt instead of null) ----

    // tryParse(identifier) = tryBySeparator(identifier, ':'). Identifier.java:51-53.
    static std::optional<Identifier> tryParse(const std::string& identifier) {
        return tryBySeparator(identifier, ':');
    }

    // tryBuild(ns, path) — Identifier.java:55-57.
    static std::optional<Identifier> tryBuild(const std::string& namespace_, const std::string& path) {
        if (isValidNamespace(namespace_) && isValidPath(path)) {
            return Identifier(namespace_, path);
        }
        return std::nullopt;
    }

    // tryBySeparator(identifier, separator) — Identifier.java:74-89.
    static std::optional<Identifier> tryBySeparator(const std::string& identifier, char separator) {
        std::size_t sep = identifier.find(separator);
        if (sep != std::string::npos) {
            std::string path = identifier.substr(sep + 1);
            if (!isValidPath(path)) {
                return std::nullopt;
            } else if (sep != 0) {
                std::string ns = identifier.substr(0, sep);
                if (isValidNamespace(ns)) return Identifier(ns, path);
                return std::nullopt;
            } else {
                return Identifier(std::string(DEFAULT_NAMESPACE), path);
            }
        } else {
            if (isValidPath(identifier)) return Identifier(std::string(DEFAULT_NAMESPACE), identifier);
            return std::nullopt;
        }
    }

private:
    std::string namespace_;
    std::string path_;

    // Private ctor — Identifier.java:28-33 (the asserts are debug-only in Java).
    Identifier(std::string namespace_in, std::string path_in)
        : namespace_(std::move(namespace_in)), path_(std::move(path_in)) {}

    // assertValidNamespace(ns, path) — Identifier.java:244-250.
    static std::string assertValidNamespace(const std::string& namespace_, const std::string& path) {
        if (!isValidNamespace(namespace_)) {
            throw IdentifierException("Non [a-z0-9_.-] character in namespace of identifier: " + namespace_ + ":" + path);
        }
        return namespace_;
    }

    // assertValidPath(ns, path) — Identifier.java:260-266.
    static std::string assertValidPath(const std::string& namespace_, const std::string& path) {
        if (!isValidPath(path)) {
            throw IdentifierException("Non [a-z0-9/._-] character in path of location: " + namespace_ + ":" + path);
        }
        return path;
    }
};

} // namespace mc::resources
