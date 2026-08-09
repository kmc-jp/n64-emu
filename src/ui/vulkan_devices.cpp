#include "ui/vulkan_devices.h"
#include "utils/log.h"
#include <cctype>
#include <cstdio>
#include <cstring>

namespace N64 {
namespace Ui {

namespace {

bool physical_device_has_prdp_storage(VkPhysicalDevice gpu) {
    VkPhysicalDevice16BitStorageFeatures storage_16bit = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_16BIT_STORAGE_FEATURES};
    VkPhysicalDevice8BitStorageFeatures storage_8bit = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_8BIT_STORAGE_FEATURES};
    storage_8bit.pNext = &storage_16bit;

    VkPhysicalDeviceFeatures2 features2 = {
        VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};
    features2.pNext = &storage_8bit;
    vkGetPhysicalDeviceFeatures2(gpu, &features2);

    return storage_8bit.storageBuffer8BitAccess &&
           storage_16bit.storageBuffer16BitAccess;
}

std::string normalize_uuid(std::string_view uuid) {
    std::string out;
    out.reserve(VK_UUID_SIZE * 2);
    for (char c : uuid) {
        if (c == '-' || c == ' ' || c == '{' || c == '}')
            continue;
        if (!std::isxdigit(static_cast<unsigned char>(c)))
            return {};
        out.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    if (out.size() != VK_UUID_SIZE * 2)
        return {};
    return out;
}

} // namespace

std::string vulkan_uuid_to_string(const uint8_t uuid[VK_UUID_SIZE]) {
    char buf[VK_UUID_SIZE * 2 + 1];
    for (size_t i = 0; i < VK_UUID_SIZE; ++i)
        std::snprintf(buf + i * 2, 3, "%02x", uuid[i]);
    return std::string(buf);
}

const char *vulkan_device_type_name(VkPhysicalDeviceType type) {
    switch (type) {
    case VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU:
        return "Discrete";
    case VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU:
        return "Integrated";
    case VK_PHYSICAL_DEVICE_TYPE_CPU:
        return "CPU";
    case VK_PHYSICAL_DEVICE_TYPE_VIRTUAL_GPU:
        return "Virtual";
    default:
        return "Other";
    }
}

std::vector<VulkanDeviceInfo> enumerate_vulkan_devices(VkInstance instance) {
    std::vector<VulkanDeviceInfo> out;
    if (instance == VK_NULL_HANDLE)
        return out;

    uint32_t count = 0;
    if (vkEnumeratePhysicalDevices(instance, &count, nullptr) != VK_SUCCESS ||
        count == 0)
        return out;

    std::vector<VkPhysicalDevice> gpus(count);
    if (vkEnumeratePhysicalDevices(instance, &count, gpus.data()) != VK_SUCCESS)
        return out;

    out.reserve(count);
    for (VkPhysicalDevice gpu : gpus) {
        VkPhysicalDeviceIDProperties id_props = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ID_PROPERTIES};
        VkPhysicalDeviceProperties2 props2 = {
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};
        props2.pNext = &id_props;
        vkGetPhysicalDeviceProperties2(gpu, &props2);

        VulkanDeviceInfo info;
        info.handle = gpu;
        info.name = props2.properties.deviceName;
        info.type = props2.properties.deviceType;
        info.uuid = vulkan_uuid_to_string(id_props.deviceUUID);
        info.supports_prdp =
            props2.properties.apiVersion >= VK_API_VERSION_1_1 &&
            physical_device_has_prdp_storage(gpu);
        out.push_back(std::move(info));
    }
    return out;
}

const VulkanDeviceInfo *
find_device_by_uuid(const std::vector<VulkanDeviceInfo> &devices,
                     std::string_view uuid) {
    const std::string key = normalize_uuid(uuid);
    if (key.empty())
        return nullptr;
    for (const auto &d : devices) {
        if (normalize_uuid(d.uuid) == key)
            return &d;
    }
    return nullptr;
}

bool init_wsi_with_device(Vulkan::WSI &wsi, unsigned num_thread_indices,
                          const Vulkan::Context::SystemHandles &system_handles,
                          std::string_view preferred_uuid) {
    auto &platform = wsi.get_platform();
    auto instance_ext = platform.get_instance_extensions();
    auto device_ext = platform.get_device_extensions();
    auto new_context = Util::make_handle<Vulkan::Context>();

    new_context->set_application_info(platform.get_application_info());
    new_context->set_num_thread_indices(num_thread_indices);
    new_context->set_system_handles(system_handles);

    constexpr Vulkan::ContextCreationFlags flags =
        Vulkan::CONTEXT_CREATION_ENABLE_ADVANCED_WSI_BIT;

    if (!new_context->init_instance(instance_ext.data(), instance_ext.size(),
                                    flags)) {
        Utils::critical("Failed to create Vulkan instance");
        return false;
    }

    VkPhysicalDevice preferred = VK_NULL_HANDLE;
    const std::string preferred_key = normalize_uuid(preferred_uuid);
    if (!preferred_key.empty()) {
        const auto devices =
            enumerate_vulkan_devices(new_context->get_instance());
        if (const VulkanDeviceInfo *match =
                find_device_by_uuid(devices, preferred_key)) {
            preferred = match->handle;
            Utils::info("Preferred Vulkan device: {} ({})", match->name,
                        match->uuid);
        } else {
            Utils::warn(
                "Vulkan device UUID {} not found; falling back to auto-select",
                std::string(preferred_uuid));
        }
    }

    VkSurfaceKHR tmp_surface =
        platform.create_surface(new_context->get_instance(), preferred);

    if (!new_context->init_device(preferred, tmp_surface, device_ext.data(),
                                  device_ext.size(), flags)) {
        Utils::critical("Failed to create Vulkan device");
        if (tmp_surface)
            platform.destroy_surface(new_context->get_instance(), tmp_surface);
        return false;
    }

    if (tmp_surface)
        platform.destroy_surface(new_context->get_instance(), tmp_surface);

    if (!wsi.init_from_existing_context(std::move(new_context)))
        return false;
    if (!wsi.init_device())
        return false;
    if (!wsi.init_surface_swapchain())
        return false;
    return true;
}

} // namespace Ui
} // namespace N64
