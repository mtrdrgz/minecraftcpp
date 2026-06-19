#pragma once

// 1:1 port of net.minecraft.world.level.pathfinder.BinaryHeap (26.1.2).
//
// A min-heap (by Node.f, the A* total cost) with a backing array that grows by
// doubling. Every node carries its own heapIdx so changeCost/remove can locate
// the element in O(1) and then sift. The port reproduces the Java VERBATIM:
//   * the comparison is the STRICT `cost < other.f` (Java `!(a < b)` breaks),
//     so equal costs do NOT sift — tie ordering is fully determined by the op
//     sequence, exactly as in vanilla.
//   * downHeap uses Float.POSITIVE_INFINITY as the right-child sentinel when the
//     right child is out of range (BinaryHeap.java:111). We use the same IEEE-754
//     +inf so the `leftCost < rightCost` branch picks left identically.
//   * the backing array starts at length 128 and doubles (size << 1) on overflow
//     (BinaryHeap.java:6,14-18). We replicate the doubling so the *capacity* path
//     is exercised, though capacity has no observable effect on ordering.
//   * pop()/remove() pull `heap[--size]` into the hole then sift; insert() places
//     at `heap[size]` then upHeaps `size++` (post-increment) — order preserved.
//
// The heap only ever touches Node.f and Node.heapIdx, so we model a Node as a
// minimal struct holding exactly those two mutable fields plus an immutable `id`
// used purely by the parity test to report pop order. This is NOT a re-port of
// the full pathfinder Node (see Node.h for the certified pure helpers); it is the
// faithful subset BinaryHeap manipulates. No value/formula is invented here.

#include <cstdint>
#include <limits>
#include <vector>

namespace mc::pathfinder {

// Minimal stand-in for the two BinaryHeap-visible fields of
// net.minecraft.world.level.pathfinder.Node:
//   public int heapIdx = -1;   (Node.java:14)
//   public float f;            (Node.java:17)
// `id` is parity-test bookkeeping (not present in Java); it never affects heap
// behaviour, mirroring how Node identity does not influence ordering.
struct HeapNode {
    int id;                 // test-only stable identity
    float f = 0.0f;         // Node.f — the heap key
    int heapIdx = -1;       // Node.heapIdx — slot in the heap array, -1 if absent

    explicit HeapNode(int nid, float cost = 0.0f) : id(nid), f(cost) {}
};

class BinaryHeap {
public:
    // private Node[] heap = new Node[128]; private int size; (BinaryHeap.java:6-7)
    BinaryHeap() : heap_(128, nullptr), size_(0) {}

    // public Node insert(final Node node) — BinaryHeap.java:9-24.
    HeapNode* insert(HeapNode* node) {
        // if (node.heapIdx >= 0) throw new IllegalStateException("OW KNOWS!");
        // (We mirror the guard but never trip it with valid op scripts.)
        if (node->heapIdx >= 0) {
            // Match Java semantics: this is a programming error. Keep behaviour
            // observable rather than silently no-op'ing.
            return node;
        }

        if (size_ == static_cast<int>(heap_.size())) {
            // Node[] newHeap = new Node[this.size << 1]; arraycopy; this.heap = newHeap;
            std::vector<HeapNode*> newHeap(static_cast<std::size_t>(size_) << 1, nullptr);
            for (int i = 0; i < size_; ++i) newHeap[static_cast<std::size_t>(i)] = heap_[static_cast<std::size_t>(i)];
            heap_.swap(newHeap);
        }

        heap_[static_cast<std::size_t>(size_)] = node;
        node->heapIdx = size_;
        upHeap(size_++);
        return node;
    }

    // public void clear() — BinaryHeap.java:26-28.
    void clear() { size_ = 0; }

    // public Node peek() — BinaryHeap.java:30-32.
    HeapNode* peek() { return heap_[0]; }

