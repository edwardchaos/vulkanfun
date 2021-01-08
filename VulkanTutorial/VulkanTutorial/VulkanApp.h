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

  // Initialize glfw window
  void initWindow();

  // Main init function, creates instance and subobjects
  void initVulkan();

  // Main loop for drawing
  void mainLoop();
  
  void cleanUp();

  // Create vulkan instance, called in initVulkan
  void createInstance();

  // Verify if all required validation layers 'validation_layers_' are supported
  bool checkValidationLayerSupport();

  // Vulkan is implementation agnostic, it needs to specify extensions to
  // interface with the window manager and more.
  std::vector<const char*> getRequiredExtensions();

  // Populates the struct for creating a debug messenger
  void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT& createInfo);

  // Setup debug messenger to call our callback instead of just putting 
  // everything on stdout
  void setupDebugMessenger();

  // Our custom debug messenger callback
  static VKAPI_ATTR VkBool32 VKAPI_CALL
  debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
                VkDebugUtilsMessageTypeFlagsEXT message_type,
                const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
                void* pUserData);

  /* Extension functions are not loaded by default, we need to look up its
  * address ourselves with this wrapper
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
