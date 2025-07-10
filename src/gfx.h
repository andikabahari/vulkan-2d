#pragma once

#define VK_CHECK_RESULT(v) ASSERT((v) == VK_SUCCESS)

struct Vulkan_Context {
    //
};

internal Vulkan_Context *vulkan_init();
internal void vulkan_cleanup(Vulkan_Context *context);
