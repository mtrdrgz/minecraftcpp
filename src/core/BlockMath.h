// 1:1 port of net.minecraft.core.BlockMath — the UV-lock face transformation used
// by the block-model baking pipeline (FaceBakery / element rotation). Ported
// VERBATIM from 26.1.2/src/net/minecraft/core/BlockMath.java.
//
// Reuses the certified org.joml subset in render/model/Joml.h (Matrix4f / Vector3f /
// Quaternionf, plus MatrixUtil.isIdentity == matrixIsIdentity). NO new math lives
// here — only the BlockMath glue: the two precomputed Direction→Transformation maps
// (VANILLA_UV_TRANSFORM_LOCAL_TO_GLOBAL / _GLOBAL_TO_LOCAL), com.mojang.math
// Transformation's compose/inverse/IDENTITY semantics, and getFaceTransformation.
//
// Bit-exact: every constant/order comes from the Java. The static maps build their
// quaternions with new Quaternionf().rotateY/rotateX(angle) — these go through
// org.joml.Math.sin (= (float)Math.sin((double)x), mirrored by joml::jsin) and
// cosFromSin, so they reproduce identically in C++. Inputs in the parity gate are
// built from exact-float quaternions so no transcendental enters the *input* side.
//
// NO deviation from the source is permitted in this file.
#pragma once

#include "../render/model/Joml.h"

#include <array>

namespace mc::core::block_math {

namespace joml = mc::render::model::joml;

// ── net.minecraft.core.Direction (the 6 enum constants, in DECLARATION order) ──
// Direction.getApproximateNearest iterates VALUES in this exact order and compares
// dots strictly (>), so the order is load-bearing for ties.
//   DOWN(0,-1,0) UP(0,1,0) NORTH(0,0,-1) SOUTH(0,0,1) WEST(-1,0,0) EAST(1,0,0)
enum class Dir : int { DOWN = 0, UP = 1, NORTH = 2, SOUTH = 3, WEST = 4, EAST = 5 };

// normal (int) per enum constant, declaration order.
inline constexpr int DIR_NX[6] = { 0, 0, 0, 0, -1, 1 };
inline constexpr int DIR_NY[6] = { -1, 1, 0, 0, 0, 0 };
inline constexpr int DIR_NZ[6] = { 0, 0, -1, 1, 0, 0 };

// Direction.getApproximateNearest(float,float,float):
//   result = NORTH; highestDot = Float.MIN_VALUE (= 1.4E-45f, bits 0x00000001);
//   for each direction in VALUES: dot = dx*nx + dy*ny + dz*nz; if (dot > highestDot){...}
inline Dir getApproximateNearest(float dx, float dy, float dz) {
    Dir result = Dir::NORTH;
    float highestDot = 1.4e-45f; // Float.MIN_VALUE (smallest positive subnormal)
    for (int i = 0; i < 6; ++i) {
        float dot = dx * (float)DIR_NX[i] + dy * (float)DIR_NY[i] + dz * (float)DIR_NZ[i];
        if (dot > highestDot) {
            highestDot = dot;
            result = static_cast<Dir>(i);
        }
    }
    return result;
}

// ── com.mojang.math.Transformation (the subset BlockMath needs) ───────────────
// A Transformation wraps a Matrix4f. The two constructors BlockMath touches:
//  * Transformation(Matrix4f)              -> stores matrix as-is
//  * Transformation(null, leftRotation, null, null) [LOCAL_TO_GLOBAL map entries]
//       -> matrix = compose(null,left,null,null) = new Matrix4f().rotate(left)
// and the ops:
//  * compose(that): copy this matrix, mul(that.matrix)
//  * inverse():     copy this matrix, invertAffine()  (IDENTITY returns itself)
struct Transformation {
    joml::Matrix4f matrix;
    bool isIdentitySingleton = false; // mirrors `this == Transformation.IDENTITY`

    Transformation() = default;
    explicit Transformation(const joml::Matrix4f& m) : matrix(m) {}

    // Transformation.compose(that): new Matrix4f(this.matrix).mul(that.getMatrix())
    Transformation compose(const Transformation& that) const {
        joml::Matrix4f m = matrix;            // new Matrix4f(this.matrix) (copies properties)
        m.mul(that.matrix);
        return Transformation(m);
    }

    // Transformation.inverse(): IDENTITY -> this; else getMatrixCopy().invertAffine().
    // (Callers here only inverse the precomputed map entries, all finite.)
    Transformation inverse() const {
        if (isIdentitySingleton) {
            return *this;
        }
        joml::Matrix4f m = matrix;            // getMatrixCopy()
        return Transformation(m.invertAffine());
    }