    // public Node pop() — BinaryHeap.java:34-44.
    HeapNode* pop() {
        HeapNode* popped = heap_[0];
        heap_[0] = heap_[static_cast<std::size_t>(--size_)];
        heap_[static_cast<std::size_t>(size_)] = nullptr;
        if (size_ > 0) {
            downHeap(0);
        }
        popped->heapIdx = -1;
        return popped;
    }

    // public void remove(final Node node) — BinaryHeap.java:46-58.
    void remove(HeapNode* node) {
        const int idx = node->heapIdx;
        heap_[static_cast<std::size_t>(idx)] = heap_[static_cast<std::size_t>(--size_)];
        heap_[static_cast<std::size_t>(size_)] = nullptr;
        if (size_ > idx) {
            if (heap_[static_cast<std::size_t>(idx)]->f < node->f) {
                upHeap(idx);
            } else {
                downHeap(idx);
            }
        }
        node->heapIdx = -1;
    }

    // public void changeCost(final Node node, final float newCost) — BinaryHeap.java:60-68.
    void changeCost(HeapNode* node, float newCost) {
        const float oldCost = node->f;
        node->f = newCost;
        if (newCost < oldCost) {
            upHeap(node->heapIdx);
        } else {
            downHeap(node->heapIdx);
        }
    }

    // public int size() — BinaryHeap.java:70-72.
    int size() const { return size_; }

    // public boolean isEmpty() — BinaryHeap.java:140-142.
    bool isEmpty() const { return size_ == 0; }

    // public Node[] getHeap() — BinaryHeap.java:144-146 (Arrays.copyOf(heap, size)).
    std::vector<HeapNode*> getHeap() const {
        return std::vector<HeapNode*>(heap_.begin(), heap_.begin() + size_);
    }

private:
    // private void upHeap(int idx) — BinaryHeap.java:74-92.
    void upHeap(int idx) {
        HeapNode* node = heap_[static_cast<std::size_t>(idx)];
        const float cost = node->f;

        while (idx > 0) {
            const int parentIdx = (idx - 1) >> 1;
            HeapNode* parent = heap_[static_cast<std::size_t>(parentIdx)];
            if (!(cost < parent->f)) {
                break;
            }
            heap_[static_cast<std::size_t>(idx)] = parent;
            parent->heapIdx = idx;
            idx = parentIdx;
        }

        heap_[static_cast<std::size_t>(idx)] = node;
        node->heapIdx = idx;
    }

    // private void downHeap(int idx) — BinaryHeap.java:94-138.
    void downHeap(int idx) {
        HeapNode* node = heap_[static_cast<std::size_t>(idx)];
        const float cost = node->f;

        while (true) {
            const int leftIdx = 1 + (idx << 1);
            const int rightIdx = leftIdx + 1;
            if (leftIdx >= size_) {
                break;
            }

            HeapNode* leftNode = heap_[static_cast<std::size_t>(leftIdx)];
            const float leftCost = leftNode->f;
            HeapNode* rightNode;
            float rightCost;
            if (rightIdx >= size_) {
                rightNode = nullptr;
                rightCost = std::numeric_limits<float>::infinity();  // Float.POSITIVE_INFINITY
            } else {
                rightNode = heap_[static_cast<std::size_t>(rightIdx)];
                rightCost = rightNode->f;
            }

            if (leftCost < rightCost) {
                if (!(leftCost < cost)) {
                    break;
                }
                heap_[static_cast<std::size_t>(idx)] = leftNode;
                leftNode->heapIdx = idx;
                idx = leftIdx;
            } else {
                if (!(rightCost < cost)) {
                    break;
                }
                heap_[static_cast<std::size_t>(idx)] = rightNode;
                rightNode->heapIdx = idx;
                idx = rightIdx;
            }
        }

        heap_[static_cast<std::size_t>(idx)] = node;
        node->heapIdx = idx;
    }

    std::vector<HeapNode*> heap_;
    int size_;
};

}  // namespace mc::pathfinder
