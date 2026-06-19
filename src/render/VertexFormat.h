// 1:1 C++ port of com.mojang.blaze3d.vertex.VertexFormat (+ VertexFormatElement,
// VertexFormat.Mode, VertexFormat.IndexType) from Minecraft 26.1.2.
//
// VertexFormat is pure 32-bit integer layout math with NO GL: a Builder accumulates
// per-element byte offsets (offset += element.byteSize(), plus explicit padding(n)),
// then the format computes a 32-entry offsetsByElement[] table (offset for each
// registered element id, or -1), an elementsMask (OR of 1<<id), getOffset/contains,
// and a hashCode of (elementsMask*31 + Arrays.hashCode(offsetsByElement)). The GPU
// upload helpers in the Java class are NOT layout math and are intentionally omitted.
//
// Verified bit-for-bit against the real class by render/VertexFormatParityTest.cpp.
//
// 1:1 traps honoured here:
//   * all arithmetic is 32-bit two's-complement (int32_t); hashCode wraps exactly
//     like Java's Arrays.hashCode(int[]) (result = 31*result + e, seed 1).
//   * offsetsByElement[id] uses elements.indexOf(byId(id)) → record-equality over
//     ALL five element fields (id,index,type,normalized,count), not just id.
//   * Type sizes are the enum's own bytes (FLOAT=4, UBYTE/BYTE=1, U/SHORT=2,
//     U/INT=4) — element byteSize = type.size()*count.
//   * Mode.indexCount: LINES/QUADS → vertexCount/4*6 (integer truncation toward 0);
//     all others → vertexCount.
//   * IndexType.least: (length & -65536) != 0 ? INT : SHORT — crossover at 65536.

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace mc::render {

// ── VertexFormatElement.Type (ordinals match the Java enum declaration order) ──
enum class VfeType : int32_t {
    FLOAT = 0,   // size 4
    UBYTE = 1,   // size 1
    BYTE = 2,    // size 1
    USHORT = 3,  // size 2
    SHORT = 4,   // size 2
    UINT = 5,    // size 4
    INT = 6,     // size 4
};

inline int32_t vfeTypeSize(VfeType t) {
    switch (t) {
        case VfeType::FLOAT: return 4;
        case VfeType::UBYTE: return 1;
        case VfeType::BYTE: return 1;
        case VfeType::USHORT: return 2;
        case VfeType::SHORT: return 2;
        case VfeType::UINT: return 4;
        case VfeType::INT: return 4;
    }
    return 0;
}

// ── VertexFormatElement (record: id, index, type, normalized, count) ──
struct VertexFormatElement {
    int32_t id = 0;
    int32_t index = 0;
    VfeType type = VfeType::FLOAT;
    bool normalized = false;
    int32_t count = 0;

    int32_t mask() const { return static_cast<int32_t>(1) << id; }          // 1 << id
    int32_t byteSize() const { return vfeTypeSize(type) * count; }          // type.size() * count

    // record equals(): all five components (used by VertexFormat's indexOf).
    bool operator==(const VertexFormatElement& o) const {
        return id == o.id && index == o.index && type == o.type &&
               normalized == o.normalized && count == o.count;
    }
    bool operator!=(const VertexFormatElement& o) const { return !(*this == o); }
};

// The canonical singletons registered by VertexFormatElement (ids 0..6).
namespace vfe {
inline const VertexFormatElement POSITION  {0, 0, VfeType::FLOAT, false, 3};
inline const VertexFormatElement COLOR     {1, 0, VfeType::UBYTE, true,  4};
inline const VertexFormatElement UV0       {2, 0, VfeType::FLOAT, false, 2};
inline const VertexFormatElement UV1       {3, 1, VfeType::SHORT, false, 2};
inline const VertexFormatElement UV2       {4, 2, VfeType::SHORT, false, 2};
inline const VertexFormatElement NORMAL    {5, 0, VfeType::BYTE,  true,  3};
inline const VertexFormatElement LINE_WIDTH{6, 0, VfeType::FLOAT, false, 1};

// VertexFormatElement.byId(id): the registry slot, or nullptr if unregistered.
inline const VertexFormatElement* byId(int32_t id) {
    switch (id) {
        case 0: return &POSITION;
        case 1: return &COLOR;
        case 2: return &UV0;
        case 3: return &UV1;
        case 4: return &UV2;
        case 5: return &NORMAL;
        case 6: return &LINE_WIDTH;
        default: return nullptr;  // ids 7..31 are not registered
    }
}
}  // namespace vfe

// ── VertexFormat ──
class VertexFormat {
public:
    static constexpr int32_t UNKNOWN_ELEMENT = -1;

    // Mirror of the private constructor: (elements, names, offsets, vertexSize).
    VertexFormat(std::vector<VertexFormatElement> elements,
                 std::vector<std::string> names,
                 std::vector<int32_t> offsets,
                 int32_t vertexSize)
        : elements_(std::move(elements)),
          names_(std::move(names)),
          vertexSize_(vertexSize) {
        // elementsMask = elements.stream().mapToInt(mask).reduce(0, |)
        elementsMask_ = 0;
        for (const auto& e : elements_) elementsMask_ |= e.mask();

        // offsetsByElement[id] = indexOf(byId(id)) != -1 ? offsets[index] : -1
        for (int32_t id = 0; id < 32; ++id) {
            const VertexFormatElement* element = vfe::byId(id);
            int32_t index = -1;
            if (element != nullptr) {
                for (size_t i = 0; i < elements_.size(); ++i) {
                    if (elements_[i] == *element) { index = static_cast<int32_t>(i); break; }
                }
            }
            offsetsByElement_[id] = (index != -1) ? offsets[static_cast<size_t>(index)] : -1;
        }
    }

    int32_t getVertexSize() const { return vertexSize_; }
    const std::vector<VertexFormatElement>& getElements() const { return elements_; }
    const std::vector<std::string>& getElementAttributeNames() const { return names_; }
    const std::array<int32_t, 32>& getOffsetsByElement() const { return offsetsByElement_; }

    int32_t getOffset(const VertexFormatElement& element) const {
        return offsetsByElement_[static_cast<size_t>(element.id)];
    }

    bool contains(const VertexFormatElement& element) const {
        return (elementsMask_ & element.mask()) != 0;
    }

    int32_t getElementsMask() const { return elementsMask_; }

    // getElementName(element): names.get(elements.indexOf(element)); throws if absent.
    // Returns false (and leaves out untouched) when the element is not in this format.
    bool getElementName(const VertexFormatElement& element, std::string& out) const {
        for (size_t i = 0; i < elements_.size(); ++i) {
            if (elements_[i] == element) { out = names_[i]; return true; }
        }
        return false;  // Java throws IllegalArgumentException here
    }

    // toString() = "VertexFormat" + names  (java.util.List.toString: [a, b, c]).
    std::string toString() const {
        std::string s = "VertexFormat[";
        for (size_t i = 0; i < names_.size(); ++i) {
            if (i) s += ", ";
            s += names_[i];
        }
        s += "]";
        return s;
    }

    // hashCode() = elementsMask * 31 + Arrays.hashCode(offsetsByElement)
    int32_t hashCode() const {
        int32_t arrHash = 1;  // Arrays.hashCode seed
        for (int32_t i = 0; i < 32; ++i) {
            arrHash = static_cast<int32_t>(31 * static_cast<int64_t>(arrHash) + offsetsByElement_[i]);
        }
        return static_cast<int32_t>(static_cast<int64_t>(elementsMask_) * 31 + arrHash);
    }

    // ── Builder (VertexFormat.Builder) ──
    class Builder {
    public:
        Builder& add(const std::string& name, const VertexFormatElement& element) {
            elements_.push_back(element);
            names_.push_back(name);
            offsets_.push_back(offset_);
            offset_ += element.byteSize();
            return *this;
        }
        Builder& padding(int32_t bytes) {
            offset_ += bytes;
            return *this;
        }
        // build(): vertexSize = offset; Mth.isMultipleOf(vertexSize, 4) is required
        // in Java (throws otherwise). We expose validity to the caller via outOk.
        VertexFormat build(bool* outOk = nullptr) const {
            int32_t vertexSize = offset_;
            bool ok = isMultipleOf(vertexSize, 4);
            if (outOk) *outOk = ok;
            return VertexFormat(elements_, names_, offsets_, vertexSize);
        }

    private:
        std::vector<VertexFormatElement> elements_;
        std::vector<std::string> names_;
        std::vector<int32_t> offsets_;
        int32_t offset_ = 0;
    };

    static Builder builder() { return Builder(); }

    // Mth.isMultipleOf(dividend, divisor): dividend % divisor == 0.
    static bool isMultipleOf(int32_t dividend, int32_t divisor) {
        return dividend % divisor == 0;
    }

    // ── IndexType ──
    enum class IndexType : int32_t { SHORT = 0, INT = 1 };
    static int32_t indexTypeBytes(IndexType t) {
        return t == IndexType::SHORT ? 2 : 4;
    }
    // least(length) = (length & -65536) != 0 ? INT : SHORT
    static IndexType indexTypeLeast(int32_t length) {
        return (length & -65536) != 0 ? IndexType::INT : IndexType::SHORT;
    }

    // ── Mode ──
    enum class Mode : int32_t {
        LINES = 0,
        DEBUG_LINES = 1,
        DEBUG_LINE_STRIP = 2,
        POINTS = 3,
        TRIANGLES = 4,
        TRIANGLE_STRIP = 5,
        TRIANGLE_FAN = 6,
        QUADS = 7,
    };
    static int32_t modePrimitiveLength(Mode m) {
        switch (m) {
            case Mode::LINES: case Mode::DEBUG_LINES: return 2;
            case Mode::DEBUG_LINE_STRIP: return 2;
            case Mode::POINTS: return 1;
            case Mode::TRIANGLES: case Mode::TRIANGLE_STRIP: case Mode::TRIANGLE_FAN: return 3;
            case Mode::QUADS: return 4;
        }
        return 0;
    }
    static int32_t modePrimitiveStride(Mode m) {
        switch (m) {
            case Mode::LINES: case Mode::DEBUG_LINES: return 2;
            case Mode::DEBUG_LINE_STRIP: return 1;
            case Mode::POINTS: return 1;
            case Mode::TRIANGLES: return 3;
            case Mode::TRIANGLE_STRIP: case Mode::TRIANGLE_FAN: return 1;
            case Mode::QUADS: return 4;
        }
        return 0;
    }
    static bool modeConnectedPrimitives(Mode m) {
        switch (m) {
            case Mode::DEBUG_LINE_STRIP:
            case Mode::TRIANGLE_STRIP:
            case Mode::TRIANGLE_FAN:
                return true;
            default:
                return false;
        }
    }
    // indexCount(vertexCount): LINES/QUADS → vertexCount/4*6; else → vertexCount.
    static int32_t modeIndexCount(Mode m, int32_t vertexCount) {
        switch (m) {
            case Mode::LINES:
            case Mode::QUADS:
                return vertexCount / 4 * 6;  // integer division truncates toward 0
            case Mode::DEBUG_LINES:
            case Mode::DEBUG_LINE_STRIP:
            case Mode::POINTS:
            case Mode::TRIANGLES:
            case Mode::TRIANGLE_STRIP:
            case Mode::TRIANGLE_FAN:
                return vertexCount;
        }
        return 0;
    }

private:
    std::vector<VertexFormatElement> elements_;
    std::vector<std::string> names_;
    int32_t vertexSize_ = 0;
    int32_t elementsMask_ = 0;
    std::array<int32_t, 32> offsetsByElement_{};
};

}  // namespace mc::render
