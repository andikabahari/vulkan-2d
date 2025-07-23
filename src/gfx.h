#pragma once

// -----------------------------------------------------------------------------

#define VK_CHECK(v) ASSERT((v) == VK_SUCCESS)

struct Vk_Queue_Family_Indices {
    u32 graphics_family;
    u32 present_family;
    u32 compute_family;
    u32 transfer_family;
};

struct Vk_Swapchain_Support_Info {
    VkSurfaceCapabilitiesKHR capabilities;
    u32 format_count;
    VkSurfaceFormatKHR *formats;
    u32 present_mode_count;
    VkPresentModeKHR *present_modes;
};

struct Vk_Vertex {
    f32 position[2];
    f32 tex_coord[2];
};

struct Vk_Context {
    VkInstance instance;
    VkSurfaceKHR surface;
    VkAllocationCallbacks *allocator;
    VkDebugUtilsMessengerEXT debug_messenger;

    VkPhysicalDevice physical_device;
    VkDevice device;
    Vk_Swapchain_Support_Info swapchain_support;
    Vk_Queue_Family_Indices queue_family_support;

    VkQueue graphics_queue;
    VkQueue present_queue;
    VkQueue transfer_queue;

    VkSwapchainKHR swapchain;
    u32 swapchain_image_count;
    VkImage *swapchain_images;
    VkFormat swapchain_image_format;
    VkImageView *swapchain_image_views;
    VkExtent2D swapchain_extent;

    VkFramebuffer *framebuffers;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;

    VkSemaphore image_available_semaphore;
    VkSemaphore render_finished_semaphore;
    VkFence in_flight_fence;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    u32 vertex_count;

    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;
    u32 index_count;
};

internal Vk_Context *vk_init(GLFWwindow *window);
internal void vk_cleanup(Vk_Context *context);

internal void vk_draw_frame(Vk_Context *context);

internal void vk_wait_idle(Vk_Context *context);

// -----------------------------------------------------------------------------

global const char *vk_validation_layer_names[] = {
    "VK_LAYER_KHRONOS_validation",
};

internal b8 vk_check_validation_layer_support();

global const char *vk_required_extension_names[] = {
    VK_KHR_SURFACE_EXTENSION_NAME,
    VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
    "VK_KHR_win32_surface",
};

internal void vk_create_instance(Vk_Context *context);

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

internal void vk_create_debug_messenger(Vk_Context *context);

internal void vk_create_surface(Vk_Context *context, GLFWwindow *window);

internal void vk_get_queue_family_support(
        VkPhysicalDevice device, VkSurfaceKHR surface, Vk_Queue_Family_Indices *supported);

global const char *vk_device_extension_names[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
};

internal b8 vk_check_device_extension_support(VkPhysicalDevice device);

internal void vk_get_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, Vk_Swapchain_Support_Info *info);
internal void vk_cleanup_swapchain_support(Vk_Swapchain_Support_Info *info);

internal u32 vk_rate_device_suitability(VkPhysicalDevice device, VkSurfaceKHR surface);

internal void vk_pick_physical_device(Vk_Context *context);

internal void vk_create_device(Vk_Context *context);

internal void vk_create_swapchain(Vk_Context *context, GLFWwindow *window);
internal void vk_cleanup_swapchain(Vk_Context *context);

internal void vk_create_render_pass(Vk_Context *context);

internal char *vk_read_code(const char *filename, u64 *size);

internal void vk_create_graphics_pipeline(Vk_Context *context);

internal void vk_create_framebuffers(Vk_Context *context);
internal void vk_cleanup_framebuffers(Vk_Context *context);

internal void vk_create_command_buffer(Vk_Context *context);

internal void vk_create_sync_objects(Vk_Context *context);

internal void vk_record_command_buffer(Vk_Context *context, u32 image_index);

u32 vk_find_memory_type(VkPhysicalDeviceMemoryProperties mem_properties, u32 type_filter, VkMemoryPropertyFlags properties);

internal void vk_create_buffer(
    Vk_Context *context, VkDeviceSize size,
    VkBufferUsageFlags usage, VkBuffer *buffer,
    VkMemoryPropertyFlags properties, VkDeviceMemory *buffer_memory);

internal void vk_copy_buffer(Vk_Context *context, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size);

global Vk_Vertex vk_quad_vertices[] = {
    {{-1.0f, -1.0f}, {0.f, 1.f}}, // Bottom-left
    {{ 1.0f, -1.0f}, {1.f, 1.f}}, // Bottom-right
    {{ 1.0f,  1.0f}, {1.f, 0.f}}, // Top-right
    {{-1.0f,  1.0f}, {0.f, 0.f}}, // Top-left
};

global u32 vk_quad_indices[] = {0, 1, 2, 2, 3, 0};
