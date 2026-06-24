#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace mc::levelgen::Climate {

inline int64_t quantizeCoord(float coord) {
    return static_cast<int64_t>(coord * 10000.0f);
}

inline float unquantizeCoord(int64_t coord) {
    return static_cast<float>(coord) / 10000.0f;
}

struct Parameter {
    int64_t min = 0;
    int64_t max = 0;

    static Parameter point(float value) {
        return span(value, value);
    }

    static Parameter span(float minValue, float maxValue) {
        if (minValue > maxValue) {
            throw std::invalid_argument("Climate::Parameter min > max");
        }
        return { quantizeCoord(minValue), quantizeCoord(maxValue) };
    }

    static Parameter span(Parameter minValue, Parameter maxValue) {
        if (minValue.min > maxValue.max) {
            throw std::invalid_argument("Climate::Parameter min > max");
        }
        return { minValue.min, maxValue.max };
    }

    int64_t distance(int64_t target) const {
        const int64_t above = target - max;
        const int64_t below = min - target;
        return above > 0 ? above : (below > 0 ? below : 0);
    }

    int64_t distance(Parameter target) const {
        const int64_t above = target.min - max;
        const int64_t below = min - target.max;
        return above > 0 ? above : (below > 0 ? below : 0);
    }

    Parameter span(Parameter other) const {
        return { min < other.min ? min : other.min, max > other.max ? max : other.max };
    }
};

struct TargetPoint {
    int64_t temperature = 0;
    int64_t humidity = 0;
    int64_t continentalness = 0;
    int64_t erosion = 0;
    int64_t depth = 0;
    int64_t weirdness = 0;
};

inline TargetPoint target(float temperature, float humidity, float continentalness,
                          float erosion, float depth, float weirdness) {
    return {
        quantizeCoord(temperature),
        quantizeCoord(humidity),
        quantizeCoord(continentalness),
        quantizeCoord(erosion),
        quantizeCoord(depth),
        quantizeCoord(weirdness)
    };
}

struct ParameterPoint {
    Parameter temperature;
    Parameter humidity;
    Parameter continentalness;
    Parameter erosion;
    Parameter depth;
    Parameter weirdness;
    int64_t offset = 0;

    int64_t fitness(const TargetPoint& targetPoint) const {
        auto square = [](int64_t value) {
            return value * value;
        };
        return square(temperature.distance(targetPoint.temperature))
            + square(humidity.distance(targetPoint.humidity))
            + square(continentalness.distance(targetPoint.continentalness))
            + square(erosion.distance(targetPoint.erosion))
            + square(depth.distance(targetPoint.depth))
            + square(weirdness.distance(targetPoint.weirdness))
            + square(offset);
    }
};

inline ParameterPoint parameters(Parameter temperature, Parameter humidity,
                                 Parameter continentalness, Parameter erosion,
                                 Parameter depth, Parameter weirdness, float offset) {
    return { temperature, humidity, continentalness, erosion, depth, weirdness, quantizeCoord(offset) };
}

inline ParameterPoint parameters(float temperature, float humidity, float continentalness,
                                 float erosion, float depth, float weirdness, float offset) {
    return parameters(Parameter::point(temperature), Parameter::point(humidity),
                      Parameter::point(continentalness), Parameter::point(erosion),
                      Parameter::point(depth), Parameter::point(weirdness), offset);
}

// Port of Climate.RTree (the production biome-search index). Java's
// ParameterList.findValue() does NOT use brute force; it builds this 6-ary
// R-tree over the parameter points and performs an exact branch-and-bound
// nearest-neighbour search. The result is identical to brute force except at
// distance ties, where the leaf returned depends on tree-traversal order (and
// on the previous query, via the lastResult seed) rather than list order.
// Replicating it exactly is required for 1:1 biome selection parity.
template <typename T>
class RTree {
public:
    struct Leaf;

    struct Node {
        std::array<Parameter, 7> parameterSpace{};
        virtual ~Node() = default;

        int64_t distance(const std::array<int64_t, 7>& target) const {
            int64_t total = 0;
            for (int i = 0; i < 7; ++i) {
                const int64_t d = parameterSpace[i].distance(target[i]);
                total += d * d;
            }
            return total;
        }

        virtual const Leaf* search(const std::array<int64_t, 7>& target, const Leaf* candidate) const = 0;
    };

    struct Leaf : Node {
        T value;
        Leaf(const ParameterPoint& point, T v) : value(std::move(v)) {
            this->parameterSpace = parameterSpaceOf(point);
        }
        const Leaf* search(const std::array<int64_t, 7>&, const Leaf*) const override { return this; }
    };

