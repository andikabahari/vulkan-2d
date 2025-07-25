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
    vk_create_command_buffer(context);

    vk_create_swapchain(context, window);
    vk_create_render_pass(context);
    vk_create_graphics_pipeline(context);

    vk_create_framebuffers(context);
    vk_create_sync_objects(context);

    {
        context->vertex_count = ARRAY_COUNT(vk_quad_vertices);

        VkDeviceSize vert_size = sizeof(vk_quad_vertices);

        vk_create_buffer(
            context, vert_size,
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, &context->vertex_buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &context->vertex_buffer_memory);

        {
            void *data;
            VK_CHECK(vkMapMemory(context->device, context->vertex_buffer_memory, 0, vert_size, 0, &data));
            memcpy(data, vk_quad_vertices, (u64)vert_size);
            vkUnmapMemory(context->device, context->vertex_buffer_memory);
        }

        context->index_count = ARRAY_COUNT(vk_quad_indices);

        VkDeviceSize copy_size = sizeof(vk_quad_indices);

        VkBuffer staging_buffer;
        VkDeviceMemory staging_buffer_memory;
        vk_create_buffer(
            context, copy_size,
            VK_BUFFER_USAGE_TRANSFER_SRC_BIT, &staging_buffer,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, &staging_buffer_memory);

        {
            void *data;
            vkMapMemory(context->device, staging_buffer_memory, 0, copy_size, 0, &data);
            memcpy(data, vk_quad_indices, (u64)copy_size);
            vkUnmapMemory(context->device, staging_buffer_memory);
        }

        vk_create_buffer(
            context, copy_size,
            VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT, &context->index_buffer,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, &context->index_buffer_memory);

        vk_copy_buffer(context, staging_buffer, context->index_buffer, copy_size);

        vkDestroyBuffer(context->device, staging_buffer, context->allocator);
        vkFreeMemory(context->device, staging_buffer_memory, context->allocator);
    }

    return context;
}

internal void vk_cleanup(Vk_Context *context) {
    {
        vkDestroyBuffer(context->device, context->vertex_buffer, context->allocator);
        vkFreeMemory(context->device, context->vertex_buffer_memory, context->allocator);

        vkDestroyBuffer(context->device, context->index_buffer, context->allocator);
        vkFreeMemory(context->device, context->index_buffer_memory, context->allocator);
    }

    vkDestroySemaphore(context->device, context->image_available_semaphore, context->allocator);
    vkDestroySemaphore(context->device, context->render_finished_semaphore, context->allocator);
    vkDestroyFence(context->device, context->in_flight_fence, context->allocator);

    vkDestroyCommandPool(context->device, context->command_pool, context->allocator);

    vk_cleanup_framebuffers(context);

    vkDestroyPipeline(context->device, context->graphics_pipeline, context->allocator);
    vkDestroyPipelineLayout(context->device, context->pipeline_layout, context->allocator);

    vkDestroyRenderPass(context->device, context->render_pass, context->allocator);

    vk_cleanup_swapchain(context);

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

internal void vk_draw_frame(Vk_Context *context) {
    vkWaitForFences(context->device, 1, &context->in_flight_fence, VK_TRUE, UINT64_MAX);
    vkResetFences(context->device, 1, &context->in_flight_fence);

    u32 image_index;
    VK_CHECK(vkAcquireNextImageKHR(
        context->device, context->swapchain, UINT64_MAX, context->image_available_semaphore, VK_NULL_HANDLE, &image_index));

    VK_CHECK(vkResetCommandBuffer(context->command_buffer, 0));
    vk_record_command_buffer(context, image_index);

    VkSemaphore wait_semaphores[] = {context->image_available_semaphore};
    VkPipelineStageFlags wait_stages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSemaphore signal_semaphores[] = {context->render_finished_semaphore};

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.waitSemaphoreCount = 1;
    submit_info.pWaitSemaphores = wait_semaphores;
    submit_info.pWaitDstStageMask = wait_stages;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &context->command_buffer;
    submit_info.signalSemaphoreCount = 1;
    submit_info.pSignalSemaphores = signal_semaphores;
    VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &submit_info, context->in_flight_fence));

    VkSwapchainKHR swap_chains[] = {context->swapchain};

    VkPresentInfoKHR present_info{};
    present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    present_info.waitSemaphoreCount = 1;
    present_info.pWaitSemaphores = signal_semaphores;
    present_info.swapchainCount = 1;
    present_info.pSwapchains = swap_chains;
    present_info.pImageIndices = &image_index;
    present_info.pResults = NULL; // optional
    VK_CHECK(vkQueuePresentKHR(context->present_queue, &present_info));
}

