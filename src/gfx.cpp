// -----------------------------------------------------------------------------

internal Vulkan_Context *vk_init(GLFWwindow *window) {
    if (!vk_check_validation_layer_support()) {
        LOG_FATAL("Validation layers requested, but not available");
    }

    Vulkan_Context *context = new Vulkan_Context{};
    vk_create_instance(context);
    vk_create_debug_messenger(context);
    vk_create_surface(context, window);
    vk_pick_physical_device(context);
    return context;
}

internal void vk_cleanup(Vulkan_Context *context) {
    delete context;
}

// -----------------------------------------------------------------------------

internal b8 vk_check_validation_layer_support() {
    u32 available_layer_count = 0;
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));

    VkLayerProperties *available_layers = new VkLayerProperties[available_layer_count]{};
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

    u32 requested_layer_count = ARRAY_COUNT(vk_validation_layer_names);
    for (u32 i = 0; i < requested_layer_count; ++i) {
        const char *requested = vk_validation_layer_names[i];
        b8 found = false;
        for (u32 j = 0; j < available_layer_count; ++j) {
            const char *available = available_layers[j].layerName;
            if (strcmp(requested, available) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }

    delete[] available_layers;

    return true;
}

internal void vk_create_instance(Vulkan_Context *context) {
    VkApplicationInfo app_info{};
    app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app_info.pApplicationName = APP_NAME;
    app_info.applicationVersion = VK_MAKE_VERSION(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    app_info.pEngineName = APP_NAME;
    app_info.engineVersion = VK_MAKE_VERSION(APP_VERSION_MAJOR, APP_VERSION_MINOR, APP_VERSION_PATCH);
    app_info.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    create_info.pApplicationInfo = &app_info;
    create_info.enabledExtensionCount = ARRAY_COUNT(vk_required_extension_names);
    create_info.ppEnabledExtensionNames = vk_required_extension_names;

    VK_CHECK_RESULT(vkCreateInstance(&create_info, context->allocator, &context->instance));
}

internal void vk_create_debug_messenger(Vulkan_Context *context) {
    VkDebugUtilsMessengerCreateInfoEXT create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT;
    create_info.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT
        | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT;
    create_info.pfnUserCallback = vk_debug_callback;

    PFN_vkCreateDebugUtilsMessengerEXT callback =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(context->instance, "vkCreateDebugUtilsMessengerEXT");
    VK_CHECK_RESULT(callback(context->instance, &create_info, context->allocator, &context->debug_messenger));
}

internal void vk_create_surface(Vulkan_Context *context, GLFWwindow *window) {
    VK_CHECK_RESULT(glfwCreateWindowSurface(context->instance, window, context->allocator, &context->surface));
}

internal void vk_get_queue_family_support(
    VkPhysicalDevice device, VkSurfaceKHR surface, Vulkan_Queue_Family_Indices *supported) {
    supported->graphics_family = -1;
    supported->present_family = -1;
    supported->compute_family = -1;
    supported->transfer_family = -1;

    u32 queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);

    auto queue_families = new VkQueueFamilyProperties[queue_family_count]{};
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, queue_families);

    u8 min_transfer_score = 255;
    for (u32 i = 0; i < queue_family_count; ++i) {
        u8 current_transfer_score = 0;
        VkQueueFlags flags = queue_families[i].queueFlags;

        if (flags & VK_QUEUE_GRAPHICS_BIT) {
            supported->graphics_family = i;
            ++current_transfer_score;
        }

        VkBool32 present_support = VK_FALSE;
        VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support));
        if (present_support) {
            supported->present_family = i;
        }

        if (flags & VK_QUEUE_COMPUTE_BIT) {
            supported->compute_family = i;
            ++current_transfer_score;
        }

        if (flags & VK_QUEUE_TRANSFER_BIT) {
            if (current_transfer_score <= min_transfer_score) {
                min_transfer_score = current_transfer_score;
                supported->transfer_family = i;
            }
        }
    }
}

