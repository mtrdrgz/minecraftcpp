// 1:1 port of com.mojang.math.Transformation (Minecraft 26.1.2), matrix-level
// methods only. Verbatim translation of 26.1.2/src/com/mojang/math/Transformation.java.
//
// PORTED (matrix-level, pure/bounded):
//   - Transformation(Matrix4fc)            constructor that stores the matrix
//   - getMatrix()                          returns the stored matrix (Matrix4fc)
//   - getMatrixCopy()                      new Matrix4f(this.matrix)  (a COPY)
//   - compose(Transformation)              getMatrixCopy().mul(that.getMatrix())
//   - inverse()                            getMatrixCopy().invertAffine(); null if !isFinite()
//   - IDENTITY                             new Matrix4f() (identity)
//
// NOTE: the Java field is named `matrix` and getMatrix() returns the *internal*
// Matrix4fc reference (NOT a copy); getMatrixCopy() is the copy. Both are
// reproduced exactly. inverse() uses Matrix4f.invertAffine() (the affine
// inverse), as in the decompiled source — NOT a general invert().
//
// SKIPPED (need MatrixUtil.svdDecompose / Quaternionf.slerp / Vector3f.lerp,
// none of which are ported): ensureDecomposed(), translation(), leftRotation(),
// scale(), rightRotation(), slerp(), and the Codec/equals/hashCode plumbing.
// Listed in unportedMethods. These are hard-omitted, never stubbed.
//
// Reuses the certified render/model/Joml.h (org.joml 1.10.8 bit-exact subset).
#pragma once

#include "Joml.h"

namespace mc::render::model {

// com.mojang.math.Transformation — matrix-level subset.
struct Transformation {
    // private final Matrix4fc matrix;
    joml::Matrix4f matrix;

    // public Transformation(final Matrix4fc matrix) { this.matrix = matrix; }
    explicit Transformation(const joml::Matrix4f& m) : matrix(m) {}

    // public Matrix4fc getMatrix() { return this.matrix; }
    const joml::Matrix4f& getMatrix() const { return matrix; }

    // public Matrix4f getMatrixCopy() { return new Matrix4f(this.matrix); }
    // org.joml Matrix4f(Matrix4fc) copies all 16 elements AND the properties bits;
    // the default C++ copy of the struct does exactly that.
    joml::Matrix4f getMatrixCopy() const { return matrix; }

    // public Transformation compose(final Transformation that) {
    //    Matrix4f matrix = this.getMatrixCopy();
    //    matrix.mul(that.getMatrix());
    //    return new Transformation(matrix);
    // }
    Transformation compose(const Transformation& that) const {
        joml::Matrix4f m = getMatrixCopy();
        m.mul(that.getMatrix());
        return Transformation(m);
    }

    // public @Nullable Transformation inverse() {
    //    if (this == IDENTITY) return this;            // identity short-circuit
    //    Matrix4f matrix = this.getMatrixCopy().invertAffine();
    //    return matrix.isFinite() ? new Transformation(matrix) : null;
    // }
    // We expose the raw computation; `ok` is the isFinite() result (false => Java null).
    // The IDENTITY short-circuit is by object identity in Java; for an arbitrary
    // identity-valued matrix invertAffine() yields the identity again, so the
    // returned matrix value is unaffected — only the (here-irrelevant) decomposed
    // cache differs. We compute invertAffine() faithfully.
    joml::Matrix4f inverseMatrix(bool& ok) const {
        joml::Matrix4f inv = getMatrixCopy().invertAffine();
        ok = inv.isFinite();
        return inv;
    }

    // public static final Transformation IDENTITY = ... new Transformation(new Matrix4f()) ...
    static Transformation identity() { return Transformation(joml::Matrix4f()); }
};

} // namespace mc::render::model