internal void vk_wait_idle(Vk_Context *context) {
    vkDeviceWaitIdle(context->device);
}

// -----------------------------------------------------------------------------

internal b8 vk_check_validation_layer_support() {
    u32 available_layer_count = 0;
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));

    auto available_layers = new VkLayerProperties[available_layer_count]{};
    VK_CHECK(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

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

    VK_CHECK(vkCreateInstance(&create_info, context->allocator, &context->instance));
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
    VK_CHECK(callback(context->instance, &create_info, context->allocator, &context->debug_messenger));
}

internal void vk_create_surface(Vk_Context *context, GLFWwindow *window) {
    VK_CHECK(glfwCreateWindowSurface(context->instance, window, context->allocator, &context->surface));
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

    u8 min_transfer_scontext = 255;
    for (u32 i = 0; i < queue_family_count; ++i) {
        u8 current_transfer_scontext = 0;
        VkQueueFlags flags = queue_families[i].queueFlags;

        if (flags & VK_QUEUE_GRAPHICS_BIT) {
            supported->graphics_family = i;
            ++current_transfer_scontext;
        }

        VkBool32 present_support = VK_FALSE;
        VK_CHECK(vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &present_support));
        if (present_support) {
            supported->present_family = i;
        }

        if (flags & VK_QUEUE_COMPUTE_BIT) {
            supported->compute_family = i;
            ++current_transfer_scontext;
        }

        if (flags & VK_QUEUE_TRANSFER_BIT) {
            if (current_transfer_scontext <= min_transfer_scontext) {
                min_transfer_scontext = current_transfer_scontext;
                supported->transfer_family = i;
            }
        }
    }

    delete[] queue_families;
}

internal b8 vk_check_device_extension_support(VkPhysicalDevice device) {
    u32 available_extension_count;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, NULL));
    
    u32 device_extension_count = ARRAY_COUNT(vk_device_extension_names);
    if (device_extension_count > 0) {
        auto available_extensions = new VkExtensionProperties[available_extension_count];
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &available_extension_count, available_extensions));

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
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &info->capabilities));

    VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &info->format_count, NULL));
    if (info->format_count > 0) {
        if (!info->formats) {
            info->formats = new VkSurfaceFormatKHR[info->format_count];
        }
        VK_CHECK(vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &info->format_count, info->formats));
    }

    VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &info->present_mode_count, NULL));
    if (info->present_mode_count > 0) {
        if (!info->present_modes) {
            info->present_modes = new VkPresentModeKHR[info->present_mode_count];
        }
        VK_CHECK(vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &info->present_mode_count, info->present_modes));
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
    u32 scontext = 0;
    
    VkPhysicalDeviceProperties properties;
    vkGetPhysicalDeviceProperties(device, &properties);
    if (properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) scontext += 1000;
    scontext += properties.limits.maxImageDimension2D; // Maximum possible size of textures affects graphics quality

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

    return scontext;
}

