#include <GLFW/glfw3.h>
#include <vulkan/vulkan.h>

#ifdef _WIN32 
int main();
int WinMain()
{
    main();
}
#endif // _WIN32 



int main()
{
    if (!glfwInit()) {
        return -1;
    }

    // Optional: prevent OpenGL context if using Vulkan later
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    // Create a windowed mode window and its context
    GLFWwindow* window = glfwCreateWindow(800, 600, "Empty GLFW Window", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return -1;
    }


    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); 
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}