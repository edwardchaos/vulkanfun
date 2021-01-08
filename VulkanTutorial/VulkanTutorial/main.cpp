// glfw will include vulkan when it sees this define
#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h> 

#include <iostream>
#include <stdexcept>
#include <cstdlib>
#include <vector>

class HelloTriangleApplication{
public:
  void run() {
    initWindow();
    initVulkan();
    mainLoop();
    cleanUp();
  }
private:
  GLFWwindow* window_;
  const uint32_t WIDTH = 800;
  const uint32_t HEIGHT = 600;
  VkInstance instance_;

  void initWindow() {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API); // don't create an opengl context
    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE); // can't be resizable

    window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  }
  void initVulkan(){
    createInstance();

  }
  void mainLoop() {
    while (!glfwWindowShouldClose(window_)) {
      glfwPollEvents();
    }
  }
  void cleanUp() {
    // Clean up other vulkan resources here first before instance

    // Clean up vulkan instance
    vkDestroyInstance(instance_, nullptr);

    // Clean up gflw window
    glfwDestroyWindow(window_);
    glfwTerminate();
  }

  void createInstance() {
    // Optional app info
    VkApplicationInfo appInfo{};

    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.pEngineName = "No Engine Name";
    appInfo.engineVersion = VK_MAKE_VERSION(1,0,0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    // Not optional
    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    // Vulkan is implementation agnostic, it needs to specify extensions to 
    // interface with the window manager
    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions;

    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    createInfo.enabledExtensionCount = glfwExtensionCount;
    createInfo.ppEnabledExtensionNames = glfwExtensions;
    createInfo.enabledLayerCount = 0;

    if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
      throw std::runtime_error("Failed to create vulkan instance");
    }


    // Check driver for supported extensions
    uint32_t extensionCount = 0;
    vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount, nullptr);

    // Get the extension details
    std::vector<VkExtensionProperties> extensions(extensionCount);

    vkEnumerateInstanceExtensionProperties(
      nullptr, &extensionCount, extensions.data());


    std::cout << "Available extensions" << std::endl;

    for (auto& ext : extensions) {
      std::cout << "\t" << ext.extensionName << '\n';
    }
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