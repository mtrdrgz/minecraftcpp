#pragma once

#include "DecorationDriver.h"

#include <sstream>
#include <string>

namespace mc::levelgen::feature {

// Text dump for parity auditing. This format is intentionally simple and stable
// so a Java exporter can produce the same rows for direct diffing:
//
//   decoration_seed=<seed>
//   step=<step> index=<featureIndex> seed=<featureSeed> feature=<featureKey>
//
// It is not used for placement and should remain side-effect free.
inline std::string dumpDecorationDriverResult(const DecorationDriverResult& result) {
    std::ostringstream out;
    out << "decoration_seed=" << result.decorationSeed << '\n';
    for (const DecorationSeedCall& call : result.calls) {
        out << "step=" << call.step
            << " index=" << call.featureIndex
            << " seed=" << call.featureSeed
            << " feature=" << call.featureKey
            << '\n';
    }
    return out.str();
}

} // namespace mc::levelgen::feature
