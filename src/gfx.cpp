// -----------------------------------------------------------------------------

internal Vk_Context *vk_init(GLFWwindow *window) {
    if (!vk_check_validation_layer_support()) {
        LOG_FATAL("Validation layers requested, but not available");
    }

    auto context = new Vk_Context{};
    vk_create_instance(context);
    vk_create_debug_messenger(context);
    vk_create_surface(context, window);
    vk_pick_physical_device(context);
    vk_create_device(context);
    vk_create_swapchain(context, window);
    return context;
}

internal void vk_cleanup(Vk_Context *context) {
    for (u32 i = 0; i < context->swapchain_image_count; ++i) {
        vkDestroyImageView(context->device, context->swapchain_image_views[i], context->allocator);
    }
    delete[] context->swapchain_images;
    context->swapchain_image_count = 0;
    vkDestroySwapchainKHR(context->device, context->swapchain, context->allocator);

    vk_cleanup_swapchain_support(&context->swapchain_support);
    vkDestroyDevice(context->device, context->allocator);

    {
        PFN_vkDestroyDebugUtilsMessengerEXT callback =
            (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
                context->instance, "vkDestroyDebugUtilsMessengerEXT");

        callback(context->instance, context->debug_messenger, context->allocator);
    }

    vkDestroySurfaceKHR(context->instance, context->surface, context->allocator);
    vkDestroyInstance(context->instance, context->allocator);

    delete context;
    context = NULL;
}

// -----------------------------------------------------------------------------

internal b8 vk_check_validation_layer_support() {
    u32 available_layer_count = 0;
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));

    auto available_layers = new VkLayerProperties[available_layer_count]{};
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

