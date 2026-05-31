#pragma once

#include "Climate.h"
#include <string>
#include <vector>

namespace mc::levelgen {

std::vector<Climate::ParameterList<std::string>::Entry> buildOverworldBiomePreset();
std::vector<Climate::ParameterList<std::string>::Entry> buildNetherBiomePreset();

} // namespace mc::levelgen
