#include "VulkanApp.h"

#include <iostream>

VkResult VulkanApp::CreateDebugUtilsMessengerEXT(
  VkInstance instance,
  const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
  const VkAllocationCallbacks* pAllocator,
  VkDebugUtilsMessengerEXT* pDebugMessenger) {

  auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance,pCreateInfo,pAllocator,pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}

void VulkanApp::DestroyDebugUtilsMessengerEXT(
  VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger,
  const VkAllocationCallbacks* pAllocator) {
  
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}
void VulkanApp::run() {
  initWindow();
  initVulkan();
  mainLoop();
  cleanUp();
}

void VulkanApp::initWindow() {
  glfwInit();
  glfwWindowHint(GLFW_CLIENT_API,
                 GLFW_NO_API);  // don't create an opengl context
  glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);  // can't be resizable

  window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
}
void VulkanApp::initVulkan() {
  createInstance();
  setupDebugMessenger();
}
void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
  }
}
void VulkanApp::cleanUp() {
  // Clean up other vulkan resources here first before instance
  if (enable_valid_layers_) {
    DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
  }

  // Clean up vulkan instance
  vkDestroyInstance(instance_, nullptr);

  // Clean up gflw window
  glfwDestroyWindow(window_);
  glfwTerminate();
}

/* Instance is how vulkan interacts with this application*/
void VulkanApp::createInstance() {
  if (enable_valid_layers_ && !checkValidationLayerSupport())
    throw std::runtime_error("Validation layers requested, not available.");

  // Optional app info
  VkApplicationInfo appInfo{};

  appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
  appInfo.pApplicationName = "Hello Triangle";
  appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.pEngineName = "No Engine Name";
  appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
  appInfo.apiVersion = VK_API_VERSION_1_0;

  // Not optional
  VkInstanceCreateInfo createInfo{};
  createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
  createInfo.pApplicationInfo = &appInfo;

  // Create debug messenger so debugging can be done in vkCreateInstance and 
  // vkDestroyInstance
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo;

  auto required_extensions = getRequiredExtensions();

  createInfo.enabledExtensionCount =
      static_cast<uint32_t>(required_extensions.size());
  createInfo.ppEnabledExtensionNames = required_extensions.data();

  if (enable_valid_layers_) {
    createInfo.enabledLayerCount =
        static_cast<uint32_t>(validation_layers_.size());
    createInfo.ppEnabledLayerNames = validation_layers_.data();

    populateDebugMessengerCreateInfo(debugCreateInfo);
    createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
  } else {
    createInfo.enabledLayerCount = 0;
    createInfo.pNext = nullptr;
  }

  if (vkCreateInstance(&createInfo, nullptr, &instance_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create vulkan instance");
  }

  // Check driver for supported extensions
  uint32_t extension_count = 0;
  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count, nullptr);

  // Get the extension details
  std::vector<VkExtensionProperties> extensions(extension_count);

  vkEnumerateInstanceExtensionProperties(nullptr, &extension_count,
                                         extensions.data());

  std::cout << "Available extensions" << std::endl;

  for (auto& ext : extensions) {
    std::cout << "\t" << ext.extensionName << '\n';
  }
}

bool VulkanApp::checkValidationLayerSupport() {
  uint32_t layer_count;
  vkEnumerateInstanceLayerProperties(&layer_count, nullptr);

  std::vector<VkLayerProperties> available_layers(layer_count);
  vkEnumerateInstanceLayerProperties(&layer_count, available_layers.data());

  for (const auto& req_layer : validation_layers_) {
    bool found = false;
    for (const auto& avail_layer : available_layers) {
      if (std::strcmp(avail_layer.layerName, req_layer) == 0) {
        found = true;
        break;
      }
    }
    if (!found) return false;
  }
  return true;
}

std::vector<const char*> 
VulkanApp::getRequiredExtensions() {
  // Vulkan is implementation agnostic, it needs to specify extensions to
  // interface with the window manager
  uint32_t glfw_extension_count = 0;
  const char** glfw_extensions;
  glfw_extensions = glfwGetRequiredInstanceExtensions(&glfw_extension_count);

  std::vector<const char*> extensions(glfw_extensions,
                                      glfw_extensions + glfw_extension_count);

  // Optional debugging extension for validation layers
  if (enable_valid_layers_) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }

  return extensions;
}

VKAPI_ATTR VkBool32 VKAPI_CALL
VulkanApp::debugCallback(
  VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
  VkDebugUtilsMessageTypeFlagsEXT message_type,
  const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
  void* pUserData) {

  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

  return VK_FALSE;
}

void VulkanApp::populateDebugMessengerCreateInfo(
VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};

  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
      //VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

  createInfo.pfnUserCallback = VulkanApp::debugCallback;
  createInfo.pUserData = nullptr; // Optional
}

void VulkanApp::setupDebugMessenger() {
  if (!enable_valid_layers_) return;

  VkDebugUtilsMessengerCreateInfoEXT createInfo;
  populateDebugMessengerCreateInfo(createInfo);

  if (CreateDebugUtilsMessengerEXT(instance_, &createInfo, nullptr,
                                   &debug_messenger_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to set up debug messenger.");
  }
}