internal void vk_create_instance(Vk_Context *context) {
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

internal void vk_create_debug_messenger(Vk_Context *context) {
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

internal void vk_create_surface(Vk_Context *context, GLFWwindow *window) {
    VK_CHECK_RESULT(glfwCreateWindowSurface(context->instance, window, context->allocator, &context->surface));
}

internal void vk_get_queue_family_support(
    VkPhysicalDevice device, VkSurfaceKHR surface, Vk_Queue_Family_Indices *supported) {
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

    delete[] queue_families;
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

internal void vk_get_swapchain_support(VkPhysicalDevice device, VkSurfaceKHR surface, Vk_Swapchain_Support_Info *info) {
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

internal void vk_cleanup_swapchain_support(Vk_Swapchain_Support_Info *info) {
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
        Vk_Queue_Family_Indices queue_family_support;
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
        Vk_Swapchain_Support_Info swapchain_support{};
        vk_get_swapchain_support(device, surface, &swapchain_support);

        b8 swapchain_support_adequate =
            swapchain_support.format_count > 0 &&
            swapchain_support.present_mode_count > 0;

        vk_cleanup_swapchain_support(&swapchain_support);

        if (!swapchain_support_adequate) return 0;
    }

    return score;
}

internal void vk_pick_physical_device(Vk_Context *context) {
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

internal void vk_create_device(Vk_Context *context) {
    b8 shared_present_queue =
        context->queue_family_support.graphics_family ==
            context->queue_family_support.present_family;
    b8 shared_transfer_queue =
        context->queue_family_support.graphics_family ==
            context->queue_family_support.transfer_family;
    
    u32 index_count = 1;
    if (!shared_present_queue) index_count++;
    if (!shared_transfer_queue) index_count++;

    auto indices = new u32[index_count];
    u32 index = 0;
    indices[index++] = context->queue_family_support.graphics_family;
    if (!shared_present_queue)
        indices[index++] = context->queue_family_support.present_family;
    if (!shared_transfer_queue)
        indices[index++] = context->queue_family_support.transfer_family;

    auto queue_create_infos = new VkDeviceQueueCreateInfo[index_count]{};
    for (u32 i = 0; i < index_count; ++i) {
        queue_create_infos[i].sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queue_create_infos[i].queueFamilyIndex = indices[i];
        queue_create_infos[i].queueCount = 1;
        f32 queue_priority = 1.0f;
        queue_create_infos[i].pQueuePriorities = &queue_priority;
        queue_create_infos[i].flags = 0;
        queue_create_infos[i].pNext = 0;
    }

    delete[] indices;

    // Not used yet?
    VkPhysicalDeviceFeatures device_features{};

    VkDeviceCreateInfo device_create_info{};
    device_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    device_create_info.queueCreateInfoCount = index_count;
    device_create_info.pQueueCreateInfos = queue_create_infos;
    device_create_info.pEnabledFeatures = &device_features;
    device_create_info.enabledExtensionCount = ARRAY_COUNT(vk_device_extension_names);
    device_create_info.ppEnabledExtensionNames = vk_device_extension_names;

    // Deprecated and ignored
    device_create_info.enabledLayerCount = 0;
    device_create_info.ppEnabledLayerNames = NULL;

    VK_CHECK_RESULT(vkCreateDevice(
                context->physical_device, &device_create_info, context->allocator, &context->device));

    delete[] queue_create_infos;

    vkGetDeviceQueue(
        context->device, context->queue_family_support.graphics_family, 0, &context->graphics_queue);
    vkGetDeviceQueue(
        context->device, context->queue_family_support.present_family, 0, &context->present_queue);
    vkGetDeviceQueue(
        context->device, context->queue_family_support.transfer_family, 0, &context->transfer_queue);
}

internal void vk_create_swapchain(Vk_Context *context, GLFWwindow *window) {
    VkSurfaceFormatKHR surface_format;
    b8 found = false;
    for (u32 i = 0; i < context->swapchain_support.format_count; ++i) {
        VkSurfaceFormatKHR available_format = context->swapchain_support.formats[i];
        if (available_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
            available_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            surface_format = available_format;
            found = true;
            break;
        }
    }
    if (!found) surface_format = context->swapchain_support.formats[0];

    VkPresentModeKHR present_mode = VK_PRESENT_MODE_FIFO_KHR;
    for (u32 i = 0; i < context->swapchain_support.present_mode_count; ++i) {
        VkPresentModeKHR available_present_mode = context->swapchain_support.present_modes[i];
        if (available_present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            present_mode = available_present_mode;
            break;
        }
    }

    s32 width, height;
    glfwGetFramebufferSize(window, &width, &height);
    VkExtent2D extent{(u32)width, (u32)height};
    if (context->swapchain_support.capabilities.currentExtent.width != UINT32_MAX) {
        extent = context->swapchain_support.capabilities.currentExtent;
    } else {
        VkExtent2D min_extent = context->swapchain_support.capabilities.minImageExtent;
        VkExtent2D max_extent = context->swapchain_support.capabilities.maxImageExtent;
        extent.width = CLAMP(extent.width, min_extent.width, max_extent.width);
        extent.height = CLAMP(extent.height, min_extent.height, max_extent.height);
    }

    u32 image_count = context->swapchain_support.capabilities.minImageCount + 1;
    if (context->swapchain_support.capabilities.maxImageCount > 0 &&
        image_count > context->swapchain_support.capabilities.maxImageCount) {
        image_count = context->swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info{};
    create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    create_info.surface = context->surface;
    create_info.minImageCount = image_count;
    create_info.imageFormat = surface_format.format;
    create_info.imageColorSpace = surface_format.colorSpace;
    create_info.imageExtent = extent;
    create_info.imageArrayLayers = 1;
    create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    
    if (context->queue_family_support.graphics_family !=
        context->queue_family_support.present_family) {
        u32 queue_family_indices[] = {
            context->queue_family_support.graphics_family,
            context->queue_family_support.present_family,
        };
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = 0;
    }

    create_info.preTransform = context->swapchain_support.capabilities.currentTransform;
    create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    create_info.presentMode = present_mode;
    create_info.clipped = VK_TRUE;
    create_info.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK_RESULT(vkCreateSwapchainKHR(
        context->device, &create_info, context->allocator, &context->swapchain));

    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(
        context->device, context->swapchain, &context->swapchain_image_count, NULL));
    if (context->swapchain_images == NULL) {
        context->swapchain_images = new VkImage[context->swapchain_image_count]{};
    }
    VK_CHECK_RESULT(vkGetSwapchainImagesKHR(
        context->device, context->swapchain, &context->swapchain_image_count, context->swapchain_images));

    context->swapchain_image_format = surface_format.format;
    context->swapchain_extent = extent;

    { // Image views
        if (context->swapchain_image_views == NULL) {
            context->swapchain_image_views = new VkImageView[context->swapchain_image_count];
        }

        for (u32 i = 0; i < context->swapchain_image_count; ++i) {
            VkImageViewCreateInfo create_info{};
            create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            create_info.image = context->swapchain_images[i];
            create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
            create_info.format = context->swapchain_image_format;
            create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            create_info.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            create_info.subresourceRange.baseMipLevel = 0;
            create_info.subresourceRange.levelCount = 1;
            create_info.subresourceRange.baseArrayLayer = 0;
            create_info.subresourceRange.layerCount = 1;

            VK_CHECK_RESULT(vkCreateImageView(
                context->device, &create_info, context->allocator, &context->swapchain_image_views[i]));
        }
    }
}
