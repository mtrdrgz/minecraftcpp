// 1:1 ports of the math/value types the block-model pipeline uses:
//   net.minecraft.core.Direction              (subset: model baking needs)
//   com.mojang.math.Quadrant
//   com.mojang.math.SymmetricGroup3
//   com.mojang.math.OctahedralGroup
//   com.mojang.math.Transformation            (matrix-only subset)
//   net.minecraft.core.BlockMath
// Source: 26.1.2/src decompiled Java. No GL / engine dependencies.
#pragma once

#include "Joml.h"
#include <array>
#include <optional>
#include <string>

namespace mc::render::model {

using joml::Matrix3f;
using joml::Matrix4f;
using joml::Quaternionf;
using joml::Vector3f;

// ── net.minecraft.util.Mth.positiveModulo(int, int) ─────────────────────────
inline int positiveModulo(int x, int y) {
    int r = x % y;
    return r < 0 ? r + y : r;
}

// ── net.minecraft.core.Direction ─────────────────────────────────────────────
// Enum order (ordinals): DOWN, UP, NORTH, SOUTH, WEST, EAST
enum class Direction : int { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

enum class Axis : int { X = 0, Y = 1, Z = 2 };
enum class AxisDirection : int { POSITIVE = 0, NEGATIVE = 1 };

struct DirectionInfo {
    const char* name;
    Axis axis;
    AxisDirection axisDirection;
    int nx, ny, nz; // integer normal
};

inline const DirectionInfo& directionInfo(Direction d) {
    static const DirectionInfo INFOS[6] = {
        { "down",  Axis::Y, AxisDirection::NEGATIVE,  0, -1,  0 },
        { "up",    Axis::Y, AxisDirection::POSITIVE,  0,  1,  0 },
        { "north", Axis::Z, AxisDirection::NEGATIVE,  0,  0, -1 },
        { "south", Axis::Z, AxisDirection::POSITIVE,  0,  0,  1 },
        { "west",  Axis::X, AxisDirection::NEGATIVE, -1,  0,  0 },
        { "east",  Axis::X, AxisDirection::POSITIVE,  1,  0,  0 },
    };
    return INFOS[(int)d];
}

inline const char* directionName(Direction d) { return directionInfo(d).name; }

inline Vector3f directionUnitVec(Direction d) {
    const DirectionInfo& i = directionInfo(d);
    return Vector3f((float)i.nx, (float)i.ny, (float)i.nz);
}

std::optional<Direction> directionByName(const std::string& name);
Direction directionFromAxisAndDirection(Axis axis, AxisDirection dir);
Axis axisByName(const std::string& name); // throws on bad input

// Direction.getApproximateNearest(float, float, float):
//   best = NORTH; highestDot = Float.MIN_VALUE (1.4e-45f, smallest denormal!)
Direction getApproximateNearest(float dx, float dy, float dz);

// Direction.rotate(Matrix4fc, Direction)
Direction rotateDirection(const Matrix4f& matrix, Direction facing);

// ── com.mojang.math.SymmetricGroup3 ──────────────────────────────────────────
// Enum order: P123, P213, P132, P312, P231, P321
struct SymmetricGroup3 {
    int ordinal;
    int p0, p1, p2;
    Matrix3f transformation;

    static const SymmetricGroup3& value(int ordinal);
    static const SymmetricGroup3& compose(const SymmetricGroup3& first, const SymmetricGroup3& second);
    const SymmetricGroup3& inverse() const;

    int permute(int i) const { return i == 0 ? p0 : (i == 1 ? p1 : p2); }
    Axis permuteAxis(Axis axis) const { return (Axis)permute((int)axis); }
};

// ── com.mojang.math.OctahedralGroup ──────────────────────────────────────────
struct OctahedralGroup {
    int ordinal;
    const char* name;
    const SymmetricGroup3* permutation;
    bool invertX, invertY, invertZ;
    Matrix3f transformation;

    static const OctahedralGroup& value(int ordinal); // 48 values, Java enum order
    static const OctahedralGroup& identity();
    // BLOCK_ROT_* aliases
    static const OctahedralGroup& blockRot(Axis axis, int quadrantOrdinal); // quadrant 0..3 (R0..R270)

    const OctahedralGroup& compose(const OctahedralGroup& that) const;
    bool inverts(Axis axis) const { return axis == Axis::X ? invertX : (axis == Axis::Y ? invertY : invertZ); }
    Direction rotate(Direction direction) const;
};

// ── com.mojang.math.Quadrant ─────────────────────────────────────────────────
enum class Quadrant : int { R0 = 0, R90 = 1, R180 = 2, R270 = 3 };

inline int quadrantShift(Quadrant q) { return (int)q; }
Quadrant quadrantParseJson(int degrees); // throws on invalid
// Quadrant.fromXYZAngles: zRot.rotationZ.compose(yRot.rotationY.compose(xRot.rotationX))
const OctahedralGroup& quadrantFromXYZAngles(Quadrant x, Quadrant y, Quadrant z);
inline int rotateVertexIndex(Quadrant q, int index) { return (index + quadrantShift(q)) % 4; }

// ── com.mojang.math.Transformation (matrix subset) ───────────────────────────
// The pipeline only uses getMatrix/compose/inverse and the IDENTITY singleton;
// identity is tracked as a flag to reproduce Java's reference comparisons
// (`rotation != Transformation.IDENTITY`).
struct Transformation {
    Matrix4f matrix;       // properties tracked inside
    bool isIdentityRef = false;

    static const Transformation& identityRef();

    Transformation() = default;
    explicit Transformation(const Matrix4f& m) : matrix(m) {}

    const Matrix4f& getMatrix() const { return matrix; }

    Transformation compose(const Transformation& that) const {
        Matrix4f m = matrix;     // getMatrixCopy()
        m.mul(that.matrix);
        return Transformation(m);
    }

    Transformation inverse() const {
        if (isIdentityRef) return *this;
        Matrix4f m = matrix.invertAffine(); // getMatrixCopy().invertAffine()
        // Java checks isFinite and returns null otherwise; unreachable for the
        // orthonormal rotations used here.
        return Transformation(m);
    }
};

// ── net.minecraft.core.BlockMath ─────────────────────────────────────────────
namespace BlockMath {
    // VANILLA_UV_TRANSFORM_LOCAL_TO_GLOBAL / GLOBAL_TO_LOCAL, indexed by Direction
    const Transformation& uvTransformLocalToGlobal(Direction d);
    const Transformation& uvTransformGlobalToLocal(Direction d);
    Transformation getFaceTransformation(const Transformation& transformation, Direction originalSide);
}

} // namespace mc::render::model
