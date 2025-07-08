#include <stdio.h>
#include <stdint.h>

typedef uint32_t u32;

#define ASSERT(expr)        \
    do {                    \
        if (!(expr)) {      \
            __debugbreak(); \
        }                   \
    } while (0)

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define WINDOW_WIDTH  640
#define WINDOW_HEIGHT 480
#define WINDOW_TITLE  "Vulkan 2D"

int main() {
    printf("App started\n");

    ASSERT(glfwInit());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow *window;
    window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, WINDOW_TITLE, NULL, NULL);
    ASSERT(window != NULL);

    u32 extension_count = 0;
    vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL);
    ASSERT(extension_count > 0);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();

    printf("App stopped\n");
    return 0;
}
