#pragma once

#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

class VulkanApp {
 public:
  void run();

 private:
  GLFWwindow* window_;
  const uint32_t WIDTH = 800;
  const uint32_t HEIGHT = 600;
  VkInstance instance_;

  // Handle debug messages from validation layers
  VkDebugUtilsMessengerEXT debug_messenger_;
  
  // standard set of validation layers
  const std::vector<const char*> validation_layers_ = {
      "VK_LAYER_KHRONOS_validation"};

#ifdef NDEBUG
  const bool enable_valid_layers_ = false;
#else
  const bool enable_valid_layers_ = true;
#endif

  void initWindow();
  void initVulkan();
  void mainLoop();
  void cleanUp();
  void createInstance();
  bool checkValidationLayerSupport();
  std::vector<const char*> getRequiredExtensions();
  void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo);
  void setupDebugMessenger();

  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                VkDebugUtilsMessageTypeFlagsEXT message_type,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData);

  /* Extension functions are not loaded by default, we need to look up its address
  * ourselves with this wrapper
  */
  static VkResult CreateDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
      const VkAllocationCallbacks* pAllocator,
      VkDebugUtilsMessengerEXT* pDebugMessenger);

  /* Also load the clean up code for the debug utils extension
  */
  static void DestroyDebugUtilsMessengerEXT(
      VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
      const VkAllocationCallbacks* pAllocator);
};
