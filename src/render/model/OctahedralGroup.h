#pragma once

// 1:1 port of com.mojang.math.OctahedralGroup (Minecraft 26.1.2) — the 48-element
// octahedral symmetry group used to turn a blockstate variant's x/y/z rotation
// (com.mojang.math.Quadrant) into the ModelState transformation matrix that
// FaceBakery/bakeCuboidGeometry apply to a block model.
//
// Builds entirely on the certified SymmetricGroup3.h (permutation matrices,
// compose, permuteAxis) and the certified render/model/Joml.h Matrix3f/Matrix4f.
// NO new math is invented: every element is the exact (permutation, invertX,
// invertY, invertZ) tuple from the Java enum declaration, and:
//   transformation() = Matrix3f().scaling(±1,±1,±1).mul(permutation.transformation())
//   compose(that)    = the Cayley-table composition built from the trace fingerprint,
//                      exactly as OctahedralGroup's static CAYLEY_TABLE.
//
// Source of truth: 26.1.2/src/com/mojang/math/OctahedralGroup.java and Quadrant.java.

#include "Joml.h"
#include "SymmetricGroup3.h"

#include <array>

namespace mc::render::model {

using joml::Matrix3f;
using joml::Matrix4f;

class OctahedralGroup {
public:
    // Enum ordinals in Java declaration order (load-bearing: drives Quadrant tables).
    enum Value : int {
        IDENTITY = 0,
        ROT_180_FACE_XY, ROT_180_FACE_XZ, ROT_180_FACE_YZ,
        ROT_120_NNN, ROT_120_NNP, ROT_120_NPN, ROT_120_NPP,
        ROT_120_PNN, ROT_120_PNP, ROT_120_PPN, ROT_120_PPP,
        ROT_180_EDGE_XY_NEG, ROT_180_EDGE_XY_POS,
        ROT_180_EDGE_XZ_NEG, ROT_180_EDGE_XZ_POS,
        ROT_180_EDGE_YZ_NEG, ROT_180_EDGE_YZ_POS,
        ROT_90_X_NEG, ROT_90_X_POS,
        ROT_90_Y_NEG, ROT_90_Y_POS,
        ROT_90_Z_NEG, ROT_90_Z_POS,
        INVERSION, INVERT_X, INVERT_Y, INVERT_Z,
        ROT_60_REF_NNN, ROT_60_REF_NNP, ROT_60_REF_NPN, ROT_60_REF_NPP,
        ROT_60_REF_PNN, ROT_60_REF_PNP, ROT_60_REF_PPN, ROT_60_REF_PPP,
        SWAP_XY, SWAP_YZ, SWAP_XZ,
        SWAP_NEG_XY, SWAP_NEG_YZ, SWAP_NEG_XZ,
        ROT_90_REF_X_NEG, ROT_90_REF_X_POS,
        ROT_90_REF_Y_NEG, ROT_90_REF_Y_POS,
        ROT_90_REF_Z_NEG, ROT_90_REF_Z_POS,
        COUNT
    };

    // Aliases used by Quadrant (OctahedralGroup.BLOCK_ROT_*).
    static constexpr int BLOCK_ROT_X_270 = ROT_90_X_POS;
    static constexpr int BLOCK_ROT_X_180 = ROT_180_FACE_YZ;
    static constexpr int BLOCK_ROT_X_90  = ROT_90_X_NEG;
    static constexpr int BLOCK_ROT_Y_270 = ROT_90_Y_POS;
    static constexpr int BLOCK_ROT_Y_180 = ROT_180_FACE_XZ;
    static constexpr int BLOCK_ROT_Y_90  = ROT_90_Y_NEG;
    static constexpr int BLOCK_ROT_Z_270 = ROT_90_Z_POS;
    static constexpr int BLOCK_ROT_Z_180 = ROT_180_FACE_XY;
    static constexpr int BLOCK_ROT_Z_90  = ROT_90_Z_NEG;

    struct Entry { int perm; bool invX, invY, invZ; };

    static const Entry& entry(int ordinal) { return TABLE()[ordinal]; }

