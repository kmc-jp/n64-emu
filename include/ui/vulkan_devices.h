#pragma once

#include "wsi.hpp"
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace N64 {
namespace Ui {

struct VulkanDeviceInfo {
    std::string uuid;
    std::string name;
    VkPhysicalDeviceType type{VK_PHYSICAL_DEVICE_TYPE_OTHER};
    bool supports_prdp{false};
    VkPhysicalDevice handle{VK_NULL_HANDLE};
};

std::string vulkan_uuid_to_string(const uint8_t uuid[VK_UUID_SIZE]);
const char *vulkan_device_type_name(VkPhysicalDeviceType type);

std::vector<VulkanDeviceInfo> enumerate_vulkan_devices(VkInstance instance);
const VulkanDeviceInfo *find_device_by_uuid(
    const std::vector<VulkanDeviceInfo> &devices, std::string_view uuid);

// Like WSI::init_simple, but selects a physical device by UUID when non-empty.
// Falls back to auto-select (with a warning) if the UUID is missing/unknown.
bool init_wsi_with_device(Vulkan::WSI &wsi, unsigned num_thread_indices,
                          const Vulkan::Context::SystemHandles &system_handles,
                          std::string_view preferred_uuid);

} // namespace Ui
} // namespace N64
