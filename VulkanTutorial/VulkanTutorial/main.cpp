// glfw will include vulkan when it sees this define
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> 

#include <iostream>
#include <stdexcept>
#include <cstdlib>

class HelloTriangleApplication{
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
  }
private:
  GLFWwindow* window;
  const uint32_t WIDTH = 800;
  const uint32_t HEIGHT = 600;

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create an opengl context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // can't be resizable

    window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }
  void initVulkan(){

  }
  void mainLoop() {
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }
  void cleanUp() {
    glfwDestroyWindow(window);
    glfwTerminate();
  }
};

int main() {
  HelloTriangleApplication app;

  try{
    app.run();
  }
  catch (const std::exception &e){
    std::cerr << e.what() << std::endl;
    return EXIT_FAILURE;
  }

  return EXIT_SUCCESS;
}