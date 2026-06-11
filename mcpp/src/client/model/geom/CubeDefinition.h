// 1:1 C++ port of the PURE, GL-free cube vertex-builder math of
// net.minecraft.client.model.geom.ModelPart.Cube / .Polygon / .Vertex
// (Minecraft 26.1.2). This is the geometry the model baker emits per cube: 8
// box corners (grow-inflated, optionally X-mirrored), the per-face texture-rect
// remap into UVs, and the per-face normal (the Direction unit vector, mirrored
// on the X axis when `mirror` is set). NONE of it touches a VertexConsumer,
// PoseStack, or GL context — it is deterministic float arithmetic, exactly the
// kind the parity ladder certifies.
//
// Source (ModelPart.java, 26.1.2):
//
//   Cube(int xTexOffs,int yTexOffs, float minX,minY,minZ, float w,h,d,
//        float growX,growY,growZ, boolean mirror, float xTexSize,yTexSize,
//        Set<Direction> visibleFaces)
//     -> minX/Y/Z, maxX/Y/Z fields use the UN-grown box;
//        the 8 vertices use the GROWN, possibly mirrored box;
//        u0..u4 / v0..v2 are the texture-rect cuts;
//        polygons are emitted in DOWN,UP,WEST,NORTH,EAST,SOUTH order, skipping
//        faces not in visibleFaces.
//
//   Polygon(Vertex[] v, float u0,v0,u1,v1, float xTexSize,yTexSize,
//           boolean mirror, Direction facing)
//     normal = (mirror ? mirrorFacing(facing) : facing).getUnitVec3f();
//     us = 0.0F / xTexSize;  vs = 0.0F / yTexSize;   // NB: a real float divide
//     v[0]=v[0].remap(u1/xTexSize - us, v0/yTexSize + vs);
//     v[1]=v[1].remap(u0/xTexSize + us, v0/yTexSize + vs);
//     v[2]=v[2].remap(u0/xTexSize + us, v1/yTexSize - vs);
//     v[3]=v[3].remap(u1/xTexSize - us, v1/yTexSize - vs);
//     if (mirror) reverse(v);
//   mirrorFacing(d) = d.axis==X ? d.opposite : d;
//
//   Vertex.worldX/Y/Z() = x/16.0F, y/16.0F, z/16.0F (SCALE_FACTOR=16).
//
// 1:1 NOTES:
//  * All arithmetic is plain float a+b / a-b / a/b — no FMA, no transcendentals,
//    so -ffp-contract=off (project default) keeps a*b+c un-fused (not used here,
//    but the divides must NOT be reassociated). Build at the project flags.
//  * `us`/`vs` are literally `0.0F / xTexSize`. For a finite non-zero texSize
//    that is +0.0f (or -0.0f if texSize<0); for texSize==0 it is NaN; the remap
//    then adds/subtracts it. We reproduce the divide verbatim rather than folding
//    it to 0, so the NaN/-0 edge cases match Java bit-for-bit.
//  * Direction.getUnitVec3f() is `new Vector3f(normal.getX(),getY(),getZ())` from
//    the integer Vec3i normals — exact small integers cast to float.
//  * minX/Y/Z and maxX/Y/Z Cube fields are the UN-grown box (computed before the
//    grow subtraction); the vertices use the grown box. Easy to conflate — keep
//    the two `maxX/Y/Z` locals (grown) distinct from the fields.

#pragma once

#include <array>
#include <cstddef>
#include <vector>

