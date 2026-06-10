// 1:1 port of net.minecraft.core.Cursor3D (Minecraft 26.1.2).
//
// 3D box iterator. advance() walks the (width x height x depth) box with x
// fastest, then y, then z (Cursor3D.java:30-41). getNextType() classifies the
// current cell as interior(0)/face(1)/edge(2)/corner(3) by counting how many of
// its three coordinates lie on a min/max boundary (Cursor3D.java:55-70).
//
// Every value/constant/formula/order is taken VERBATIM from the decompiled Java
// under 26.1.2/src/net/minecraft/core/Cursor3D.java. New header, no shared deps.
#pragma once

namespace mc {

class Cursor3D {
public:
    // Cursor3D.java:4-7
    static constexpr int TYPE_INSIDE = 0;
    static constexpr int TYPE_FACE = 1;
    static constexpr int TYPE_EDGE = 2;
    static constexpr int TYPE_CORNER = 3;

    // Cursor3D.java:20-28
    Cursor3D(int minX, int minY, int minZ, int maxX, int maxY, int maxZ)
        : originX_(minX),
          originY_(minY),
          originZ_(minZ),
          width_(maxX - minX + 1),
          height_(maxY - minY + 1),
          depth_(maxZ - minZ + 1),
          end_(width_ * height_ * depth_),
          index_(0),
          x_(0),
          y_(0),
          z_(0) {}

    // Cursor3D.java:30-41
    bool advance() {
        if (index_ == end_) {
            return false;
        }

        x_ = index_ % width_;
        int slice = index_ / width_;
        y_ = slice % height_;
        z_ = slice / height_;
        index_++;
        return true;
    }

    // Cursor3D.java:43-45
    int nextX() const { return originX_ + x_; }

    // Cursor3D.java:47-49
    int nextY() const { return originY_ + y_; }

    // Cursor3D.java:51-53
    int nextZ() const { return originZ_ + z_; }

    // Cursor3D.java:55-70
    int getNextType() const {
        int type = 0;
        if (x_ == 0 || x_ == width_ - 1) {
            type++;
        }

        if (y_ == 0 || y_ == height_ - 1) {
            type++;
        }

        if (z_ == 0 || z_ == depth_ - 1) {
            type++;
        }

        return type;
    }

private:
    const int originX_;
    const int originY_;
    const int originZ_;
    const int width_;
    const int height_;
    const int depth_;
    const int end_;
    int index_;
    int x_;
    int y_;
    int z_;
};

} // namespace mc
