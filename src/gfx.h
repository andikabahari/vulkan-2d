#pragma once

// -----------------------------------------------------------------------------

#define VK_CHECK_RESULT(v) ASSERT((v) == VK_SUCCESS)

struct Vulkan_Context {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkAllocationCallbacks *allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
};

internal Vulkan_Context *vk_init();
internal void vk_cleanup(Vulkan_Context *context);

// -----------------------------------------------------------------------------

global const char *vk_validation_layer_names[] = {
    "VK_LAYER_KHRONOS_validation",
};

internal bool vk_check_validation_layer_support();

global const char *vk_required_extension_names[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    "VK_KHR_win32_surface",
};

internal void vk_create_instance(Vulkan_Context *context);
