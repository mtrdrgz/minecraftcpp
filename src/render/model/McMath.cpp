#include "McMath.h"

#include <cstdlib>
#include <stdexcept>
#include <vector>

namespace mc::render::model {

// ── Direction ────────────────────────────────────────────────────────────────
std::optional<Direction> directionByName(const std::string& name) {
    for (int i = 0; i < 6; i++) {
        if (name == directionInfo((Direction)i).name) return (Direction)i;
    }
    return std::nullopt;
}

Direction directionFromAxisAndDirection(Axis axis, AxisDirection dir) {
    for (int i = 0; i < 6; i++) {
        const DirectionInfo& info = directionInfo((Direction)i);
        if (info.axis == axis && info.axisDirection == dir) return (Direction)i;
    }
    std::abort();
}

Axis axisByName(const std::string& name) {
    if (name == "x") return Axis::X;
    if (name == "y") return Axis::Y;
    if (name == "z") return Axis::Z;
    throw std::runtime_error("Invalid rotation axis: " + name);
}

Direction getApproximateNearest(float dx, float dy, float dz) {
    Direction result = Direction::NORTH;
    float highestDot = 1.4e-45f; // Float.MIN_VALUE (smallest positive denormal)
    for (int i = 0; i < 6; i++) {
        const DirectionInfo& info = directionInfo((Direction)i);
        float dot = dx * (float)info.nx + dy * (float)info.ny + dz * (float)info.nz;
        if (dot > highestDot) {
            highestDot = dot;
            result = (Direction)i;
        }
    }
    return result;
}

Direction rotateDirection(const Matrix4f& matrix, Direction facing) {
    Vector3f vec;
    matrix.transformDirection(directionUnitVec(facing), vec);
    return getApproximateNearest(vec.x, vec.y, vec.z);
}

// ── SymmetricGroup3 ──────────────────────────────────────────────────────────
namespace {

struct Sym3Tables {
    SymmetricGroup3 values[6];
    int cayley[6][6];   // ordinal of first.compose(second)
    int inverse[6];

