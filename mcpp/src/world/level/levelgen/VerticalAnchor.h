#pragma once
#ifndef MCPP_LEVELGEN_VERTICALANCHOR_H
#define MCPP_LEVELGEN_VERTICALANCHOR_H

// Port of net.minecraft.world.level.levelgen.VerticalAnchor (Absolute /
// AboveBottom / BelowTop) — resolves a Y coordinate relative to the world's
// vertical bounds.
// NOTE: a macro include guard is kept alongside #pragma once because some build
// configs reached this header via two distinct relative (`..`) paths and double-
// defined the class; the macro guard dedupes by token regardless of path.

#include "WorldGenerationContext.h"

#include <memory>
#include <utility>

namespace mc::levelgen {

class VerticalAnchor {
public:
    virtual ~VerticalAnchor() = default;
    virtual int resolveY(const WorldGenerationContext& context) const = 0;
};

using VerticalAnchorPtr = std::shared_ptr<const VerticalAnchor>;

class AbsoluteAnchor final : public VerticalAnchor {
public:
    explicit AbsoluteAnchor(int y) : m_y(y) {}
    int resolveY(const WorldGenerationContext&) const override { return m_y; }

private:
    int m_y;
};

class AboveBottomAnchor final : public VerticalAnchor {
public:
    explicit AboveBottomAnchor(int offset) : m_offset(offset) {}
    int resolveY(const WorldGenerationContext& c) const override { return c.getMinGenY() + m_offset; }

private:
    int m_offset;
};

class BelowTopAnchor final : public VerticalAnchor {
public:
    explicit BelowTopAnchor(int offset) : m_offset(offset) {}
    int resolveY(const WorldGenerationContext& c) const override {
        return c.getGenDepth() - 1 + c.getMinGenY() - m_offset;
    }

private:
    int m_offset;
};

namespace VerticalAnchors {
inline VerticalAnchorPtr absolute(int y) { return std::make_shared<AbsoluteAnchor>(y); }
inline VerticalAnchorPtr aboveBottom(int offset) { return std::make_shared<AboveBottomAnchor>(offset); }
inline VerticalAnchorPtr belowTop(int offset) { return std::make_shared<BelowTopAnchor>(offset); }
inline VerticalAnchorPtr bottom() { return aboveBottom(0); }
inline VerticalAnchorPtr top() { return belowTop(0); }
} // namespace VerticalAnchors

} // namespace mc::levelgen

#endif // MCPP_LEVELGEN_VERTICALANCHOR_H
