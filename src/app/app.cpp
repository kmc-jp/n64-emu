#include "vulkan_headers.hpp"
#include <volk.h>
#include <wsi.hpp>

namespace N64 {
namespace Frontend {

using namespace Vulkan;

WSI *wsi;
VkInstance instance{};

} // namespace Frontend
} // namespace N64