    Sym3Tables() {
        // Enum order: P123(0,1,2), P213(1,0,2), P132(0,2,1), P312(2,0,1), P231(1,2,0), P321(2,1,0)
        const int perms[6][3] = { {0,1,2}, {1,0,2}, {0,2,1}, {2,0,1}, {1,2,0}, {2,1,0} };
        for (int i = 0; i < 6; i++) {
            values[i].ordinal = i;
            values[i].p0 = perms[i][0];
            values[i].p1 = perms[i][1];
            values[i].p2 = perms[i][2];
            // transformation = new Matrix3f().zero().set(permute(0),0,1).set(permute(1),1,1).set(permute(2),2,1)
            values[i].transformation.zero();
            values[i].transformation.set(values[i].permute(0), 0, 1.0f);
            values[i].transformation.set(values[i].permute(1), 1, 1.0f);
            values[i].transformation.set(values[i].permute(2), 2, 1.0f);
        }
        // CAYLEY_TABLE: result p_i = first.permute(second.p_i)
        for (int f = 0; f < 6; f++) {
            for (int s = 0; s < 6; s++) {
                int p0 = values[f].permute(values[s].p0);
                int p1 = values[f].permute(values[s].p1);
                int p2 = values[f].permute(values[s].p2);
                int found = -1;
                for (int k = 0; k < 6; k++) {
                    if (values[k].p0 == p0 && values[k].p1 == p1 && values[k].p2 == p2) { found = k; break; }
                }
                cayley[f][s] = found;
            }
        }
        for (int f = 0; f < 6; f++) {
            for (int s = 0; s < 6; s++) {
                if (cayley[f][s] == 0) { inverse[f] = s; break; }
            }
        }
    }
};

const Sym3Tables& sym3() {
    static const Sym3Tables T;
    return T;
}

} // namespace

const SymmetricGroup3& SymmetricGroup3::value(int ordinal) { return sym3().values[ordinal]; }

const SymmetricGroup3& SymmetricGroup3::compose(const SymmetricGroup3& first, const SymmetricGroup3& second) {
    return sym3().values[sym3().cayley[first.ordinal][second.ordinal]];
}

const SymmetricGroup3& SymmetricGroup3::inverse() const { return sym3().values[sym3().inverse[ordinal]]; }

// ── OctahedralGroup ──────────────────────────────────────────────────────────
namespace {

// Permutation ordinals: P123=0, P213=1, P132=2, P312=3, P231=4, P321=5
struct OctaInit { const char* name; int perm; bool ix, iy, iz; };
constexpr OctaInit OCTA_INIT[48] = {
    { "identity",          0, false, false, false },
    { "rot_180_face_xy",   0, true,  true,  false },
    { "rot_180_face_xz",   0, true,  false, true  },
    { "rot_180_face_yz",   0, false, true,  true  },
    { "rot_120_nnn",       4, false, false, false },
    { "rot_120_nnp",       3, true,  false, true  },
    { "rot_120_npn",       3, false, true,  true  },
    { "rot_120_npp",       4, true,  false, true  },
    { "rot_120_pnn",       3, true,  true,  false },
    { "rot_120_pnp",       4, true,  true,  false },
    { "rot_120_ppn",       4, false, true,  true  },
    { "rot_120_ppp",       3, false, false, false },
    { "rot_180_edge_xy_neg", 1, true,  true,  true  },
    { "rot_180_edge_xy_pos", 1, false, false, true  },
    { "rot_180_edge_xz_neg", 5, true,  true,  true  },
    { "rot_180_edge_xz_pos", 5, false, true,  false },
    { "rot_180_edge_yz_neg", 2, true,  true,  true  },
    { "rot_180_edge_yz_pos", 2, true,  false, false },
    { "rot_90_x_neg",      2, false, false, true  },
    { "rot_90_x_pos",      2, false, true,  false },
    { "rot_90_y_neg",      5, true,  false, false },
    { "rot_90_y_pos",      5, false, false, true  },
    { "rot_90_z_neg",      1, false, true,  false },
    { "rot_90_z_pos",      1, true,  false, false },
    { "inversion",         0, true,  true,  true  },
    { "invert_x",          0, true,  false, false },
    { "invert_y",          0, false, true,  false },
    { "invert_z",          0, false, false, true  },
    { "rot_60_ref_nnn",    3, true,  true,  true  },
    { "rot_60_ref_nnp",    4, true,  false, false },
    { "rot_60_ref_npn",    4, false, false, true  },
    { "rot_60_ref_npp",    3, false, false, true  },
    { "rot_60_ref_pnn",    4, false, true,  false },
    { "rot_60_ref_pnp",    3, true,  false, false },
    { "rot_60_ref_ppn",    3, false, true,  false },
    { "rot_60_ref_ppp",    4, true,  true,  true  },
    { "swap_xy",           1, false, false, false },
    { "swap_yz",           2, false, false, false },
    { "swap_xz",           5, false, false, false },
    { "swap_neg_xy",       1, true,  true,  false },
    { "swap_neg_yz",       2, false, true,  true  },
    { "swap_neg_xz",       5, true,  false, true  },
    { "rot_90_ref_x_neg",  2, true,  false, true  },
    { "rot_90_ref_x_pos",  2, true,  true,  false },
    { "rot_90_ref_y_neg",  5, true,  true,  false },
    { "rot_90_ref_y_pos",  5, false, true,  true  },
    { "rot_90_ref_z_neg",  1, false, true,  true  },
    { "rot_90_ref_z_pos",  1, true,  false, true  },
};

struct OctaTables {
    OctahedralGroup values[48];
    int cayley[48][48];

    static int trace(bool ix, bool iy, bool iz, int permOrdinal) {
        int inversionIndex = (iz ? 4 : 0) + (iy ? 2 : 0) + (ix ? 1 : 0);
        return permOrdinal << 3 | inversionIndex;
    }