internal void vk_pick_physical_device(Vk_Context *context) {
    u32 physical_device_count;
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, NULL));
    ASSERT(physical_device_count > 0);

    auto physical_devices = new VkPhysicalDevice[physical_device_count]{};
    VK_CHECK(vkEnumeratePhysicalDevices(context->instance, &physical_device_count, physical_devices));

    u32 best_picked_index = -1;
    u32 best_picked_scontext = 0;
    for (u32 i = 0; i < physical_device_count; ++i) {
        u32 scontext = vk_rate_device_suitability(physical_devices[i], context->surface);
        if (scontext > best_picked_scontext) {
            best_picked_index = i;
            best_picked_scontext = scontext;
        }
    }
    ASSERT(best_picked_scontext > 0);

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

    VK_CHECK(vkCreateDevice(
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
    vk_get_swapchain_support(context->physical_device, context->surface, &context->swapchain_support);

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

    VK_CHECK(vkCreateSwapchainKHR(
        context->device, &create_info, context->allocator, &context->swapchain));

    VK_CHECK(vkGetSwapchainImagesKHR(
        context->device, context->swapchain, &context->swapchain_image_count, NULL));
    if (context->swapchain_images == NULL) {
        context->swapchain_images = new VkImage[context->swapchain_image_count]{};
    }
    VK_CHECK(vkGetSwapchainImagesKHR(
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

            VK_CHECK(vkCreateImageView(
                context->device, &create_info, context->allocator, &context->swapchain_image_views[i]));
        }
    }
}

internal void vk_cleanup_swapchain(Vk_Context *context) {
    for (u32 i = 0; i < context->swapchain_image_count; ++i) {
        vkDestroyImageView(context->device, context->swapchain_image_views[i], context->allocator);
    }
    delete[] context->swapchain_image_views;
    context->swapchain_image_views = NULL;

    if (context->swapchain != VK_NULL_HANDLE) {
        vkDestroySwapchainKHR(context->device, context->swapchain, context->allocator);
        context->swapchain = VK_NULL_HANDLE;
    }
}

internal void vk_create_render_pass(Vk_Context *context) {
    VkAttachmentDescription color_attachment{};
    color_attachment.format = context->swapchain_image_format;
    color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;
    color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    color_attachment.flags = 0;

    VkAttachmentReference color_attachment_ref{};
    color_attachment_ref.attachment = 0;
    color_attachment_ref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass_desc{};
    subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass_desc.colorAttachmentCount = 1;
    subpass_desc.pColorAttachments = &color_attachment_ref;

    VkSubpassDependency subpass_dependency{};
    subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    subpass_dependency.dstSubpass = 0;
    subpass_dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.srcAccessMask = 0;
    subpass_dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    subpass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo render_pass_create_info{};
    render_pass_create_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    render_pass_create_info.attachmentCount = 1;
    render_pass_create_info.pAttachments = &color_attachment;
    render_pass_create_info.subpassCount = 1;
    render_pass_create_info.pSubpasses = &subpass_desc;
    render_pass_create_info.dependencyCount = 1;
    render_pass_create_info.pDependencies = &subpass_dependency;

    VK_CHECK(vkCreateRenderPass(
        context->device, &render_pass_create_info, context->allocator, &context->render_pass));
}

internal char *vk_read_code(const char *filename, u64 *size) {
    FILE *file = NULL;
    fopen_s(&file, filename, "rb");
    ASSERT(file != NULL);

    fseek(file, 0, SEEK_END);
    *size = ftell(file);
    fseek(file, 0, SEEK_SET);

    auto buffer = new char[*size];
    fread(buffer, 1, *size, file);
    fclose(file);

    return buffer;
}

internal void vk_create_graphics_pipeline(Vk_Context *context) {
    u64 vert_shader_size;
    char *vert_shader_code = vk_read_code("res/shaders/quad.vert.spv", &vert_shader_size);
    VkShaderModuleCreateInfo vert_shader_module_info{};
    vert_shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    vert_shader_module_info.codeSize = vert_shader_size;
    vert_shader_module_info.pCode = (u32 *)vert_shader_code;
    VkShaderModule vert_shader_module;
    VK_CHECK(vkCreateShaderModule(
        context->device, &vert_shader_module_info, context->allocator, &vert_shader_module));

    u64 frag_shader_size;
    char *frag_shader_code = vk_read_code("res/shaders/quad.frag.spv", &frag_shader_size);
    VkShaderModuleCreateInfo frag_shader_module_info{};
    frag_shader_module_info.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    frag_shader_module_info.codeSize = frag_shader_size;
    frag_shader_module_info.pCode = (u32 *)frag_shader_code;
    VkShaderModule frag_shader_module;
    VK_CHECK(vkCreateShaderModule(
        context->device, &frag_shader_module_info, context->allocator, &frag_shader_module));

    VkPipelineShaderStageCreateInfo vert_shader_stage_info{};
    vert_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    vert_shader_stage_info.stage = VK_SHADER_STAGE_VERTEX_BIT;
    vert_shader_stage_info.module = vert_shader_module;
    vert_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo frag_shader_stage_info{};
    frag_shader_stage_info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    frag_shader_stage_info.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    frag_shader_stage_info.module = frag_shader_module;
    frag_shader_stage_info.pName = "main";

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info,
    };

    VkVertexInputBindingDescription binding_desc{};
    binding_desc.binding = 0;
    binding_desc.stride = sizeof(Vk_Vertex);
    binding_desc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    VkVertexInputAttributeDescription a_position_desc{};
    a_position_desc.binding = 0;
    a_position_desc.location = 0;
    a_position_desc.format = VK_FORMAT_R32G32_SFLOAT;
    a_position_desc.offset = offsetof(Vk_Vertex, position);

    VkVertexInputAttributeDescription a_tex_coord_desc{};
    a_tex_coord_desc.binding = 0;
    a_tex_coord_desc.location = 1;
    a_tex_coord_desc.format = VK_FORMAT_R32G32_SFLOAT;
    a_tex_coord_desc.offset = offsetof(Vk_Vertex, tex_coord);

    VkVertexInputAttributeDescription attribute_desc[] = {
        a_position_desc,
        a_tex_coord_desc,
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info{};
    vertex_input_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertex_input_info.vertexBindingDescriptionCount = 1;
    vertex_input_info.pVertexBindingDescriptions = &binding_desc;
    vertex_input_info.vertexAttributeDescriptionCount = ARRAY_COUNT(attribute_desc);
    vertex_input_info.pVertexAttributeDescriptions = attribute_desc;

    VkPipelineInputAssemblyStateCreateInfo input_assembly_info{};
    input_assembly_info.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    input_assembly_info.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    input_assembly_info.primitiveRestartEnable = VK_FALSE;

    VkPipelineViewportStateCreateInfo viewport_info{};
    viewport_info.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewport_info.viewportCount = 1;
    viewport_info.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer_info{};
    rasterizer_info.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer_info.depthClampEnable = VK_FALSE;
    rasterizer_info.rasterizerDiscardEnable = VK_FALSE;
    rasterizer_info.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer_info.lineWidth = 1.0f;
    rasterizer_info.cullMode = VK_CULL_MODE_BACK_BIT;
    rasterizer_info.frontFace = VK_FRONT_FACE_CLOCKWISE;
    rasterizer_info.depthBiasEnable = VK_FALSE;

    VkPipelineMultisampleStateCreateInfo multisampling_info{};
    multisampling_info.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling_info.sampleShadingEnable = VK_FALSE;
    multisampling_info.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState color_blend_attachment{};
    color_blend_attachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    color_blend_attachment.blendEnable = VK_FALSE;

    VkPipelineColorBlendStateCreateInfo color_blend_info{};
    color_blend_info.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    color_blend_info.logicOpEnable = VK_FALSE;
    color_blend_info.logicOp = VK_LOGIC_OP_COPY;
    color_blend_info.attachmentCount = 1;
    color_blend_info.pAttachments = &color_blend_attachment;
    color_blend_info.blendConstants[0] = 0.0f;
    color_blend_info.blendConstants[1] = 0.0f;
    color_blend_info.blendConstants[2] = 0.0f;
    color_blend_info.blendConstants[3] = 0.0f;
    
    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR,
    };
    VkPipelineDynamicStateCreateInfo dynamic_state_info{};
    dynamic_state_info.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamic_state_info.dynamicStateCount = ARRAY_COUNT(dynamic_states);
    dynamic_state_info.pDynamicStates = dynamic_states;

    VkPipelineLayoutCreateInfo pipeline_layout_info{};
    pipeline_layout_info.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipeline_layout_info.setLayoutCount = 0;
    pipeline_layout_info.pushConstantRangeCount = 0;

    VK_CHECK(vkCreatePipelineLayout(
        context->device, &pipeline_layout_info, context->allocator, &context->pipeline_layout));

    VkGraphicsPipelineCreateInfo pipeline_info{};
    pipeline_info.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipeline_info.stageCount = ARRAY_COUNT(shader_stages);
    pipeline_info.pStages = shader_stages;
    pipeline_info.pVertexInputState = &vertex_input_info;
    pipeline_info.pInputAssemblyState = &input_assembly_info;
    pipeline_info.pViewportState = &viewport_info;
    pipeline_info.pRasterizationState = &rasterizer_info;
    pipeline_info.pMultisampleState = &multisampling_info;
    pipeline_info.pDepthStencilState = NULL; // optional
    pipeline_info.pColorBlendState = &color_blend_info;
    pipeline_info.pDynamicState = &dynamic_state_info;
    pipeline_info.layout = context->pipeline_layout;
    pipeline_info.renderPass = context->render_pass;
    pipeline_info.subpass = 0;
    pipeline_info.basePipelineHandle = VK_NULL_HANDLE; // optional

    VK_CHECK(vkCreateGraphicsPipelines(
        context->device, VK_NULL_HANDLE, 1, &pipeline_info, context->allocator, &context->graphics_pipeline));

    vkDestroyShaderModule(context->device, frag_shader_module, context->allocator);
    delete[] frag_shader_code;

    vkDestroyShaderModule(context->device, vert_shader_module, context->allocator);
    delete[] vert_shader_code;
}

