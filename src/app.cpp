internal App *app_init() {
    ASSERT(glfwInit());
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    ASSERT(window != NULL);

    Vk_Context *vulkan = vk_init(window);
    ASSERT(vulkan != NULL);

    glfwSetWindowUserPointer(window, vulkan);

    glfwSetFramebufferSizeCallback(window, app_framebuffer_size_callback);

    auto app = new App{};
    app->window = window;
    app->vulkan = vulkan;
    return app;
}

internal void app_cleanup(App *app) {
    vk_cleanup(app->vulkan);

    glfwDestroyWindow(app->window);
    glfwTerminate();
}

internal void app_iterate(App *app) {
    vk_draw_frame(app->vulkan);
}

internal void app_framebuffer_size_callback(GLFWwindow *window, s32 width, s32 height) {
    auto vulkan = (Vk_Context *)glfwGetWindowUserPointer(window);
    vk_wait_idle(vulkan);
    vk_cleanup_framebuffers(vulkan);
    vk_cleanup_swapchain(vulkan);
    vk_create_swapchain(vulkan, window);
    vk_create_framebuffers(vulkan);
}

internal void app_run() {
    LOG_INFO("App started");

    App *app = app_init();
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
        app_iterate(app);
    }
    app_cleanup(app);

    LOG_INFO("App stopped");
}
