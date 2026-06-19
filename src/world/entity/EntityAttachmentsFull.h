#pragma once

// 1:1 port of net.minecraft.world.entity.EntityAttachments (Minecraft 26.1.2) —
// the FULL class, including the multi-point Builder.attach path and the rotation-
// bearing accessors that the existing world/entity/EntityDimensions.h port
// deliberately left OUT (it ports only the single-point DEFAULT construction used
// by EntityDimensions; see its header note). This header lives in its own
// namespace so it never collides with mc::EntityAttachments.
//
// Java methods ported here (EntityAttachments.java + EntityAttachment.java):
//   static EntityAttachments createDefault(float width, float height)
//   static Builder builder()
//   EntityAttachments scale(float x, float y, float z)
//   Vec3* getNullable(EntityAttachment, int index, float rotY)   (nullptr == Java null)
//   Vec3  get(EntityAttachment, int index, float rotY)           (throws if null)
//   Vec3  getAverage(EntityAttachment)
//   Vec3  getClamped(EntityAttachment, int index, float rotY)
//   private static Vec3 transformPoint(Vec3 point, float rotY)
//   Builder.attach(EntityAttachment, float x,y,z) / attach(EntityAttachment, Vec3)
//   Builder.build(float width, float height)
//   EntityAttachment.createFallbackPoints(width, height) (AT_FEET/AT_HEIGHT/AT_CENTER)
//
// Bit-exact dependencies, all already certified:
//   - Vec3.yRot(float) uses Mth.cos/sin (the TABLE), not std::sin/cos.
//   - transformPoint angle is -rotY * (float)(Math.PI/180.0) == -rotY * DEG_TO_RAD.
//   - getAverage divides by 1.0F / size as a FLOAT, then Vec3.scale(double) widens it.
//   - vec3.multiply / new Vec3(float,float,float) widen float -> double.

#include "world/phys/Vec3.h"
#include "world/level/levelgen/Mth.h"

#include <array>
#include <stdexcept>
#include <vector>

namespace mc::entity_attachments {

// Mirrors net.minecraft.world.entity.EntityAttachment (enum, declaration order).
enum class EntityAttachment : int {
    PASSENGER = 0,     // Fallback.AT_HEIGHT
    VEHICLE = 1,       // Fallback.AT_FEET
    NAME_TAG = 2,      // Fallback.AT_HEIGHT
    WARDEN_CHEST = 3,  // Fallback.AT_CENTER
};

inline constexpr int ENTITY_ATTACHMENT_COUNT = 4;

// EntityAttachment.createFallbackPoints(width, height): the per-type fallback used
// by Builder.build when no explicit point was attached. Returns exactly one Vec3.
//   AT_FEET   -> Vec3.ZERO                       (VEHICLE)
//   AT_HEIGHT -> new Vec3(0.0, height, 0.0)      (PASSENGER, NAME_TAG)
//   AT_CENTER -> new Vec3(0.0, height / 2.0, 0.0)(WARDEN_CHEST)
// `width` is unused by every fallback (matches Java — the lambdas ignore width).
inline std::vector<Vec3> createFallbackPoints(EntityAttachment a, float /*width*/, float height) {
    switch (a) {
        case EntityAttachment::VEHICLE:
            return { Vec3(0.0, 0.0, 0.0) };
        case EntityAttachment::PASSENGER:
        case EntityAttachment::NAME_TAG:
            // new Vec3(0.0, height, 0.0): float height widens to double.
            return { Vec3(0.0, static_cast<double>(height), 0.0) };
        case EntityAttachment::WARDEN_CHEST:
            // new Vec3(0.0, height / 2.0, 0.0): height widens to double BEFORE /2.0.
            return { Vec3(0.0, static_cast<double>(height) / 2.0, 0.0) };
    }
    return { Vec3(0.0, 0.0, 0.0) };  // unreachable
}

class EntityAttachments {
public:
    // points[ordinal] == the (insertion-ordered) list for that attachment type.
    std::array<std::vector<Vec3>, ENTITY_ATTACHMENT_COUNT> attachments;

    EntityAttachments() = default;
    explicit EntityAttachments(std::array<std::vector<Vec3>, ENTITY_ATTACHMENT_COUNT> a)
        : attachments(std::move(a)) {}

    class Builder;
    static Builder builder();

    // EntityAttachments.createDefault(width, height) = builder().build(width, height).
    static EntityAttachments createDefault(float width, float height);

    // scale(float x, float y, float z): every stored point *= (x, y, z); each float
    // widens to double inside Vec3.multiply (matching vec3.multiply(x, y, z)).
    EntityAttachments scale(float x, float y, float z) const {
        std::array<std::vector<Vec3>, ENTITY_ATTACHMENT_COUNT> out;
        for (int i = 0; i < ENTITY_ATTACHMENT_COUNT; ++i) {
            std::vector<Vec3> list;
            list.reserve(attachments[static_cast<std::size_t>(i)].size());
            for (const Vec3& v : attachments[static_cast<std::size_t>(i)]) {
                list.push_back(v.multiply(static_cast<double>(x),
                                          static_cast<double>(y),
                                          static_cast<double>(z)));
            }
            out[static_cast<std::size_t>(i)] = std::move(list);
        }
        return EntityAttachments(std::move(out));
    }

