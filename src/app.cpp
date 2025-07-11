internal App *app_init() {
    ASSERT(glfwInit());
    
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    ASSERT(window != NULL);

    Vulkan_Context *vulkan = vk_init();
    ASSERT(vulkan != NULL);

    App *app = new App{};
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
    //
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
