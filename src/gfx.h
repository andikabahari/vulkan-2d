#pragma once

// -----------------------------------------------------------------------------

#define VK_CHECK_RESULT(v) ASSERT((v) == VK_SUCCESS)

struct Vulkan_Context {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkAllocationCallbacks *allocator;
    VkDebugUtilsMessengerEXT debug_messenger;
};

internal Vulkan_Context *vk_init(GLFWwindow *window);
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

internal VKAPI_ATTR VkBool32 VKAPI_CALL vk_debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_types,
    const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
    void *user_data) {
    switch (message_severity) {
        default:
        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT:
            LOG_ERROR(callback_data->pMessage);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT:
            LOG_WARNING(callback_data->pMessage);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT:
            LOG_INFO(callback_data->pMessage);
            break;

        case VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT:
            LOG_DEBUG(callback_data->pMessage);
            break;
    }
    return VK_FALSE;
}

internal void vk_create_debug_messenger(Vulkan_Context *context);

internal void vk_create_surface(Vulkan_Context *context, GLFWwindow *window);