    OctaTables() {
        int byTrace[64];
        for (int i = 0; i < 64; i++) byTrace[i] = -1;
        for (int i = 0; i < 48; i++) {
            OctahedralGroup& g = values[i];
            g.ordinal = i;
            g.name = OCTA_INIT[i].name;
            g.permutation = &SymmetricGroup3::value(OCTA_INIT[i].perm);
            g.invertX = OCTA_INIT[i].ix;
            g.invertY = OCTA_INIT[i].iy;
            g.invertZ = OCTA_INIT[i].iz;
            // transformation = Matrix3f().scaling(±1,±1,±1).mul(permutation.transformation())
            g.transformation.scaling(g.invertX ? -1.0f : 1.0f, g.invertY ? -1.0f : 1.0f, g.invertZ ? -1.0f : 1.0f);
            g.transformation.mul(g.permutation->transformation);
            byTrace[trace(g.invertX, g.invertY, g.invertZ, OCTA_INIT[i].perm)] = i;
        }
        for (int f = 0; f < 48; f++) {
            for (int s = 0; s < 48; s++) {
                const OctahedralGroup& first = values[f];
                const OctahedralGroup& second = values[s];
                const SymmetricGroup3& composedPerm = SymmetricGroup3::compose(*second.permutation, *first.permutation);
                bool cx = first.inverts(Axis::X) != second.inverts(first.permutation->permuteAxis(Axis::X));
                bool cy = first.inverts(Axis::Y) != second.inverts(first.permutation->permuteAxis(Axis::Y));
                bool cz = first.inverts(Axis::Z) != second.inverts(first.permutation->permuteAxis(Axis::Z));
                cayley[f][s] = byTrace[trace(cx, cy, cz, composedPerm.ordinal)];
            }
        }
    }
};

const OctaTables& octa() {
    static const OctaTables T;
    return T;
}

} // namespace

const OctahedralGroup& OctahedralGroup::value(int ordinal) { return octa().values[ordinal]; }
const OctahedralGroup& OctahedralGroup::identity() { return octa().values[0]; }

const OctahedralGroup& OctahedralGroup::compose(const OctahedralGroup& that) const {
    return octa().values[octa().cayley[ordinal][that.ordinal]];
}

Direction OctahedralGroup::rotate(Direction direction) const {
    // Java: newAxis = permutation.inverse().permuteAxis(oldAxis);
    //       newDirection = inverts(newAxis) ? oldDirection.opposite() : oldDirection
    const DirectionInfo& info = directionInfo(direction);
    Axis newAxis = permutation->inverse().permuteAxis(info.axis);
    AxisDirection newDir = inverts(newAxis)
        ? (info.axisDirection == AxisDirection::POSITIVE ? AxisDirection::NEGATIVE : AxisDirection::POSITIVE)
        : info.axisDirection;
    return directionFromAxisAndDirection(newAxis, newDir);
}

const OctahedralGroup& OctahedralGroup::blockRot(Axis axis, int quadrantOrdinal) {
    // OctahedralGroup BLOCK_ROT aliases (enum ordinals):
    //   X: R0=identity(0), R90=ROT_90_X_NEG(18), R180=ROT_180_FACE_YZ(3), R270=ROT_90_X_POS(19)
    //   Y: R0=identity(0), R90=ROT_90_Y_NEG(20), R180=ROT_180_FACE_XZ(2), R270=ROT_90_Y_POS(21)
    //   Z: R0=identity(0), R90=ROT_90_Z_NEG(22), R180=ROT_180_FACE_XY(1), R270=ROT_90_Z_POS(23)
    static const int TABLE[3][4] = {
        { 0, 18, 3, 19 },
        { 0, 20, 2, 21 },
        { 0, 22, 1, 23 },
    };
    return octa().values[TABLE[(int)axis][quadrantOrdinal]];
}

// ── Quadrant ─────────────────────────────────────────────────────────────────
Quadrant quadrantParseJson(int degrees) {
    switch (positiveModulo(degrees, 360)) {
        case 0:   return Quadrant::R0;
        case 90:  return Quadrant::R90;
        case 180: return Quadrant::R180;
        case 270: return Quadrant::R270;
        default:  throw std::runtime_error("Invalid rotation " + std::to_string(degrees) + " found, only 0/90/180/270 allowed");
    }
}

const OctahedralGroup& quadrantFromXYZAngles(Quadrant x, Quadrant y, Quadrant z) {
    const OctahedralGroup& rx = OctahedralGroup::blockRot(Axis::X, (int)x);
    const OctahedralGroup& ry = OctahedralGroup::blockRot(Axis::Y, (int)y);
    const OctahedralGroup& rz = OctahedralGroup::blockRot(Axis::Z, (int)z);
    return rz.compose(ry.compose(rx));
}

// ── Transformation ───────────────────────────────────────────────────────────
const Transformation& Transformation::identityRef() {
    static const Transformation T = [] {
        Transformation t; // matrix defaults to identity (properties=30)
        t.isIdentityRef = true;
        return t;
    }();
    return T;
}

// ── BlockMath ────────────────────────────────────────────────────────────────
namespace BlockMath {

namespace {

constexpr float HALF_PI_F = (float)(3.14159265358979323846 / 2.0);
constexpr float PI_F = (float)3.14159265358979323846;

// new Transformation(null, quat, null, null):
//   matrix = new Matrix4f(); rotate(quat)  [identity -> rotation(quat)]
Transformation fromLeftRotation(const Quaternionf& q) {
    Matrix4f m;
    m.rotate(q);
    return Transformation(m);
}

struct UvTransforms {
    Transformation localToGlobal[6];
    Transformation globalToLocal[6];

