#pragma once

// 1:1 port of net.minecraft.client.renderer.texture.Stitcher (Stitcher.java, 26.1.2) — the
// deterministic atlas bin-packer that assigns each sprite an (x,y) origin and grows the atlas
// to a power-of-two extent. Its placements drive every sprite's u0/u1/v0/v1
// (TextureAtlasSprite: u0=(x+padding)/atlasWidth, ...), so byte-exact stitching => byte-exact
// atlas UVs. Pure integer math on the certified Mth (smallestEncompassingPowerOfTwo, clamp).
// Certified by stitcher_parity.
//
// Operator-precedence note (Stitcher.java:66): smallestFittingMinTexel is
//   ((input >> mm) + ((input & ((1<<mm)-1)) == 0 ? 0 : 1)) << mm   // round input up to a 2^mm
// (Java '+' binds tighter than '<<'). The comparator tie-break uses Identifier.compareTo =
// path.compareTo then namespace.compareTo (Identifier.java).

#include "../../world/level/levelgen/Mth.h"

#include <algorithm>
#include <memory>
#include <string>
#include <vector>

namespace mc::render::texture {

namespace mth = mc::levelgen::mth;

// Stitcher.Entry: width/height + Identifier name (namespace + path kept separate for compareTo).
struct StitcherEntry {
    int width = 0;
    int height = 0;
    std::string ns;    // Identifier namespace
    std::string path;  // Identifier path
};

// SpriteLoader.load output: the entry's atlas origin (x,y) and the padding in effect.
struct Placement {
    int entryIdx;
    int x;
    int y;
    int padding;
};

class Stitcher {
public:
    Stitcher(int maxWidth, int maxHeight, int mipLevel, int anisotropyBit)
        : mipLevel_(mipLevel), maxWidth_(maxWidth), maxHeight_(maxHeight) {
        padding_ = (1 << mipLevel) << mth::clamp(anisotropyBit - 1, 0, 4);
    }

    int getWidth() const { return storageX_; }
    int getHeight() const { return storageY_; }
    int padding() const { return padding_; }

    void registerSprite(int entryIdx, int w, int h) {
        holders_.push_back(Holder{
            smallestFittingMinTexel(w + padding_ * 2, mipLevel_),
            smallestFittingMinTexel(h + padding_ * 2, mipLevel_),
            entryIdx});
    }

    // Returns false where vanilla throws StitcherException (a holder that doesn't fit).
    bool stitch(const std::vector<StitcherEntry>& entries) {
        std::vector<Holder> sorted = holders_;
        std::stable_sort(sorted.begin(), sorted.end(), [&](const Holder& a, const Holder& b) {
            if (a.height != b.height) return a.height > b.height;  // comparing(-height): height desc
            if (a.width != b.width) return a.width > b.width;      // thenComparing(-width): width desc
            const StitcherEntry& ea = entries[a.entryIdx];
            const StitcherEntry& eb = entries[b.entryIdx];
            int r = ea.path.compare(eb.path);                     // Identifier.compareTo: path first
            if (r == 0) r = ea.ns.compare(eb.ns);                 // then namespace
            return r < 0;
        });
        for (const Holder& h : sorted)
            if (!addToStorage(h)) return false;
        return true;
    }

    void gatherSprites(std::vector<Placement>& out) const {
        for (const auto& region : storage_) region->walk(out, padding_);
    }

private:
    struct Holder { int width, height, entryIdx; };

    struct Region {
        int originX, originY, width, height;
        std::vector<std::unique_ptr<Region>> subSlots;
        bool hasSubSlots = false;
        Holder holder{0, 0, -1};
        bool hasHolder = false;

        Region(int ox, int oy, int w, int h) : originX(ox), originY(oy), width(w), height(h) {}

