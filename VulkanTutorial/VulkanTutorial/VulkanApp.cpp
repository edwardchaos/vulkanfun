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
  pickPhysicalDevice();
  createLogicalDevice();
}

void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
  }
}

void VulkanApp::cleanUp() {
  // Logical device 
  vkDestroyDevice(logical_device_, nullptr);

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

void VulkanApp::pickPhysicalDevice() {
  uint32_t device_count = 0;

  vkEnumeratePhysicalDevices(instance_, &device_count, nullptr);

  if (device_count == 0){
    throw std::runtime_error(
      "Failed to find GPUs with Vulkan support");
  }

  std::vector<VkPhysicalDevice> devices(device_count);

  vkEnumeratePhysicalDevices(
    instance_, &device_count, devices.data());

  // Check each card to see if they support operations we need
  for(const auto &device:devices){
    if(isDeviceSuitable(device)){
      // Just select the first device
      physical_device_ = device;
      break;
    }
  }

  if(physical_device_ == VK_NULL_HANDLE)
    throw std::runtime_error("Failed to find a suitable GPU");
}

bool VulkanApp::isDeviceSuitable(VkPhysicalDevice device){
  VkPhysicalDeviceProperties deviceProperties;
  VkPhysicalDeviceFeatures deviceFeatures;

  // Get name, type, supported vulkan version etc.
  vkGetPhysicalDeviceProperties(device, &deviceProperties);

  // Get optional features
  vkGetPhysicalDeviceFeatures(device, &deviceFeatures);

  if(deviceProperties.deviceType != VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) return false;
  if(!deviceFeatures.geometryShader) return false;

  // Check if device has queue family we need
  QueueFamilyIndices indices = findQueueFamilies(device);
  if(!indices.isComplete()) return false;

  return true;
}

QueueFamilyIndices VulkanApp::findQueueFamilies(VkPhysicalDevice device){
  QueueFamilyIndices indices;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(
    device, &queue_family_count, nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
    device, &queue_family_count, queue_families.data());
  
  // Identify queue families that support graphics
  int i = 0;
  for(const auto& queue_family : queue_families){
    if(queue_family.queueFlags & VK_QUEUE_GRAPHICS_BIT){
      indices.graphics_family = i;
      break;
    }
    ++i;
  }
  return indices;
}

void VulkanApp::createLogicalDevice(){
  if(physical_device_ == VK_NULL_HANDLE){
    std::runtime_error("Physical device is null, find physical device first.");
  }
  auto indices = findQueueFamilies(physical_device_);

  VkDeviceQueueCreateInfo queue_create_info{}; 
  queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
  queue_create_info.queueFamilyIndex = indices.graphics_family.value();
  queue_create_info.queueCount = 1;

  // Priority lets vulkan prioritize multiple command buffers. It's required
  // even if there is only 1 queue.
  float queue_priority = 1.0;
  queue_create_info.pQueuePriorities = &queue_priority;

  // Specify device features that we'll use.
  // TODO: everything is VK_FALSE for now, select features later.
  VkPhysicalDeviceFeatures device_features{};

  VkDeviceCreateInfo logical_device_info{};

  logical_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  logical_device_info.pQueueCreateInfos = &queue_create_info;
  logical_device_info.queueCreateInfoCount = 1;
  logical_device_info.pEnabledFeatures = &device_features;

  // Similarly to instance creation, specify extensions and validation layers
  // Difference is: device specific rather than whole application.
  // Eg. VK_KHR_swapchain for displaying rendered images to windows
  // Some devices don't have VK_KHR_swapchain like compute only devices.
  
  logical_device_info.enabledExtensionCount = 0;

  // NOTE: Device specific validation layers are deprecated. Following is
  // ignored by latest vulkan. Instead, the validation layers specified before
  // is used both for application and device.
  if(enable_valid_layers_){
    logical_device_info.enabledLayerCount =
      static_cast<uint32_t>(validation_layers_.size());
    logical_device_info.ppEnabledLayerNames =
      validation_layers_.data();
  }else{
    logical_device_info.enabledLayerCount = 0;
  }

  if(vkCreateDevice(physical_device_, &logical_device_info,
    nullptr, &logical_device_) != VK_SUCCESS){
    throw std::runtime_error("Failed to create a logical device");
  }

  // Queue is created when logical device is created, but we don't have a handle
  // to it yet.

  vkGetDeviceQueue(logical_device_, indices.graphics_family.value(), 0,
    &graphics_queue_);
}