    // OctahedralGroup.transformation():
    //   new Matrix3f().scaling(invertX?-1:1, invertY?-1:1, invertZ?-1:1)
    //                 .mul(permutation.transformation())
    static const Matrix3f& transformation(int ordinal) { return matrices()[ordinal]; }

    // BlockModelRotation.transformation() stores new Matrix4f(orientation.transformation()).
    static Matrix4f modelMatrix(int ordinal) { return Matrix4f(transformation(ordinal)); }

    // OctahedralGroup.inverts(axis): X=0,Y=1,Z=2.
    static bool inverts(int ordinal, int axis) {
        const Entry& e = TABLE()[ordinal];
        return axis == 0 ? e.invX : (axis == 1 ? e.invY : e.invZ);
    }

    // this.compose(that) = CAYLEY_TABLE[this][that], computed from the trace fingerprint:
    //   composedPermutation = that.permutation.compose(this.permutation)
    //   composedInvert<A>   = this.inverts(A) ^ that.inverts(this.permutation.permuteAxis(A))
    static int compose(int self, int that) {
        const Entry& a = TABLE()[self];
        const SymmetricGroup3& pa = SymmetricGroup3::value(a.perm);
        const SymmetricGroup3& composedPerm = SymmetricGroup3::value(TABLE()[that].perm).compose(pa);
        bool iX = a.invX ^ inverts(that, pa.permuteAxis(0));
        bool iY = a.invY ^ inverts(that, pa.permuteAxis(1));
        bool iZ = a.invZ ^ inverts(that, pa.permuteAxis(2));
        return fingerprintToOrdinal(trace(composedPerm.ordinal(), iX, iY, iZ));
    }

    // Quadrant.fromXYZAngles(x,y,z) = z.rotationZ.compose(y.rotationY.compose(x.rotationX)).
    // shift is the Quadrant ordinal: R0=0, R90=1, R180=2, R270=3.
    static int fromXYZAngles(int xShift, int yShift, int zShift) {
        return compose(rotZ(zShift), compose(rotY(yShift), rotX(xShift)));
    }

private:
    static int rotX(int shift) {
        static const int M[4] = { IDENTITY, BLOCK_ROT_X_90, BLOCK_ROT_X_180, BLOCK_ROT_X_270 };
        return M[shift & 3];
    }
    static int rotY(int shift) {
        static const int M[4] = { IDENTITY, BLOCK_ROT_Y_90, BLOCK_ROT_Y_180, BLOCK_ROT_Y_270 };
        return M[shift & 3];
    }
    static int rotZ(int shift) {
        static const int M[4] = { IDENTITY, BLOCK_ROT_Z_90, BLOCK_ROT_Z_180, BLOCK_ROT_Z_270 };
        return M[shift & 3];
    }

    // trace(invX,invY,invZ,permutation) = permutation.ordinal()<<3 | inversionIndex.
    static int trace(int permOrdinal, bool invX, bool invY, bool invZ) {
        int inversionIndex = (invZ ? 4 : 0) + (invY ? 2 : 0) + (invX ? 1 : 0);
        return (permOrdinal << 3) | inversionIndex;
    }

    static const std::array<Entry, COUNT>& TABLE() {
        static const std::array<Entry, COUNT> T = makeTable();
        return T;
    }