internal b8 vk_check_device_extension_support(VkPhysicalDevice device) {
    u32 available_extension_count;
    VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, NULL));
    
    u32 device_extension_count = ARRAY_COUNT(vk_device_extension_names);
    if (device_extension_count > 0) {
        auto available_extensions = new VkExtensionProperties[available_extension_count];
        VK_CHECK_RESULT(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, available_extensions));

        for (u32 i = 0; i < device_extension_count; ++i) {
            b8 found = false;
            const char *requested = vk_device_extension_names[i];
            for (u32 j = 0; j < available_extension_count; ++j) {
                const char *available = available_extensions[j].extensionName;
                if (strcmp(requested, available) == 0) {
                    found = true;
                    break;
                }
            }
            if (!found) {
                LOG_WARNING("Required extension not found: %s", vk_device_extension_names[i]);
                return false;
            }
        }

        delete[] available_extensions;
    }

    return true;
}

internal void vk_get_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, Vulkan_Swapchain_Support_Info *info) {
    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &info->capabilities));

    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &info->format_count, NULL));
    if (info->format_count > 0) {
        if (!info->formats) {
            info->formats = new VkSurfaceFormatKHR[info->format_count];
        }
        VK_CHECK_RESULT(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &info->format_count, info->formats));
    }

    VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &info->present_mode_count, NULL));
    if (info->present_mode_count > 0) {
        if (!info->present_modes) {
            info->present_modes = new VkPresentModeKHR[info->present_mode_count];
        }
        VK_CHECK_RESULT(vkGetPhysicalDeviceSurfacePresentModesKHR(
                    device, surface, &info->present_mode_count, info->present_modes));
    }
}

internal void vk_cleanup_swapchain_support(Vulkan_Swapchain_Support_Info *info) {
    if (info->formats) {
        delete[] info->formats;
        info->formats = NULL;
        info->format_count = 0;
    }

    if (info->present_modes) {
        delete[] info->present_modes;
        info->present_modes = NULL;
        info->present_mode_count = 0;
    }
}

internal u32 vk_rate_device_suitability(VkPhysicalDevice device, VkSurfaceKHR surface) {
    u32 score = 0;
    
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) score += 1000;
    score += properties.limits.maxImageDimension2D; // Maximum possible size of textures affects graphics quality

    VkPhysicalDeviceFeatures features;
    vkGetPhysicalDeviceFeatures(device, &features);
    if (!features.geometryShader) return 0; // Application can't function without geometry shaders

    { // Check queue family support
        Vulkan_Queue_Family_Indices queue_family_support;
        vk_get_queue_family_support(device, surface, &queue_family_support);

        b8 queue_family_support_adequate =
            queue_family_support.graphics_family != -1 &&
            queue_family_support.present_family != -1 &&
            queue_family_support.compute_family != -1 &&
            queue_family_support.transfer_family != -1;

        if (!queue_family_support_adequate) return 0;
    }

    if (!vk_check_device_extension_support(device)) return 0;

    { // Check swapchain support
        Vulkan_Swapchain_Support_Info swapchain_support{};
        vk_get_swapchain_support(device, surface, &swapchain_support);

        b8 swapchain_support_adequate =
            swapchain_support.format_count > 0 &&
            swapchain_support.present_mode_count > 0;

        vk_cleanup_swapchain_support(&swapchain_support);

        if (!swapchain_support_adequate) return 0;
    }

    return score;
}

internal void vk_pick_physical_device(Vulkan_Context *context) {
    u32 physical_device_count;
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL));
    ASSERT(physical_device_count > 0);

    auto physical_devices = new VkPhysicalDevice[physical_device_count]{};
    VK_CHECK_RESULT(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices));

    u32 best_picked_index = -1;
    u32 best_picked_score = 0;
    for (u32 i = 0; i < physical_device_count; ++i) {
        u32 score = vk_rate_device_suitability(physical_devices[i], context->surface);
        if (score > best_picked_score) {
            best_picked_index = i;
            best_picked_score = score;
        }
    }
    ASSERT(best_picked_score > 0);

    context->physical_device = physical_devices[best_picked_index];

    delete[] physical_devices;

    vk_get_queue_family_support(context->physical_device, context->surface, &context->queue_family_support);
    vk_get_swapchain_support(context->physical_device, context->surface, &context->swapchain_support);
}
