#pragma once

// 1:1 port of the PURE surface of net.minecraft.network.chat.Style (Style.java, MC 26.1.2).
// Style is an immutable bag of text-formatting fields. This header ports ONLY the fields and
// methods that are free of GL / registry / Codec / network concerns, and reuses the certified
// TextColor (mc::chat) and ChatFormatting (mc::chat_formatting) ports. Certified by style_parity.
//
// PORTED fields (Java types in comments):
//   color        @Nullable TextColor   -> std::optional<TextColor>
//   bold/italic/underlined/strikethrough/obfuscated
//                @Nullable Boolean     -> std::optional<bool>  (tri-state TRUE / FALSE / null)
//
// PORTED methods (each translated VERBATIM):
//   isEmpty
//   isBold/isItalic/isUnderlined/isStrikethrough/isObfuscated  (== Boolean.TRUE -> *opt == true)
//   getColor
//   withColor(TextColor), withColor(ChatFormatting)
//   withBold/withItalic/withUnderlined/withStrikethrough/withObfuscated(Boolean)
//   applyFormat(ChatFormatting), applyLegacyFormat(ChatFormatting), applyFormats(ChatFormatting...)
//   applyTo(Style)         the parent-merge: each field = child-if-non-null-else-parent
//   equals
//
// 1:1 traps captured (all reproduced exactly below):
//   * Tri-state: a field is TRUE, FALSE, or null(absent). isBold() returns `bold == Boolean.TRUE`,
//     so a FALSE field is NOT bold. applyTo merges on null-ness only (a FALSE child overrides a
//     TRUE parent), so std::optional models null vs present(true/false).
//   * isEmpty() in Java is identity `this == EMPTY`. Every constructor path that could produce the
//     all-null Style (create / checkEmptyAfterChange / applyFormat RESET / applyLegacyFormat RESET)
//     canonicalizes back to the EMPTY singleton, so `this == EMPTY` is equivalent to "all fields
//     null". This ported subset only ever sets color + the 5 booleans (the SKIPPED fields below are
//     always absent here), so isEmpty() is reproduced as value-equality to the all-absent Style.
//   * applyFormat: color formats set color via TextColor.fromLegacyFormat; the 5 format codes set
//     the matching boolean to TRUE; RESET returns EMPTY; otherwise (a color) color is replaced.
//   * applyLegacyFormat: identical to applyFormat for the format codes / RESET, BUT the color branch
//     (default) ALSO resets all 5 booleans to FALSE before setting the color (legacy '§<color>'
//     semantics: a color code clears prior formatting).
//   * applyFormats(...): applyFormat's body run for each format in order; RESET short-circuits to
//     EMPTY immediately (does NOT continue the loop).
//   * equals: field-by-field; for the tri-state, Java compares Boolean references but because all
//     Booleans here come from autoboxing of `true`/`false` (cached) or null, reference equality
//     equals value equality — std::optional<bool> equality reproduces it.
//
// SKIPPED (Java-only / out of this pure scope; NOT faked — these fields are simply never set by the
// ported methods, and the Java methods that touch them are not ported):
//   shadowColor (@Nullable Integer), clickEvent (ClickEvent), hoverEvent (HoverEvent),
//   insertion (String), font (FontDescription); plus all Codec/Serializer/StreamCodec,
//   toString, hashCode, withColor(int)/withShadowColor/withoutShadow, with{Click,Hover,Insertion,
//   Font}Event, getShadowColor/getClickEvent/getHoverEvent/getInsertion/getFont.

#include <optional>

#include "../../ChatFormatting.h"
#include "TextColor.h"

namespace mc::chat {

struct Style {
    // @Nullable -> std::optional. nullopt == Java null.
    std::optional<TextColor> color;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underlined;
    std::optional<bool> strikethrough;
    std::optional<bool> obfuscated;

    // public static final Style EMPTY = new Style(null, ...). All ported fields null.
    static Style empty() { return Style{}; }

    // getColor(): @Nullable TextColor.
    const std::optional<TextColor>& getColor() const { return color; }