    static std::array<Entry, COUNT> makeTable() {
        using S = SymmetricGroup3;
        return {{
            { S::P123, false, false, false }, // IDENTITY
            { S::P123, true,  true,  false }, // ROT_180_FACE_XY
            { S::P123, true,  false, true  }, // ROT_180_FACE_XZ
            { S::P123, false, true,  true  }, // ROT_180_FACE_YZ
            { S::P231, false, false, false }, // ROT_120_NNN
            { S::P312, true,  false, true  }, // ROT_120_NNP
            { S::P312, false, true,  true  }, // ROT_120_NPN
            { S::P231, true,  false, true  }, // ROT_120_NPP
            { S::P312, true,  true,  false }, // ROT_120_PNN
            { S::P231, true,  true,  false }, // ROT_120_PNP
            { S::P231, false, true,  true  }, // ROT_120_PPN
            { S::P312, false, false, false }, // ROT_120_PPP
            { S::P213, true,  true,  true  }, // ROT_180_EDGE_XY_NEG
            { S::P213, false, false, true  }, // ROT_180_EDGE_XY_POS
            { S::P321, true,  true,  true  }, // ROT_180_EDGE_XZ_NEG
            { S::P321, false, true,  false }, // ROT_180_EDGE_XZ_POS
            { S::P132, true,  true,  true  }, // ROT_180_EDGE_YZ_NEG
            { S::P132, true,  false, false }, // ROT_180_EDGE_YZ_POS
            { S::P132, false, false, true  }, // ROT_90_X_NEG
            { S::P132, false, true,  false }, // ROT_90_X_POS
            { S::P321, true,  false, false }, // ROT_90_Y_NEG
            { S::P321, false, false, true  }, // ROT_90_Y_POS
            { S::P213, false, true,  false }, // ROT_90_Z_NEG
            { S::P213, true,  false, false }, // ROT_90_Z_POS
            { S::P123, true,  true,  true  }, // INVERSION
            { S::P123, true,  false, false }, // INVERT_X
            { S::P123, false, true,  false }, // INVERT_Y
            { S::P123, false, false, true  }, // INVERT_Z
            { S::P312, true,  true,  true  }, // ROT_60_REF_NNN
            { S::P231, true,  false, false }, // ROT_60_REF_NNP
            { S::P231, false, false, true  }, // ROT_60_REF_NPN
            { S::P312, false, false, true  }, // ROT_60_REF_NPP
            { S::P231, false, true,  false }, // ROT_60_REF_PNN
            { S::P312, true,  false, false }, // ROT_60_REF_PNP
            { S::P312, false, true,  false }, // ROT_60_REF_PPN
            { S::P231, true,  true,  true  }, // ROT_60_REF_PPP
            { S::P213, false, false, false }, // SWAP_XY
            { S::P132, false, false, false }, // SWAP_YZ
            { S::P321, false, false, false }, // SWAP_XZ
            { S::P213, true,  true,  false }, // SWAP_NEG_XY
            { S::P132, false, true,  true  }, // SWAP_NEG_YZ
            { S::P321, true,  false, true  }, // SWAP_NEG_XZ
            { S::P132, true,  false, true  }, // ROT_90_REF_X_NEG
            { S::P132, true,  true,  false }, // ROT_90_REF_X_POS
            { S::P321, true,  true,  false }, // ROT_90_REF_Y_NEG
            { S::P321, false, true,  true  }, // ROT_90_REF_Y_POS
            { S::P213, false, true,  true  }, // ROT_90_REF_Z_NEG
            { S::P213, true,  false, true  }, // ROT_90_REF_Z_POS
        }};
    }

    static const std::array<Matrix3f, COUNT>& matrices() {
        static const std::array<Matrix3f, COUNT> M = makeMatrices();
        return M;
    }

    static std::array<Matrix3f, COUNT> makeMatrices() {
        std::array<Matrix3f, COUNT> M;
        for (int i = 0; i < COUNT; ++i) {
            const Entry& e = TABLE()[i];
            Matrix3f s;
            s.scaling(e.invX ? -1.0f : 1.0f, e.invY ? -1.0f : 1.0f, e.invZ ? -1.0f : 1.0f);
            s.mul(SymmetricGroup3::value(e.perm).transformation());
            M[i] = s;
        }
        return M;
    }

    static int fingerprintToOrdinal(int tr) {
        static const std::array<int, 48> map = makeFingerprint();
        // trace = perm.ordinal()(0..5) << 3 | inversion(0..7); only 48 of 64 used.
        // Compress to a 48-slot dense lookup via (perm*8 + inv) -> but perm<<3 already
        // gives 0..47 since perm in 0..5 and inv in 0..7 -> max 5*8+7 = 47.
        return map[tr];
    }

    static std::array<int, 48> makeFingerprint() {
        std::array<int, 48> map{};
        for (auto& v : map) v = -1;
        for (int i = 0; i < COUNT; ++i) {
            const Entry& e = TABLE()[i];
            map[trace(e.perm, e.invX, e.invY, e.invZ)] = i;
        }
        return map;
    }
};

} // namespace mc::render::model
