internal Vulkan_Context *vulkan_init() {
    return new Vulkan_Context;
}

internal void vulkan_cleanup(Vulkan_Context *context) {
    delete context;
}