    struct SubTree : Node {
        std::vector<std::shared_ptr<Node>> children;
        explicit SubTree(std::vector<std::shared_ptr<Node>> ch) : children(std::move(ch)) {
            this->parameterSpace = buildParameterSpace(children);
        }
        const Leaf* search(const std::array<int64_t, 7>& target, const Leaf* candidate) const override {
            int64_t minDistance = candidate == nullptr ? std::numeric_limits<int64_t>::max() : candidate->distance(target);
            const Leaf* closestLeaf = candidate;
            for (const auto& child : children) {
                const int64_t childDistance = child->distance(target);
                if (minDistance > childDistance) {
                    const Leaf* leaf = child->search(target, closestLeaf);
                    const int64_t leafDistance =
                        (static_cast<const void*>(child.get()) == static_cast<const void*>(leaf)) ? childDistance
                                                                                                  : leaf->distance(target);
                    if (minDistance > leafDistance) {
                        minDistance = leafDistance;
                        closestLeaf = leaf;
                    }
                }
            }
            return closestLeaf;
        }
    };

    static RTree<T> create(const std::vector<std::pair<ParameterPoint, T>>& values) {
        if (values.empty()) {
            throw std::invalid_argument("Need at least one value to build the search tree.");
        }
        std::vector<std::shared_ptr<Node>> leaves;
        leaves.reserve(values.size());
        for (const auto& v : values) {
            leaves.push_back(std::make_shared<Leaf>(v.first, v.second));
        }
        return RTree<T>(build(7, std::move(leaves)));
    }

    const T& search(const TargetPoint& target) const {
        const std::array<int64_t, 7> arr = { target.temperature, target.humidity, target.continentalness,
                                             target.erosion, target.depth, target.weirdness, 0 };
        // Java uses a ThreadLocal<Leaf> lastResult — each thread gets its own seed.
        // C++ must mirror this with thread_local to avoid data races when multiple
        // worker threads call getNoiseBiome concurrently.
        thread_local const Leaf* tl_lastResult = nullptr;
        const Leaf* leaf = m_root->search(arr, tl_lastResult);
        tl_lastResult = leaf;
        return leaf->value;
    }

private:
    std::shared_ptr<Node> m_root;

    explicit RTree(std::shared_ptr<Node> root) : m_root(std::move(root)) {}

    static int64_t absl(int64_t v) { return v < 0 ? -v : v; }
    static int64_t center(const Node& n, int dim) {
        const Parameter& p = n.parameterSpace[dim];
        return (p.min + p.max) / 2;
    }

    static std::array<Parameter, 7> parameterSpaceOf(const ParameterPoint& p) {
        return { p.temperature, p.humidity, p.continentalness, p.erosion, p.depth, p.weirdness, Parameter{ p.offset, p.offset } };
    }

    static std::array<Parameter, 7> buildParameterSpace(const std::vector<std::shared_ptr<Node>>& children) {
        std::array<std::optional<Parameter>, 7> bounds{};
        for (const auto& child : children) {
            for (int d = 0; d < 7; ++d) {
                const Parameter& cp = child->parameterSpace[d];
                bounds[d] = bounds[d] ? Parameter{ std::min(cp.min, bounds[d]->min), std::max(cp.max, bounds[d]->max) } : cp;
            }
        }
        std::array<Parameter, 7> out{};
        for (int d = 0; d < 7; ++d) {
            out[d] = *bounds[d];
        }
        return out;
    }

    // Lexicographic comparison by box centre across all dimensions, starting at
    // `dimension` and wrapping (Climate.RTree.comparator chained via thenComparing).
    static bool less(const Node& a, const Node& b, int dimension, bool absolute) {
        for (int d = 0; d < 7; ++d) {
            const int dim = (dimension + d) % 7;
            int64_t ca = center(a, dim);
            int64_t cb = center(b, dim);
            if (absolute) {
                ca = absl(ca);
                cb = absl(cb);
            }
            if (ca != cb) {
                return ca < cb;
            }
        }
        return false;
    }

    static void sortNodes(std::vector<std::shared_ptr<Node>>& children, int dimension, bool absolute) {
        // std::stable_sort matches Java List.sort (TimSort) tie behaviour.
        std::stable_sort(children.begin(), children.end(),
                         [&](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
                             return less(*a, *b, dimension, absolute);
                         });
    }

