#pragma once

// 1:1 port of net.minecraft.world.entity.EntityDimensions (Minecraft 26.1.2) —
// the immutable record (width, height, eyeHeight, attachments, fixed) describing
// an entity's bounding-box footprint. Pure double/float arithmetic + the AABB and
// Vec3 ports. Certified by entity_dimensions_parity.
//
// Java methods ported here (EntityDimensions.java):
//   - private EntityDimensions(width, height, fixed)  (defaultEyeHeight + createDefault)
//   - private static float defaultEyeHeight(height) = height * 0.85F
//   - AABB makeBoundingBox(Vec3 pos) / makeBoundingBox(double x, y, z)
//   - EntityDimensions scale(float) / scale(float, float)
//   - static EntityDimensions scalable(width, height)
//   - static EntityDimensions fixed(width, height)
//   - EntityDimensions withEyeHeight(float)
//
// The `attachments` field (EntityAttachments) is ALSO ported here, because the
// DEFAULT attachments produced by EntityAttachments.createDefault(width,height)
// (the only construction path EntityDimensions ever uses) depend on nothing but
// Vec3 + the EntityAttachment enum fallbacks. The full Builder.attach(...) path,
// withAttachments(Builder), and the rotation-bearing accessors (get/getNullable/
// getAverage/getClamped/transformPoint) are NOT ported — see unportedMethods.
//
// EntityAttachment fallbacks (EntityAttachment.java):
//   PASSENGER    -> AT_HEIGHT -> [ Vec3(0, height,     0) ]
//   VEHICLE      -> AT_FEET   -> [ Vec3(0, 0,          0) ]   (Fallback.ZERO)
//   NAME_TAG     -> AT_HEIGHT -> [ Vec3(0, height,     0) ]
//   WARDEN_CHEST -> AT_CENTER -> [ Vec3(0, height/2.0, 0) ]

#include "world/phys/AABB.h"
#include "world/phys/Vec3.h"

#include <array>

namespace mc {

// Mirrors net.minecraft.world.entity.EntityAttachment (enum, in declaration order).
enum class EntityAttachment : int {
    PASSENGER = 0,
    VEHICLE = 1,
    NAME_TAG = 2,
    WARDEN_CHEST = 3,
};

// Port of EntityAttachments — but only the DEFAULT-construction subset that
// EntityDimensions relies on. Each of the 4 attachment types defaults to exactly
// one Vec3 fallback point (createDefault uses the empty Builder, so every type
// falls back to attachment.createFallbackPoints(width, height)).
struct EntityAttachments {
    // One default fallback point per EntityAttachment (index == enum ordinal).
    std::array<Vec3, 4> points{};

    constexpr EntityAttachments() = default;
    constexpr explicit EntityAttachments(const std::array<Vec3, 4>& p) : points(p) {}

    // Java: EntityAttachments.createDefault(width, height) = builder().build(w, h),
    // and since the builder has no explicit attaches, every type uses its fallback.
    static EntityAttachments createDefault(float width, float height) {
        // Vec3 ctor widens float -> double, matching new Vec3(0.0, height, 0.0) etc.
        EntityAttachments a;
        a.points[(int)EntityAttachment::PASSENGER]    = Vec3(0.0, (double)(float)height, 0.0);
        a.points[(int)EntityAttachment::VEHICLE]      = Vec3(0.0, 0.0, 0.0);               // ZERO
        a.points[(int)EntityAttachment::NAME_TAG]     = Vec3(0.0, (double)(float)height, 0.0);
        a.points[(int)EntityAttachment::WARDEN_CHEST] = Vec3(0.0, (double)height / 2.0, 0.0);
        return a;
    }

    // Java: EntityAttachments.scale(float x, float y, float z) multiplies every
    // stored point by (x, y, z). vec3.multiply(x, y, z) widens each float -> double.
    EntityAttachments scale(float x, float y, float z) const {
        EntityAttachments out;
        for (int i = 0; i < 4; ++i) {
            out.points[i] = points[i].multiply((double)x, (double)y, (double)z);
        }
        return out;
    }
};

struct EntityDimensions {
    float width = 0.0F;
    float height = 0.0F;
    float eyeHeight = 0.0F;
    EntityAttachments attachments{};
    bool fixed = false;

    constexpr EntityDimensions() = default;
    constexpr EntityDimensions(float width_, float height_, float eyeHeight_,
                               const EntityAttachments& attachments_, bool fixed_)
        : width(width_), height(height_), eyeHeight(eyeHeight_),
          attachments(attachments_), fixed(fixed_) {}

    // Java: private static float defaultEyeHeight(float height) = height * 0.85F;
    static constexpr float defaultEyeHeight(float height) { return height * 0.85F; }

    // Java: private EntityDimensions(width, height, fixed)
    static EntityDimensions make(float width, float height, bool fixed) {
        return EntityDimensions(width, height, defaultEyeHeight(height),
                                EntityAttachments::createDefault(width, height), fixed);
    }

    // Java: makeBoundingBox(double x, double y, double z)
    //   float w = this.width / 2.0F; float h = this.height;
    //   new AABB(x - w, y, z - w, x + w, y + h, z + w);
    // NOTE: w and h are FLOAT; each (x - w) etc. widens float -> double before the
    // double subtraction/addition, so we must keep w/h as float, exactly as Java.
    AABB makeBoundingBox(double x, double y, double z) const {
        float w = width / 2.0F;
        float h = height;
        return AABB(x - (double)w, y, z - (double)w,
                    x + (double)w, y + (double)h, z + (double)w);
    }

    AABB makeBoundingBox(const Vec3& pos) const {
        return makeBoundingBox(pos.x, pos.y, pos.z);
    }

    // Java: scale(float widthScaleFactor, float heightScaleFactor)
    EntityDimensions scale(float widthScaleFactor, float heightScaleFactor) const {
        if (!fixed && (widthScaleFactor != 1.0F || heightScaleFactor != 1.0F)) {
            return EntityDimensions(
                width * widthScaleFactor,
                height * heightScaleFactor,
                eyeHeight * heightScaleFactor,
                attachments.scale(widthScaleFactor, heightScaleFactor, widthScaleFactor),
                false);
        }
        return *this;
    }
    EntityDimensions scale(float scaleFactor) const { return scale(scaleFactor, scaleFactor); }

    // Java: static EntityDimensions scalable(width, height) -> new(width, height, false)
    static EntityDimensions scalable(float width, float height) { return make(width, height, false); }
    // Java: static EntityDimensions fixed(width, height) -> new(width, height, true).
    // Renamed makeFixed in C++ because the boolean record component is named `fixed`
    // and C++ (unlike Java) forbids a static method sharing a data-member's name.
    static EntityDimensions makeFixed(float width, float height) { return make(width, height, true); }

    // Java: withEyeHeight(float eyeHeight)
    EntityDimensions withEyeHeight(float newEyeHeight) const {
        return EntityDimensions(width, height, newEyeHeight, attachments, fixed);
    }
};

} // namespace mc