namespace mc::client::model::geom {

// net.minecraft.core.Direction (the six faces). data2d/data3d are irrelevant to
// the cube math; we only need the axis (for mirrorFacing) and the unit vector
// (the face normal). Values are the exact Vec3i normals from Direction.java.
enum class Direction { DOWN, UP, NORTH, SOUTH, WEST, EAST };

namespace direction_detail {
struct Vec3i {
    int x, y, z;
};
// Direction.java: DOWN(0,-1,0) UP(0,1,0) NORTH(0,0,-1) SOUTH(0,0,1) WEST(-1,0,0) EAST(1,0,0)
inline Vec3i normal(Direction d) {
    switch (d) {
        case Direction::DOWN:  return {0, -1, 0};
        case Direction::UP:    return {0, 1, 0};
        case Direction::NORTH: return {0, 0, -1};
        case Direction::SOUTH: return {0, 0, 1};
        case Direction::WEST:  return {-1, 0, 0};
        case Direction::EAST:  return {1, 0, 0};
    }
    return {0, 0, 0};
}
enum class Axis { X, Y, Z };
inline Axis axisOf(Direction d) {
    switch (d) {
        case Direction::DOWN:
        case Direction::UP:    return Axis::Y;
        case Direction::NORTH:
        case Direction::SOUTH: return Axis::Z;
        case Direction::WEST:
        case Direction::EAST:  return Axis::X;
    }
    return Axis::X;
}
inline Direction opposite(Direction d) {
    switch (d) {
        case Direction::DOWN:  return Direction::UP;
        case Direction::UP:    return Direction::DOWN;
        case Direction::NORTH: return Direction::SOUTH;
        case Direction::SOUTH: return Direction::NORTH;
        case Direction::WEST:  return Direction::EAST;
        case Direction::EAST:  return Direction::WEST;
    }
    return d;
}
}  // namespace direction_detail

// org.joml.Vector3f-as-data: the face normal. getUnitVec3f() builds it from the
// integer Vec3i normal, so the float values are exact small integers.
struct UnitVec3f {
    float x, y, z;
};
inline UnitVec3f getUnitVec3f(Direction d) {
    direction_detail::Vec3i n = direction_detail::normal(d);
    return {static_cast<float>(n.x), static_cast<float>(n.y), static_cast<float>(n.z)};
}

// ── ModelPart.Vertex (record) ────────────────────────────────────────────────
struct Vertex {
    float x, y, z, u, v;
    static constexpr float SCALE_FACTOR = 16.0F;

    Vertex remap(float nu, float nv) const { return Vertex{x, y, z, nu, nv}; }
    float worldX() const { return x / 16.0F; }
    float worldY() const { return y / 16.0F; }
    float worldZ() const { return z / 16.0F; }
};

// ── ModelPart.Polygon (record(Vertex[] vertices, Vector3fc normal)) ──────────
struct Polygon {
    std::array<Vertex, 4> vertices;
    UnitVec3f normal;

    // ModelPart.Polygon.mirrorFacing
    static Direction mirrorFacing(Direction facing) {
        return direction_detail::axisOf(facing) == direction_detail::Axis::X
                   ? direction_detail::opposite(facing)
                   : facing;
    }

    // The convenience-constructor body, verbatim from ModelPart.java.
    static Polygon make(std::array<Vertex, 4> v, float u0, float v0, float u1, float v1,
                        float xTexSize, float yTexSize, bool mirror, Direction facing) {
        Polygon p;
        p.normal = getUnitVec3f(mirror ? mirrorFacing(facing) : facing);
        float us = 0.0F / xTexSize;
        float vs = 0.0F / yTexSize;
        v[0] = v[0].remap(u1 / xTexSize - us, v0 / yTexSize + vs);
        v[1] = v[1].remap(u0 / xTexSize + us, v0 / yTexSize + vs);
        v[2] = v[2].remap(u0 / xTexSize + us, v1 / yTexSize - vs);
        v[3] = v[3].remap(u1 / xTexSize - us, v1 / yTexSize - vs);
        if (mirror) {
            // reverse the 4-element array (length/2 swaps)
            std::size_t length = v.size();
            for (std::size_t i = 0; i < length / 2; ++i) {
                Vertex tmp = v[i];
                v[i] = v[length - 1 - i];
                v[length - 1 - i] = tmp;
            }
        }
        p.vertices = v;
        return p;
    }
};

// ── ModelPart.Cube ───────────────────────────────────────────────────────────
struct Cube {
    std::vector<Polygon> polygons;  // size == visibleFaces.size()
    float minX, minY, minZ;         // UN-grown box (Cube fields)
    float maxX, maxY, maxZ;