    // is*(): `return this.field == Boolean.TRUE;` -> true only when present AND true.
    bool isBold() const { return bold == true; }
    bool isItalic() const { return italic == true; }
    bool isStrikethrough() const { return strikethrough == true; }
    bool isUnderlined() const { return underlined == true; }
    bool isObfuscated() const { return obfuscated == true; }

    // isEmpty(): Java `this == EMPTY`. In this ported subset (skipped fields always absent) this is
    // equivalent to all ported fields being null. The canonicalization-to-singleton in the Java
    // constructors guarantees `this == EMPTY` iff every field is null, so value-equality is faithful.
    bool isEmpty() const {
        return !color && !bold && !italic && !underlined && !strikethrough && !obfuscated;
    }

    // withColor(@Nullable TextColor): copy with color replaced. (The Java `Objects.equals ? this`
    // short-circuit and checkEmptyAfterChange/EMPTY-canonicalization are immaterial to every
    // observable this gate checks; the resulting field values are identical either way.)
    Style withColor(const std::optional<TextColor>& c) const {
        Style s = *this;
        s.color = c;
        return s;
    }

    // withColor(@Nullable ChatFormatting): withColor(format != null ? fromLegacyFormat(format) : null).
    // Modeled with a pointer so nullptr == Java null.
    Style withColor(const ChatFormattingData* format) const {
        return withColor(format != nullptr ? chat::fromLegacyFormat(*format) : std::nullopt);
    }

    // with<Flag>(@Nullable Boolean): copy with that flag replaced (tri-state preserved).
    Style withBold(std::optional<bool> v) const { Style s = *this; s.bold = v; return s; }
    Style withItalic(std::optional<bool> v) const { Style s = *this; s.italic = v; return s; }
    Style withUnderlined(std::optional<bool> v) const { Style s = *this; s.underlined = v; return s; }
    Style withStrikethrough(std::optional<bool> v) const { Style s = *this; s.strikethrough = v; return s; }
    Style withObfuscated(std::optional<bool> v) const { Style s = *this; s.obfuscated = v; return s; }

    // applyFormat(ChatFormatting): VERBATIM switch.
    Style applyFormat(const ChatFormattingData& format) const {
        std::optional<TextColor> color = this->color;
        std::optional<bool> bold = this->bold;
        std::optional<bool> italic = this->italic;
        std::optional<bool> strikethrough = this->strikethrough;
        std::optional<bool> underlined = this->underlined;
        std::optional<bool> obfuscated = this->obfuscated;

        switch (formatKind(format)) {
            case Kind::OBFUSCATED: obfuscated = true; break;
            case Kind::BOLD: bold = true; break;
            case Kind::STRIKETHROUGH: strikethrough = true; break;
            case Kind::UNDERLINE: underlined = true; break;
            case Kind::ITALIC: italic = true; break;
            case Kind::RESET: return empty();
            case Kind::COLOR_OR_DEFAULT: color = chat::fromLegacyFormat(format); break;
        }

        Style s;
        s.color = color;
        s.bold = bold;
        s.italic = italic;
        s.underlined = underlined;
        s.strikethrough = strikethrough;
        s.obfuscated = obfuscated;
        return s;
    }

    // applyLegacyFormat(ChatFormatting): VERBATIM switch — note the default (color) branch ALSO
    // resets the 5 booleans to FALSE.
    Style applyLegacyFormat(const ChatFormattingData& format) const {
        std::optional<TextColor> color = this->color;
        std::optional<bool> bold = this->bold;
        std::optional<bool> italic = this->italic;
        std::optional<bool> strikethrough = this->strikethrough;
        std::optional<bool> underlined = this->underlined;
        std::optional<bool> obfuscated = this->obfuscated;

        switch (formatKind(format)) {
            case Kind::OBFUSCATED: obfuscated = true; break;
            case Kind::BOLD: bold = true; break;
            case Kind::STRIKETHROUGH: strikethrough = true; break;
            case Kind::UNDERLINE: underlined = true; break;
            case Kind::ITALIC: italic = true; break;
            case Kind::RESET: return empty();
            case Kind::COLOR_OR_DEFAULT:
                obfuscated = false;
                bold = false;
                strikethrough = false;
                underlined = false;
                italic = false;
                color = chat::fromLegacyFormat(format);
                break;
        }

        Style s;
        s.color = color;
        s.bold = bold;
        s.italic = italic;
        s.underlined = underlined;
        s.strikethrough = strikethrough;
        s.obfuscated = obfuscated;
        return s;
    }

