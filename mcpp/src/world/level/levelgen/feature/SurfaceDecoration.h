#pragma once

#include "../../chunk/LevelChunk.h"
#include <cstdint>
#include <functional>
#include <string>

namespace mc::levelgen::feature {

void decorateSurface(LevelChunk& chunk, uint64_t worldSeed, const std::function<std::string(int, int, int)>& biomeGetter);

} // namespace mc::levelgen::feature