    UvTransforms() {
        // SOUTH: Transformation.IDENTITY
        localToGlobal[(int)Direction::SOUTH] = Transformation::identityRef();
        // EAST:  rotateY(+PI/2); WEST: rotateY(-PI/2); NORTH: rotateY(PI)
        // UP:    rotateX(-PI/2); DOWN: rotateX(+PI/2)
        { Quaternionf q; q.rotateY(HALF_PI_F);  localToGlobal[(int)Direction::EAST]  = fromLeftRotation(q); }
        { Quaternionf q; q.rotateY(-HALF_PI_F); localToGlobal[(int)Direction::WEST]  = fromLeftRotation(q); }
        { Quaternionf q; q.rotateY(PI_F);       localToGlobal[(int)Direction::NORTH] = fromLeftRotation(q); }
        { Quaternionf q; q.rotateX(-HALF_PI_F); localToGlobal[(int)Direction::UP]    = fromLeftRotation(q); }
        { Quaternionf q; q.rotateX(HALF_PI_F);  localToGlobal[(int)Direction::DOWN]  = fromLeftRotation(q); }
        for (int i = 0; i < 6; i++) globalToLocal[i] = localToGlobal[i].inverse();
    }
};

const UvTransforms& uv() {
    static const UvTransforms T;
    return T;
}

} // namespace

const Transformation& uvTransformLocalToGlobal(Direction d) { return uv().localToGlobal[(int)d]; }
const Transformation& uvTransformGlobalToLocal(Direction d) { return uv().globalToLocal[(int)d]; }

Transformation getFaceTransformation(const Transformation& transformation, Direction originalSide) {
    if (joml::matrixIsIdentity(transformation.getMatrix())) {
        return transformation;
    }
    Transformation faceAction = transformation.compose(uvTransformLocalToGlobal(originalSide));
    Vector3f transformedNormal;
    faceAction.getMatrix().transformDirection(Vector3f(0.0f, 0.0f, 1.0f), transformedNormal);
    Direction newSide = getApproximateNearest(transformedNormal.x, transformedNormal.y, transformedNormal.z);
    return uvTransformGlobalToLocal(newSide).compose(faceAction);
}

} // namespace BlockMath

} // namespace mc::render::model
