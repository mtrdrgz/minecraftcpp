#pragma once

// 1:1 port of net.minecraft.network.protocol.game.VecDeltaCodec (26.1.2).
//
// (Java path: net/minecraft/network/protocol/game/VecDeltaCodec.java — the class
//  lives under network/protocol/game, NOT world/entity, but is placed here under
//  world/entity per the parity-gate assignment; behaviour is identical.)
//
// Packs an entity's position into quantized longs for ClientboundMoveEntityPacket:
// each coordinate is multiplied by TRUNCATION_STEPS = 4096.0 and rounded to a long
// via java.lang.Math.round(double). The codec is STATEFUL — it holds a base Vec3
// and emits/decodes deltas relative to that base.
//
// The ONLY non-trivial primitive is encode(): Java uses java.lang.Math.round(double)
// which is NOT (long)(x + 0.5) (that double-rounds 0.49999999999999994 up). We reuse
// the certified bit-exact JDK implementation mc::javaMathRound from JavaMath.h.
//
// Certified by vec_delta_codec_parity.

#include "../phys/Vec3.h"               // mc::Vec3 (certified by vec3_parity)
#include "../phys/shapes/JavaMath.h"    // mc::javaMathRound — bit-exact Math.round(double)

namespace mc {

class VecDeltaCodec {
public:
    // private static final double TRUNCATION_STEPS = 4096.0;
    static constexpr double TRUNCATION_STEPS = 4096.0;

    // @VisibleForTesting static long encode(final double input)
    //   return Math.round(input * 4096.0);
    static int64_t encode(double input) {
        return mc::javaMathRound(input * 4096.0);
    }

    // @VisibleForTesting static double decode(final long v)
    //   return v / 4096.0;
    static double decode(int64_t v) {
        // long / double in Java widens v to double then divides.
        return (double)v / 4096.0;
    }

    // public Vec3 decode(final long xa, final long ya, final long za)
    Vec3 decode(int64_t xa, int64_t ya, int64_t za) const {
        if (xa == 0L && ya == 0L && za == 0L) {
            return base;
        }
        double x = xa == 0L ? base.x : decode(encode(base.x) + xa);
        double y = ya == 0L ? base.y : decode(encode(base.y) + ya);
        double z = za == 0L ? base.z : decode(encode(base.z) + za);
        return Vec3(x, y, z);
    }

    // public long encodeX(final Vec3 pos)  return encode(pos.x) - encode(this.base.x);
    int64_t encodeX(const Vec3& pos) const { return encode(pos.x) - encode(base.x); }
    // public long encodeY(final Vec3 pos)  return encode(pos.y) - encode(this.base.y);
    int64_t encodeY(const Vec3& pos) const { return encode(pos.y) - encode(base.y); }
    // public long encodeZ(final Vec3 pos)  return encode(pos.z) - encode(this.base.z);
    int64_t encodeZ(const Vec3& pos) const { return encode(pos.z) - encode(base.z); }

    // public Vec3 delta(final Vec3 pos)  return pos.subtract(this.base);
    Vec3 delta(const Vec3& pos) const { return pos.subtract(base); }

    // public void setBase(final Vec3 base)  this.base = base;
    void setBase(const Vec3& b) { base = b; }

    // public Vec3 getBase()  return this.base;
    const Vec3& getBase() const { return base; }

private:
    // private Vec3 base = Vec3.ZERO;
    Vec3 base{0.0, 0.0, 0.0};
};

} // namespace mc
