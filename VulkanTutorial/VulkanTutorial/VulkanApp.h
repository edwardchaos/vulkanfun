#pragma once

#include <optional>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

struct QueueFamilyIndices{
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  bool isComplete(){
    return graphics_family.has_value() && present_family.has_value();
  }
};

// Contain details about the swap chain to see if it's suitable.
struct SwapChainSupportDetails{
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

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

  // Window system integration extension needed since vulkan is system 
  // agnostic
  VkSurfaceKHR surface_;

  // standard set of validation layers
  const std::vector<const char*> validation_layers_ = {
      "VK_LAYER_KHRONOS_validation"};

  const std::vector<const char*> device_extensions_ = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
  };

  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice logical_device_;
  VkQueue graphics_queue_;
  VkQueue present_queue_;

  VkSwapchainKHR swap_chain_;
  std::vector<VkImage> swapchain_images_;
  VkFormat swapchain_img_format_;
  VkExtent2D swapchain_img_extent_;

  std::vector<VkImageView> swapchain_imgviews_;

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

  // Select a graphics card
  void pickPhysicalDevice();

  /*
  * Check device if it supports the operations we need
  */
  bool isDeviceSuitable(VkPhysicalDevice device);

  /*
  * Finds queue families of the device and returns an index to 
  * a queue family that supports at minimum vk_queue_graphics_bit.
  * 
  * If no queue families of the device support the minimum requirement,
  * the returned indices is not complete.
  * 
  * eg.
  * auto indices = findQueueFamilies(device);
  * if(indices.isComplete()) // A valid queue family exists for this device.
  */
  QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device);

  /*
  * After selecting a physical device, the logical device is created to
  * interface with the physical one.
  * 
  */
  void createLogicalDevice();

  /*
  * Create platform specific surface for displaying things on screen.
  */
  void createSurface();

  /* Check that the device supports things like swap chain for presenting
  */
  bool checkDeviceExtensionSupport(VkPhysicalDevice device);

  SwapChainSupportDetails
  querySwapChainSupport(VkPhysicalDevice device);

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
  const std::vector<VkSurfaceFormatKHR> &available_formats);

  VkPresentModeKHR chooseSwapPresentMode(
  const std::vector<VkPresentModeKHR> &available_present_modes);

  // Resolution of the swap chain images
  VkExtent2D chooseSwapExtent(
  const VkSurfaceCapabilitiesKHR& capabilities);

  void createSwapChain();
  
  // Views into images of the swap chain. These allow us to access parts
  // of the image like format, extent, color space, and drawing.
  void createImageViews();

  void createGraphicsPipeline();
};