internal void vk_create_framebuffers(Vk_Context *context) {
    context->framebuffers = new VkFramebuffer[context->swapchain_image_count];

    for (u32 i = 0; i < context->swapchain_image_count; ++i) {
        // TODO: we might need depth attachment later
        VkImageView attachments[] = {context->swapchain_image_views[i]};
        
        VkFramebufferCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        create_info.renderPass = context->render_pass;
        create_info.attachmentCount = ARRAY_COUNT(attachments);
        create_info.pAttachments = attachments;
        create_info.width = context->swapchain_extent.width;
        create_info.height = context->swapchain_extent.height;
        create_info.layers = 1;

        VK_CHECK(vkCreateFramebuffer(
            context->device, &create_info, context->allocator, &context->framebuffers[i]));
    }
}

internal void vk_cleanup_framebuffers(Vk_Context *context) {
    for (u32 i = 0; i < context->swapchain_image_count; ++i) {
        vkDestroyFramebuffer(context->device, context->framebuffers[i], context->allocator);
    }
    delete[] context->framebuffers;
    context->framebuffers = NULL;
}

internal void vk_create_command_buffer(Vk_Context *context) {
    { // Command pool
        VkCommandPoolCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        create_info.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        create_info.queueFamilyIndex = context->queue_family_support.graphics_family;

        VK_CHECK(vkCreateCommandPool(context->device, &create_info, context->allocator, &context->command_pool));
    }

    { // Command buffer
        VkCommandBufferAllocateInfo alloc_info{};
        alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        alloc_info.commandPool = context->command_pool;
        alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        alloc_info.commandBufferCount = 1;

        VK_CHECK(vkAllocateCommandBuffers(context->device, &alloc_info, &context->command_buffer));
    }
}

