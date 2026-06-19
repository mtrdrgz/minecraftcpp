#pragma once

// Port of net.minecraft.world.level.levelgen.WorldGenerationContext. Holds the
// resolved vertical bounds used to resolve VerticalAnchors / sample
// HeightProviders. In Java it is computed as
//   minY   = max(heightAccessor.getMinY(),  generator.getMinY())
//   height = min(heightAccessor.getHeight(), generator.getGenDepth())
// here we just carry the two resolved values.

namespace mc::levelgen {

class WorldGenerationContext {
public:
    WorldGenerationContext(int minGenY, int genDepth) : m_minGenY(minGenY), m_genDepth(genDepth) {}
    int getMinGenY() const { return m_minGenY; }
    int getGenDepth() const { return m_genDepth; }

private:
    int m_minGenY;
    int m_genDepth;
};

} // namespace mc::levelgen