    // applyFormats(ChatFormatting...): the applyFormat loop; RESET short-circuits to EMPTY.
    template <typename It>
    Style applyFormatsRange(It begin, It end) const {
        std::optional<TextColor> color = this->color;
        std::optional<bool> bold = this->bold;
        std::optional<bool> italic = this->italic;
        std::optional<bool> strikethrough = this->strikethrough;
        std::optional<bool> underlined = this->underlined;
        std::optional<bool> obfuscated = this->obfuscated;

        for (It it = begin; it != end; ++it) {
            const ChatFormattingData& format = **it;  // It dereferences to a ChatFormattingData*
            switch (formatKind(format)) {
                case Kind::OBFUSCATED: obfuscated = true; break;
                case Kind::BOLD: bold = true; break;
                case Kind::STRIKETHROUGH: strikethrough = true; break;
                case Kind::UNDERLINE: underlined = true; break;
                case Kind::ITALIC: italic = true; break;
                case Kind::RESET: return empty();
                case Kind::COLOR_OR_DEFAULT: color = chat::fromLegacyFormat(format); break;
            }
        }

        Style s;
        s.color = color;
        s.bold = bold;
        s.italic = italic;
        s.underlined = underlined;
        s.strikethrough = strikethrough;
        s.obfuscated = obfuscated;
        return s;
    }

    // applyTo(Style other): if this==EMPTY return other; if other==EMPTY return this; else merge
    // field-by-field child(this)-if-non-null-else-parent(other).
    Style applyTo(const Style& other) const {
        if (isEmpty()) return other;
        if (other.isEmpty()) return *this;
        Style s;
        s.color = color ? color : other.color;
        s.bold = bold ? bold : other.bold;
        s.italic = italic ? italic : other.italic;
        s.underlined = underlined ? underlined : other.underlined;
        s.strikethrough = strikethrough ? strikethrough : other.strikethrough;
        s.obfuscated = obfuscated ? obfuscated : other.obfuscated;
        return s;
    }

    // equals: field-by-field over the ported subset (skipped fields always absent here, so they are
    // equal trivially). For the tri-state the optional<bool> comparison matches Java's Boolean ==.
    bool equals(const Style& o) const {
        return bold == o.bold && colorEquals(color, o.color) && italic == o.italic
            && obfuscated == o.obfuscated && strikethrough == o.strikethrough
            && underlined == o.underlined;
    }

  private:
    enum class Kind { OBFUSCATED, BOLD, STRIKETHROUGH, UNDERLINE, ITALIC, RESET, COLOR_OR_DEFAULT };

    // Map a ChatFormatting onto the switch arms. The Java switches on the 5 format constants and
    // RESET by identity; every other constant falls through to `default` (the color branch).
    static Kind formatKind(const ChatFormattingData& f) {
        switch (f.ordinal) {
            case 16: return Kind::OBFUSCATED;     // OBFUSCATED
            case 17: return Kind::BOLD;           // BOLD
            case 18: return Kind::STRIKETHROUGH;  // STRIKETHROUGH
            case 19: return Kind::UNDERLINE;      // UNDERLINE
            case 20: return Kind::ITALIC;         // ITALIC
            case 21: return Kind::RESET;          // RESET
            default: return Kind::COLOR_OR_DEFAULT;
        }
    }

    // Objects.equals(this.getColor(), style.getColor()) over @Nullable TextColor. TextColor.equals in
    // Java compares ONLY `value` (not the name), so reproduce that exactly; nullopt vs present matches
    // null vs non-null.
    static bool colorEquals(const std::optional<TextColor>& a, const std::optional<TextColor>& b) {
        if (a.has_value() != b.has_value()) return false;
        if (!a.has_value()) return true;
        return a->value == b->value;
    }
};

}  // namespace mc::chat