        bool add(const Holder& h) {
            if (hasHolder) return false;
            int textureWidth = h.width, textureHeight = h.height;
            if (textureWidth <= width && textureHeight <= height) {
                if (textureWidth == width && textureHeight == height) {
                    holder = h;
                    hasHolder = true;
                    return true;
                }
                if (!hasSubSlots) {
                    hasSubSlots = true;
                    subSlots.push_back(std::make_unique<Region>(originX, originY, textureWidth, textureHeight));
                    int spareWidth = width - textureWidth;
                    int spareHeight = height - textureHeight;
                    if (spareHeight > 0 && spareWidth > 0) {
                        int right = std::max(height, spareWidth);
                        int bottom = std::max(width, spareHeight);
                        if (right >= bottom) {
                            subSlots.push_back(std::make_unique<Region>(originX, originY + textureHeight, textureWidth, spareHeight));
                            subSlots.push_back(std::make_unique<Region>(originX + textureWidth, originY, spareWidth, height));
                        } else {
                            subSlots.push_back(std::make_unique<Region>(originX + textureWidth, originY, spareWidth, textureHeight));
                            subSlots.push_back(std::make_unique<Region>(originX, originY + textureHeight, width, spareHeight));
                        }
                    } else if (spareWidth == 0) {
                        subSlots.push_back(std::make_unique<Region>(originX, originY + textureHeight, textureWidth, spareHeight));
                    } else if (spareHeight == 0) {
                        subSlots.push_back(std::make_unique<Region>(originX + textureWidth, originY, spareWidth, textureHeight));
                    }
                }
                for (auto& sub : subSlots)
                    if (sub->add(h)) return true;
                return false;
            }
            return false;
        }

        void walk(std::vector<Placement>& out, int padding) const {
            if (hasHolder) {
                out.push_back(Placement{holder.entryIdx, originX, originY, padding});
            } else if (hasSubSlots) {
                for (const auto& sub : subSlots) sub->walk(out, padding);
            }
        }
    };

    static int smallestFittingMinTexel(int input, int maxMipLevel) {
        int rem = input & ((1 << maxMipLevel) - 1);
        return ((input >> maxMipLevel) + (rem == 0 ? 0 : 1)) << maxMipLevel;
    }

    bool addToStorage(const Holder& h) {
        for (auto& region : storage_)
            if (region->add(h)) return true;
        return expand(h);
    }

    bool expand(const Holder& h) {
        int xCurrentSize = mth::smallestEncompassingPowerOfTwo(storageX_);
        int yCurrentSize = mth::smallestEncompassingPowerOfTwo(storageY_);
        int xNewSize = mth::smallestEncompassingPowerOfTwo(storageX_ + h.width);
        int yNewSize = mth::smallestEncompassingPowerOfTwo(storageY_ + h.height);
        bool xCanGrow = xNewSize <= maxWidth_;
        bool yCanGrow = yNewSize <= maxHeight_;
        if (!xCanGrow && !yCanGrow) return false;

        bool xWillGrow = xCanGrow && xCurrentSize != xNewSize;
        bool yWillGrow = yCanGrow && yCurrentSize != yNewSize;
        bool growOnX;
        if (xWillGrow ^ yWillGrow) {
            growOnX = xWillGrow;
        } else {
            growOnX = xCanGrow && xCurrentSize <= yCurrentSize;
        }

        std::unique_ptr<Region> slot;
        if (growOnX) {
            if (storageY_ == 0) storageY_ = yNewSize;
            slot = std::make_unique<Region>(storageX_, 0, xNewSize - storageX_, storageY_);
            storageX_ = xNewSize;
        } else {
            slot = std::make_unique<Region>(0, storageY_, storageX_, yNewSize - storageY_);
            storageY_ = yNewSize;
        }
        slot->add(h);
        storage_.push_back(std::move(slot));
        return true;
    }

    int mipLevel_, maxWidth_, maxHeight_, padding_;
    int storageX_ = 0, storageY_ = 0;
    std::vector<Holder> holders_;
    std::vector<std::unique_ptr<Region>> storage_;
};

}  // namespace mc::render::texture
