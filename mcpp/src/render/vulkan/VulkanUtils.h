#pragma once
#include <volk.h>
#include "../../core/Log.h"
#include <stdexcept>
#include <string>

namespace mc::render {

inline void checkVk(VkResult hr, const char* msg) {
    if (hr != VK_SUCCESS) {
        MC_LOG_ERROR("Vulkan Error: {} (code={})", msg, (int)hr);
        throw std::runtime_error(std::string("Vulkan Error: ") + msg);
    }
}

#define VK_CHECK(expr) mc::render::checkVk(expr, #expr)

} // namespace mc::render
