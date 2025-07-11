// -----------------------------------------------------------------------------

internal Vulkan_Context *vk_init(GLFWwindow *window) {
    if (!vk_check_validation_layer_support()) {
        LOG_FATAL("Validation layers requested, but not available");
    }

    Vulkan_Context *context = new Vulkan_Context{};
    vk_create_instance(context);
    vk_create_debug_messenger(context);
    vk_create_surface(context, window);
    return context;
}

internal void vk_cleanup(Vulkan_Context *context) {
    delete context;
}

// -----------------------------------------------------------------------------

internal bool vk_check_validation_layer_support() {
    u32 available_layer_count = 0;
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&available_layer_count, NULL));

    VkLayerProperties *available_layers = new VkLayerProperties[available_layer_count]{};
    VK_CHECK_RESULT(vkEnumerateInstanceLayerProperties(&available_layer_count, available_layers));

    u32 requested_layer_count = ARRAY_COUNT(vk_validation_layer_names);
    for (u32 i = 0; i < requested_layer_count; ++i) {
        const char *requested = vk_validation_layer_names[i];
        bool found = false;
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