    static int64_t cost(const std::array<Parameter, 7>& ps) {
        int64_t result = 0;
        for (const auto& p : ps) {
            result += absl(p.max - p.min);
        }
        return result;
    }

    static std::vector<std::shared_ptr<SubTree>> bucketize(const std::vector<std::shared_ptr<Node>>& nodes) {
        std::vector<std::shared_ptr<SubTree>> buckets;
        std::vector<std::shared_ptr<Node>> children;
        const int expectedChildrenCount =
            static_cast<int>(std::pow(6.0, std::floor(std::log(static_cast<double>(nodes.size()) - 0.01) / std::log(6.0))));
        for (const auto& child : nodes) {
            children.push_back(child);
            if (static_cast<int>(children.size()) >= expectedChildrenCount) {
                buckets.push_back(std::make_shared<SubTree>(children));
                children.clear();
            }
        }
        if (!children.empty()) {
            buckets.push_back(std::make_shared<SubTree>(children));
        }
        return buckets;
    }

    static std::shared_ptr<Node> build(int dimensions, std::vector<std::shared_ptr<Node>> children) {
        if (children.empty()) {
            throw std::runtime_error("Need at least one child to build a node");
        }
        if (children.size() == 1) {
            return children[0];
        }
        if (children.size() <= 6) {
            std::stable_sort(children.begin(), children.end(),
                             [&](const std::shared_ptr<Node>& a, const std::shared_ptr<Node>& b) {
                                 int64_t ma = 0;
                                 int64_t mb = 0;
                                 for (int dx = 0; dx < dimensions; ++dx) {
                                     ma += absl(center(*a, dx));
                                     mb += absl(center(*b, dx));
                                 }
                                 return ma < mb;
                             });
            return std::make_shared<SubTree>(std::move(children));
        }

        int64_t minCost = std::numeric_limits<int64_t>::max();
        int minDimension = -1;
        std::vector<std::shared_ptr<SubTree>> minBuckets;
        for (int d = 0; d < dimensions; ++d) {
            sortNodes(children, d, false);
            std::vector<std::shared_ptr<SubTree>> buckets = bucketize(children);
            int64_t totalCost = 0;
            for (const auto& bucket : buckets) {
                totalCost += cost(bucket->parameterSpace);
            }
            if (minCost > totalCost) {
                minCost = totalCost;
                minDimension = d;
                minBuckets = buckets;
            }
        }

        std::stable_sort(minBuckets.begin(), minBuckets.end(),
                         [&](const std::shared_ptr<SubTree>& a, const std::shared_ptr<SubTree>& b) {
                             return less(*a, *b, minDimension, true);
                         });
        std::vector<std::shared_ptr<Node>> built;
        built.reserve(minBuckets.size());
        for (const auto& bucket : minBuckets) {
            built.push_back(build(dimensions, bucket->children));
        }
        return std::make_shared<SubTree>(std::move(built));
    }
};

template <typename T>
class ParameterList {
public:
    using Entry = std::pair<ParameterPoint, T>;

    ParameterList() = default;
    explicit ParameterList(std::vector<Entry> values) : m_values(std::move(values)) {
        if (!m_values.empty()) {
            m_index = std::make_shared<RTree<T>>(RTree<T>::create(m_values));
        }
    }

    const std::vector<Entry>& values() const {
        return m_values;
    }

    bool empty() const {
        return m_values.empty();
    }

    // Mirrors Java ParameterList.findValue -> findValueIndex (production path).
    const T& findValue(const TargetPoint& targetPoint) const {
        if (!m_index) {
            throw std::runtime_error("Climate::ParameterList needs at least one value");
        }
        return m_index->search(targetPoint);
    }

    // Mirrors Java ParameterList.findValueBruteForce (@VisibleForTesting). Kept
    // for parity testing against the R-tree path.
    const T& findValueBruteForce(const TargetPoint& targetPoint) const {
        if (m_values.empty()) {
            throw std::runtime_error("Climate::ParameterList needs at least one value");
        }

        const Entry* best = &m_values.front();
        int64_t bestFitness = best->first.fitness(targetPoint);
        for (const Entry& entry : m_values) {
            const int64_t fitness = entry.first.fitness(targetPoint);
            if (fitness < bestFitness) {
                bestFitness = fitness;
                best = &entry;
            }
        }
        return best->second;
    }

private:
    std::vector<Entry> m_values;
    std::shared_ptr<RTree<T>> m_index;
};

} // namespace mc::levelgen::Climate