internal void vk_create_sync_objects(Vk_Context *context) {
    VkSemaphoreCreateInfo semaphore_create_info{};
    semaphore_create_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VK_CHECK(vkCreateSemaphore(
        context->device, &semaphore_create_info, context->allocator, &context->image_available_semaphore));

    VK_CHECK(vkCreateSemaphore(
        context->device, &semaphore_create_info, context->allocator, &context->render_finished_semaphore));

    VkFenceCreateInfo fence_create_info{};
    fence_create_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_create_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    VK_CHECK(vkCreateFence(
        context->device, &fence_create_info, context->allocator, &context->in_flight_fence));
}

internal void vk_record_command_buffer(Vk_Context *context, u32 image_index) {
    VkCommandBufferBeginInfo cmd_begin_info{};
    cmd_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    cmd_begin_info.flags = 0; // optional
    cmd_begin_info.pInheritanceInfo = NULL; // optional, only relevant for secondary command buffers
    VK_CHECK(vkBeginCommandBuffer(context->command_buffer, &cmd_begin_info));

    { // Render pass
        VkRenderPassBeginInfo render_pass_info{};
        render_pass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        render_pass_info.renderPass = context->render_pass;
        render_pass_info.framebuffer = context->framebuffers[image_index];
        render_pass_info.renderArea.offset.x = 0;
        render_pass_info.renderArea.offset.y = 0;
        render_pass_info.renderArea.extent.width = context->swapchain_extent.width;
        render_pass_info.renderArea.extent.height = context->swapchain_extent.height;
        render_pass_info.pNext = NULL;
        u32 clear_value_count = 1;
        auto clear_values = new VkClearValue[clear_value_count]{};
        clear_values[0].color.float32[0] = 0.0f;
        clear_values[0].color.float32[1] = 0.0f;
        clear_values[0].color.float32[2] = 0.0f;
        clear_values[0].color.float32[3] = 1.0f;
        render_pass_info.clearValueCount = clear_value_count;
        render_pass_info.pClearValues = clear_values;

        vkCmdBeginRenderPass(context->command_buffer, &render_pass_info, VK_SUBPASS_CONTENTS_INLINE);

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (f32)context->swapchain_extent.width;
        viewport.height = (f32)context->swapchain_extent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
        vkCmdSetViewport(context->command_buffer, 0, 1, &viewport);
    
        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent = context->swapchain_extent;
        vkCmdSetScissor(context->command_buffer, 0, 1, &scissor);

        vkCmdBindPipeline(context->command_buffer, VK_PIPELINE_BIND_POINT_GRAPHICS, context->graphics_pipeline);

        {
            u32 first_binding = 0;
            VkBuffer buffers[] = {context->vertex_buffer};
            VkDeviceSize offsets[] = {0};
            vkCmdBindVertexBuffers(context->command_buffer, first_binding, ARRAY_COUNT(buffers), buffers, offsets);
        }

        vkCmdBindIndexBuffer(context->command_buffer, context->index_buffer, 0, VK_INDEX_TYPE_UINT32);

        vkCmdDrawIndexed(context->command_buffer, 6, 1, 0, 0, 0);

        vkCmdEndRenderPass(context->command_buffer);

        delete[] clear_values;
    }

    VK_CHECK(vkEndCommandBuffer(context->command_buffer));
}