    // transformPoint(point, rotY) = point.yRot(-rotY * (float)(Math.PI/180.0)).
    static Vec3 transformPoint(const Vec3& point, float rotY) {
        return point.yRot(-rotY * mc::levelgen::mth::DEG_TO_RAD);
    }

    // getNullable(attachment, index, rotY): transformed point in range, else "null".
    // Returns true + writes `out` when present; returns false for the null case.
    bool getNullable(EntityAttachment attachment, int index, float rotY, Vec3& out) const {
        const std::vector<Vec3>& points = attachments[static_cast<std::size_t>(attachment)];
        if (index >= 0 && index < static_cast<int>(points.size())) {
            out = transformPoint(points[static_cast<std::size_t>(index)], rotY);
            return true;
        }
        return false;
    }

    // get(attachment, index, rotY): throws (Java IllegalStateException) when null.
    Vec3 get(EntityAttachment attachment, int index, float rotY) const {
        Vec3 out;
        if (!getNullable(attachment, index, rotY, out)) {
            throw std::logic_error("Had no attachment point of type for index");
        }
        return out;
    }

    // getAverage(attachment): sum (Vec3.add) then scale(1.0F / size) — the divisor
    // is a FLOAT (1.0F / int), widened to double by Vec3.scale(double).
    Vec3 getAverage(EntityAttachment attachment) const {
        const std::vector<Vec3>& points = attachments[static_cast<std::size_t>(attachment)];
        if (points.empty()) {
            throw std::logic_error("No attachment points of type: PASSENGER");
        }
        Vec3 sum(0.0, 0.0, 0.0);  // Vec3.ZERO
        for (const Vec3& point : points) {
            sum = sum.add(point);
        }
        float inv = 1.0F / static_cast<float>(points.size());
        return sum.scale(static_cast<double>(inv));
    }

    // getClamped(attachment, index, rotY): Mth.clamp(index, 0, size-1) then transform.
    Vec3 getClamped(EntityAttachment attachment, int index, float rotY) const {
        const std::vector<Vec3>& points = attachments[static_cast<std::size_t>(attachment)];
        if (points.empty()) {
            throw std::logic_error("Had no attachment points of type");
        }
        int idx = mc::levelgen::mth::clamp(index, 0,
                                           static_cast<int>(points.size()) - 1);
        return transformPoint(points[static_cast<std::size_t>(idx)], rotY);
    }
};

class EntityAttachments::Builder {
public:
    // Per-type insertion-ordered point lists (empty == "absent" -> fallback at build).
    std::array<std::vector<Vec3>, ENTITY_ATTACHMENT_COUNT> attachments;
    // Tracks which types were explicitly attached vs. never touched (computeIfAbsent
    // distinguishes a present-but-empty list from absence; in practice attach always
    // adds, so any touched type is non-empty, but we track it for build's branch).
    std::array<bool, ENTITY_ATTACHMENT_COUNT> present{};

    Builder() = default;

    // attach(EntityAttachment, float x, float y, float z) = attach(a, new Vec3(x,y,z)).
    Builder& attach(EntityAttachment a, float x, float y, float z) {
        return attach(a, Vec3(static_cast<double>(x),
                              static_cast<double>(y),
                              static_cast<double>(z)));
    }
    // attach(EntityAttachment, Vec3): computeIfAbsent(new ArrayList<>(1)).add(point).
    Builder& attach(EntityAttachment a, const Vec3& point) {
        std::size_t i = static_cast<std::size_t>(a);
        present[i] = true;
        attachments[i].push_back(point);
        return *this;
    }

    // build(width, height): for each enum value in declaration order, use the
    // attached list if present else attachment.createFallbackPoints(width, height).
    EntityAttachments build(float width, float height) const {
        std::array<std::vector<Vec3>, ENTITY_ATTACHMENT_COUNT> out;
        for (int i = 0; i < ENTITY_ATTACHMENT_COUNT; ++i) {
            EntityAttachment a = static_cast<EntityAttachment>(i);
            if (present[static_cast<std::size_t>(i)]) {
                out[static_cast<std::size_t>(i)] = attachments[static_cast<std::size_t>(i)];
            } else {
                out[static_cast<std::size_t>(i)] = createFallbackPoints(a, width, height);
            }
        }
        return EntityAttachments(std::move(out));
    }
};

inline EntityAttachments::Builder EntityAttachments::builder() {
    return EntityAttachments::Builder();
}

inline EntityAttachments EntityAttachments::createDefault(float width, float height) {
    return builder().build(width, height);
}

}  // namespace mc::entity_attachments