    // visibleFaces: presence flags in DOWN,UP,NORTH,SOUTH,WEST,EAST order. The
    // Java passes a Set<Direction>; .contains() order in the ctor is fixed
    // (DOWN,UP,WEST,NORTH,EAST,SOUTH), which is what we emit below.
    static Cube make(int xTexOffs, int yTexOffs, float minX, float minY, float minZ, float width,
                     float height, float depth, float growX, float growY, float growZ, bool mirror,
                     float xTexSize, float yTexSize, bool faceDown, bool faceUp, bool faceNorth,
                     bool faceSouth, bool faceWest, bool faceEast) {
        Cube c;
        c.minX = minX;
        c.minY = minY;
        c.minZ = minZ;
        c.maxX = minX + width;   // un-grown box fields
        c.maxY = minY + height;
        c.maxZ = minZ + depth;

        float maxX = minX + width;  // grown-box locals (shadow the fields, as Java does)
        float maxY = minY + height;
        float maxZ = minZ + depth;
        minX -= growX;
        minY -= growY;
        minZ -= growZ;
        maxX += growX;
        maxY += growY;
        maxZ += growZ;
        if (mirror) {
            float tmp = maxX;
            maxX = minX;
            minX = tmp;
        }

        Vertex t0{minX, minY, minZ, 0.0F, 0.0F};
        Vertex t1{maxX, minY, minZ, 0.0F, 8.0F};
        Vertex t2{maxX, maxY, minZ, 8.0F, 8.0F};
        Vertex t3{minX, maxY, minZ, 8.0F, 0.0F};
        Vertex l0{minX, minY, maxZ, 0.0F, 0.0F};
        Vertex l1{maxX, minY, maxZ, 0.0F, 8.0F};
        Vertex l2{maxX, maxY, maxZ, 8.0F, 8.0F};
        Vertex l3{minX, maxY, maxZ, 8.0F, 0.0F};

        float u0 = static_cast<float>(xTexOffs);
        float u1 = xTexOffs + depth;
        float u2 = xTexOffs + depth + width;
        float u22 = xTexOffs + depth + width + width;
        float u3 = xTexOffs + depth + width + depth;
        float u4 = xTexOffs + depth + width + depth + width;
        float v0 = static_cast<float>(yTexOffs);
        float v1 = yTexOffs + depth;
        float v2 = yTexOffs + depth + height;

        // NB: emission order matches the Java contains() checks exactly:
        // DOWN, UP, WEST, NORTH, EAST, SOUTH.
        if (faceDown)
            c.polygons.push_back(Polygon::make({l1, l0, t0, t1}, u1, v0, u2, v1, xTexSize, yTexSize,
                                               mirror, Direction::DOWN));
        if (faceUp)
            c.polygons.push_back(Polygon::make({t2, t3, l3, l2}, u2, v1, u22, v0, xTexSize, yTexSize,
                                               mirror, Direction::UP));
        if (faceWest)
            c.polygons.push_back(Polygon::make({t0, l0, l3, t3}, u0, v1, u1, v2, xTexSize, yTexSize,
                                               mirror, Direction::WEST));
        if (faceNorth)
            c.polygons.push_back(Polygon::make({t1, t0, t3, t2}, u1, v1, u2, v2, xTexSize, yTexSize,
                                               mirror, Direction::NORTH));
        if (faceEast)
            c.polygons.push_back(Polygon::make({l1, t1, t2, l2}, u2, v1, u3, v2, xTexSize, yTexSize,
                                               mirror, Direction::EAST));
        if (faceSouth)
            c.polygons.push_back(Polygon::make({l0, l1, l2, l3}, u3, v1, u4, v2, xTexSize, yTexSize,
                                               mirror, Direction::SOUTH));
        return c;
    }
};

// ── net.minecraft.client.model.geom.builders.CubeDeformation ─────────────────
struct CubeDeformation {
    float growX = 0.0F, growY = 0.0F, growZ = 0.0F;

    CubeDeformation() = default;
    CubeDeformation(float gx, float gy, float gz) : growX(gx), growY(gy), growZ(gz) {}
    explicit CubeDeformation(float grow) : growX(grow), growY(grow), growZ(grow) {}

    CubeDeformation extend(float factor) const {
        return CubeDeformation{growX + factor, growY + factor, growZ + factor};
    }
    CubeDeformation extend(float fx, float fy, float fz) const {
        return CubeDeformation{growX + fx, growY + fy, growZ + fz};
    }
};

}  // namespace mc::client::model::geom