u32 vk_find_memory_type(VkPhysicalDeviceMemoryProperties mem_properties, u32 type_filter, VkMemoryPropertyFlags properties) {
    for (u32 i = 0; i < mem_properties.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) &&
            (mem_properties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i; // Found a suitable memory type
        }
    }

    LOG_FATAL("Failed to find memory type!");
}

internal void vk_create_buffer(
    Vk_Context *context, VkDeviceSize size,
    VkBufferUsageFlags usage, VkBuffer *buffer,
    VkMemoryPropertyFlags properties, VkDeviceMemory *buffer_memory) {

    VkBufferCreateInfo buffer_info{};
    buffer_info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    buffer_info.size = size;
    buffer_info.usage = usage; 
    buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(context->device, &buffer_info, context->allocator, buffer));

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(context->device, *buffer, &mem_requirements);

    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(context->physical_device, &mem_properties);

    VkMemoryAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    alloc_info.allocationSize = mem_requirements.size;
    alloc_info.memoryTypeIndex = vk_find_memory_type(mem_properties, mem_requirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(context->device, &alloc_info, context->allocator, buffer_memory));

    VK_CHECK(vkBindBufferMemory(context->device, *buffer, *buffer_memory, 0));
}

internal void vk_copy_buffer(Vk_Context *context, VkBuffer src_buffer, VkBuffer dst_buffer, VkDeviceSize size) {
    VkCommandBufferAllocateInfo alloc_info{};
    alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    alloc_info.commandPool = context->command_pool;
    alloc_info.commandBufferCount = 1;

    VkCommandBuffer command_buffer;
    VK_CHECK(vkAllocateCommandBuffers(context->device, &alloc_info, &command_buffer));
    
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    VK_CHECK(vkBeginCommandBuffer(command_buffer, &begin_info));

    VkBufferCopy copy_region{};
    copy_region.srcOffset = 0;
    copy_region.dstOffset = 0;
    copy_region.size = size;

    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    VK_CHECK(vkEndCommandBuffer(command_buffer));

    VkSubmitInfo submit_info{};
    submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submit_info.commandBufferCount = 1;
    submit_info.pCommandBuffers = &command_buffer;

    VkFenceCreateInfo fence_info{};
    fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fence_info.flags = 0; // Not VK_FENCE_CREATE_SIGNALED_BIT

    VkFence fence;
    VK_CHECK(vkCreateFence(context->device, &fence_info, context->allocator, &fence));

    VK_CHECK(vkQueueSubmit(context->graphics_queue, 1, &submit_info, fence));

    VK_CHECK(vkWaitForFences(context->device, 1, &fence, VK_TRUE, UINT64_MAX));

    vkDestroyFence(context->device, fence, context->allocator);
    vkFreeCommandBuffers(context->device, context->command_pool, 1, &command_buffer);
}
