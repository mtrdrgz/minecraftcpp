// Self-contained 1:1 port of com.mojang.math.SymmetricGroup3 (Minecraft 26.1.2),
// authored to certify the surfaces the existing octahedral_parity gate does NOT
// cover:
//   * transformation()            — the per-element float Matrix3fc (9 floats × 6)
//   * inverse()                   — the direct INVERSE_TABLE ordinal (octahedral_parity
//                                   only checks it indirectly via compose==identity)
//   * permuteVector(Vector3f)     — float vector permutation (absent from McMath.h)
//   * permuteVector(Vector3i)     — int   vector permutation (absent from McMath.h)
//   * permuteAxis(int)            — direct permutation of an axis ordinal
//
// SymmetricGroup3 is pure enum / permutation / org.joml float math — NO GL / GPU /
// window — so it loads and runs headless. The float matrix is built from
// Matrix3f().zero().set(col,row,1f) (plain assignment, no arithmetic), so every
// element is bit-exact under -ffp-contract=off.
//
// Enum order (ordinals) MUST be: P123, P213, P132, P312, P231, P321 — this drives
// the OctahedralGroup permutation indices, so the ordering is load-bearing.
//
// Source of truth: 26.1.2/src/com/mojang/math/SymmetricGroup3.java + org.joml
// (Joml.h disassembly notes). NO deviation from the Java is permitted.
#pragma once

#include "Joml.h"

#include <array>
#include <cstdint>

namespace mc::render::model {

using joml::Matrix3f;
using joml::Vector3f;

// org.joml.Vector3i — only the get(int)/set(int,int,int) subset permuteVector needs.
// Vector3i.get(int): case 0->x, 1->y, 2->z, default throw; set(x,y,z) writes all.
struct Vector3i {
    int x = 0, y = 0, z = 0;

    Vector3i() = default;
    Vector3i(int x_, int y_, int z_) : x(x_), y(y_), z(z_) {}

    int get(int i) const { return i == 0 ? x : (i == 1 ? y : z); }
    Vector3i& set(int x_, int y_, int z_) { x = x_; y = y_; z = z_; return *this; }
};

// ── com.mojang.math.SymmetricGroup3 ──────────────────────────────────────────
class SymmetricGroup3 {
public:
    // Java enum ordinals.
    enum Value : int { P123 = 0, P213 = 1, P132 = 2, P312 = 3, P231 = 4, P321 = 5 };

    // SymmetricGroup3.values() in declaration order.
    static const std::array<SymmetricGroup3, 6>& values() {
        static const std::array<SymmetricGroup3, 6> V = makeValues();
        return V;
    }

    static const SymmetricGroup3& value(int ordinal) { return values()[ordinal]; }

    int ordinal() const { return ordinal_; }

    // public int permute(int i) — switch(i){0->p0;1->p1;2->p2; default throw}.
    int permute(int i) const {
        // Match Java: out-of-range would throw; the gate never feeds out-of-range.
        return i == 0 ? p0_ : (i == 1 ? p1_ : p2_);
    }

    // permuteAxis(Direction.Axis): Axis.VALUES[permute(axis.ordinal())]. We return
    // the permuted ordinal directly (X=0,Y=1,Z=2 == Axis ordinals).
    int permuteAxis(int axisOrdinal) const { return permute(axisOrdinal); }

    // public SymmetricGroup3 compose(SymmetricGroup3 that)
    //   CAYLEY_TABLE[this.ordinal()][that.ordinal()]
    const SymmetricGroup3& compose(const SymmetricGroup3& that) const {
        return value(cayleyTable()[ordinal_][that.ordinal_]);
    }

    // public SymmetricGroup3 inverse() — INVERSE_TABLE[this.ordinal()].
    const SymmetricGroup3& inverse() const { return value(inverseTable()[ordinal_]); }

    // public Vector3f permuteVector(Vector3f v):
    //   v0 = v.get(p0); v1 = v.get(p1); v2 = v.get(p2); return v.set(v0,v1,v2);
    // NOTE: reads use the PRIVATE p0/p1/p2 fields directly (NOT permute(i)) — they
    // are identical in value (permute(0)==p0 …) but this mirrors the bytecode.
    Vector3f& permuteVector(Vector3f& v) const {
        float v0 = v.get(p0_);
        float v1 = v.get(p1_);
        float v2 = v.get(p2_);
        return v.set(v0, v1, v2);
    }

    // public Vector3i permuteVector(Vector3i v): same shape, ints.
    Vector3i& permuteVector(Vector3i& v) const {
        int v0 = v.get(p0_);
        int v1 = v.get(p1_);
        int v2 = v.get(p2_);
        return v.set(v0, v1, v2);
    }

    // public Matrix3fc transformation():
    //   new Matrix3f().zero().set(permute(0),0,1f).set(permute(1),1,1f).set(permute(2),2,1f)
    const Matrix3f& transformation() const { return transformation_; }

private:
    int ordinal_ = 0;
    int p0_ = 0, p1_ = 0, p2_ = 0;
    Matrix3f transformation_;

    SymmetricGroup3() = default;
    SymmetricGroup3(int ordinal, int p0, int p1, int p2)
        : ordinal_(ordinal), p0_(p0), p1_(p1), p2_(p2) {
        // Matrix3f().zero().set(permute(0),0,1).set(permute(1),1,1).set(permute(2),2,1)
        transformation_.zero();
        transformation_.set(permute(0), 0, 1.0f);
        transformation_.set(permute(1), 1, 1.0f);
        transformation_.set(permute(2), 2, 1.0f);
    }

    static std::array<SymmetricGroup3, 6> makeValues() {
        // P123(0,1,2) P213(1,0,2) P132(0,2,1) P312(2,0,1) P231(1,2,0) P321(2,1,0)
        return {
            SymmetricGroup3(0, 0, 1, 2),
            SymmetricGroup3(1, 1, 0, 2),
            SymmetricGroup3(2, 0, 2, 1),
            SymmetricGroup3(3, 2, 0, 1),
            SymmetricGroup3(4, 1, 2, 0),
            SymmetricGroup3(5, 2, 1, 0),
        };
    }

    // CAYLEY_TABLE: for each (first,second), result has
    //   p0 = first.permute(second.p0), p1 = first.permute(second.p1),
    //   p2 = first.permute(second.p2); ordinal = the value matching those.
    static const std::array<std::array<int, 6>, 6>& cayleyTable() {
        static const auto T = [] {
            const auto& V = values();
            std::array<std::array<int, 6>, 6> table{};
            for (int f = 0; f < 6; ++f)
                for (int s = 0; s < 6; ++s) {
                    int rp0 = V[f].permute(V[s].p0_);
                    int rp1 = V[f].permute(V[s].p1_);
                    int rp2 = V[f].permute(V[s].p2_);
                    for (int k = 0; k < 6; ++k)
                        if (V[k].p0_ == rp0 && V[k].p1_ == rp1 && V[k].p2_ == rp2) { table[f][s] = k; break; }
                }
            return table;
        }();
        return T;
    }

    // INVERSE_TABLE: for each f, the s with f.compose(s) == P123.
    static const std::array<int, 6>& inverseTable() {
        static const auto T = [] {
            const auto& tab = cayleyTable();
            std::array<int, 6> inv{};
            for (int f = 0; f < 6; ++f)
                for (int s = 0; s < 6; ++s)
                    if (tab[f][s] == P123) { inv[f] = s; break; }
            return inv;
        }();
        return T;
    }
};

} // namespace mc::render::model
