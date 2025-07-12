struct App {
    GLFWwindow *window;
    Vk_Context *vulkan;
};

internal App *app_init();
internal void app_cleanup(App *app);
internal void app_iterate(App *app);

internal void app_run();