    // Transformation.IDENTITY = new Transformation(new Matrix4f()) with decomposed
    // identity fields; for BlockMath the only observable trait is the matrix
    // (identity) and the `this == IDENTITY` short-circuit in inverse().
    static Transformation identity() {
        Transformation t;                      // matrix defaults to JOML identity
        t.isIdentitySingleton = true;
        return t;
    }
};

// ── VANILLA_UV_TRANSFORM_LOCAL_TO_GLOBAL ──────────────────────────────────────
// Indexed by Dir (declaration order). SOUTH = Transformation.IDENTITY; the others
// are new Transformation(null, new Quaternionf().rotate{Y,X}(angle), null, null).
// rotate(quat) on a fresh identity Matrix4f dispatches to rotation(quat).
inline joml::Matrix4f rotationMatrixFromQuat(const joml::Quaternionf& q) {
    joml::Matrix4f m;          // identity
    m.rotation(q);             // identity-dispatch path of Matrix4f.rotate(quat)
    return m;
}

inline const std::array<Transformation, 6>& localToGlobal() {
    static const std::array<Transformation, 6> MAP = [] {
        std::array<Transformation, 6> a;
        // SOUTH -> Transformation.IDENTITY
        a[(int)Dir::SOUTH] = Transformation::identity();
        // EAST  -> rotateY(+PI/2)
        a[(int)Dir::EAST]  = Transformation(rotationMatrixFromQuat(
            joml::Quaternionf{}.rotateY((float)(3.14159265358979323846 / 2.0))));
        // WEST  -> rotateY(-PI/2)
        a[(int)Dir::WEST]  = Transformation(rotationMatrixFromQuat(
            joml::Quaternionf{}.rotateY((float)(-3.14159265358979323846 / 2.0))));
        // NORTH -> rotateY(PI)
        a[(int)Dir::NORTH] = Transformation(rotationMatrixFromQuat(
            joml::Quaternionf{}.rotateY((float)(3.14159265358979323846))));
        // UP    -> rotateX(-PI/2)
        a[(int)Dir::UP]    = Transformation(rotationMatrixFromQuat(
            joml::Quaternionf{}.rotateX((float)(-3.14159265358979323846 / 2.0))));
        // DOWN  -> rotateX(+PI/2)
        a[(int)Dir::DOWN]  = Transformation(rotationMatrixFromQuat(
            joml::Quaternionf{}.rotateX((float)(3.14159265358979323846 / 2.0))));
        return a;
    }();
    return MAP;
}

// VANILLA_UV_TRANSFORM_GLOBAL_TO_LOCAL = Util.mapValues(LOCAL_TO_GLOBAL, inverse).
inline const std::array<Transformation, 6>& globalToLocal() {
    static const std::array<Transformation, 6> MAP = [] {
        const auto& l2g = localToGlobal();
        std::array<Transformation, 6> a;
        for (int i = 0; i < 6; ++i) {
            a[i] = l2g[i].inverse();
        }
        return a;
    }();
    return MAP;
}

// ── BlockMath.getFaceTransformation(transformation, originalSide) ─────────────
inline Transformation getFaceTransformation(const Transformation& transformation, Dir originalSide) {
    // if (MatrixUtil.isIdentity(transformation.getMatrix())) return transformation;
    if (joml::matrixIsIdentity(transformation.matrix)) {
        return transformation;
    }
    // faceAction = VANILLA_UV_TRANSFORM_LOCAL_TO_GLOBAL.get(originalSide);
    Transformation faceAction = localToGlobal()[(int)originalSide];
    // faceAction = transformation.compose(faceAction);
    faceAction = transformation.compose(faceAction);
    // transformedNormal = faceAction.getMatrix().transformDirection(new Vector3f(0,0,1));
    joml::Vector3f transformedNormal{0.0f, 0.0f, 1.0f};
    faceAction.matrix.transformDirection(transformedNormal);
    // newSide = Direction.getApproximateNearest(x, y, z);
    Dir newSide = getApproximateNearest(transformedNormal.x, transformedNormal.y, transformedNormal.z);
    // return VANILLA_UV_TRANSFORM_GLOBAL_TO_LOCAL.get(newSide).compose(faceAction);
    return globalToLocal()[(int)newSide].compose(faceAction);
}

} // namespace mc::core::block_math
