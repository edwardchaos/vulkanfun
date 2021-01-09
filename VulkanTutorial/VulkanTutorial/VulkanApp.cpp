#include "VulkanApp.h"

#include <algorithm>
#include <iostream>
#include <set>

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
  initWindow(); // Empty rectangular container
  initVulkan(); // Main interface between vulkan and this application
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
  createSurface(); // The platform specific 'thing' to draw on
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createGraphicsPipeline();
}

void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
  }
}

void VulkanApp::cleanUp() {
  for(auto imgview: swapchain_imgviews_){
    vkDestroyImageView(logical_device_, imgview, nullptr);
  }

  // Destroy swap chain before logical device
  vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);

  // Logical device 
  vkDestroyDevice(logical_device_, nullptr);

  // Clean up other vulkan resources here first before instance
  if (enable_valid_layers_) {
    DestroyDebugUtilsMessengerEXT(instance_, debug_messenger_, nullptr);
  }

  // Destroy surface, needs to be done before the instance.
  vkDestroySurfaceKHR(instance_, surface_, nullptr);

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

  // Check device for required extensions(swap chain, etc)
  bool extensions_supported = checkDeviceExtensionSupport(device);
  if(!extensions_supported) return false;

  // Query swap chain support
  auto swap_chain_details = querySwapChainSupport(device);
  // has at least 1 supported image format and 1 presentation mode for our
  // surface.
  bool swap_chain_adequate = !swap_chain_details.formats.empty() &&
    !swap_chain_details.present_modes.empty();
  if(!swap_chain_adequate) return false;
  // Still need to choose the right settings for swap chain.

  return true;
}

QueueFamilyIndices VulkanApp::findQueueFamilies(VkPhysicalDevice device){
  QueueFamilyIndices indices;

  // For finding queue family that supports presenting on a surface.
  VkBool32 present_support = false;

  uint32_t queue_family_count = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(
    device, &queue_family_count, nullptr);

  std::vector<VkQueueFamilyProperties> queue_families(queue_family_count);
  vkGetPhysicalDeviceQueueFamilyProperties(
    device, &queue_family_count, queue_families.data());
  
  // Identify queue families that support graphics
  int i = 0;
  for(const auto& queue_family : queue_families){

    // Present queue family and graphics queue family need not be the same.
    vkGetPhysicalDeviceSurfaceSupportKHR(
      device, i, surface_, &present_support);
    if(present_support){
      indices.present_family = i;
    }

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

  std::set<uint32_t> unique_queue_families{
    indices.graphics_family.value(), indices.present_family.value()};

  std::vector<VkDeviceQueueCreateInfo> queue_create_infos{}; 

  for(const auto& queuefamily : unique_queue_families){
    VkDeviceQueueCreateInfo queue_create_info{};

    queue_create_info.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queue_create_info.queueFamilyIndex = queuefamily;
    queue_create_info.queueCount = 1;

    // Priority lets vulkan prioritize multiple command buffers. It's required
    // even if there is only 1 queue.
    float queue_priority = 1.0;
    queue_create_info.pQueuePriorities = &queue_priority;
    queue_create_infos.push_back(queue_create_info);
  }

  // Specify device features that we'll use.
  // TODO: everything is VK_FALSE for now, select features later.
  VkPhysicalDeviceFeatures device_features{};

  VkDeviceCreateInfo logical_device_info{};

  logical_device_info.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
  logical_device_info.pQueueCreateInfos = queue_create_infos.data();
  logical_device_info.queueCreateInfoCount = 1;
  logical_device_info.pEnabledFeatures = &device_features;

  // Similarly to instance creation, specify extensions and validation layers
  // Difference is: device specific rather than whole application.
  // Eg. VK_KHR_swapchain for displaying rendered images to windows
  // Some devices don't have VK_KHR_swapchain like compute only devices.
  
  logical_device_info.enabledExtensionCount =
    static_cast<uint32_t>(device_extensions_.size());
  logical_device_info.ppEnabledExtensionNames = device_extensions_.data();

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

  // Queue is created when logical device is created, but we don't have 
  // a handle to it yet.

  // If graphics and present queues are the same, these handles will be the same.
  vkGetDeviceQueue(logical_device_, indices.graphics_family.value(), 0,
    &graphics_queue_);
  vkGetDeviceQueue(logical_device_, indices.present_family.value(), 0,
    &present_queue_);
}

void VulkanApp::createSurface() {
  // Window system integration (WSI) create surface for specific platform
  if (glfwCreateWindowSurface(instance_, window_, nullptr, &surface_) !=
      VK_SUCCESS){
    throw std::runtime_error("Failed to create window surface");
  }
}

bool VulkanApp::checkDeviceExtensionSupport(VkPhysicalDevice device){
  uint32_t ext_count = 0;

  vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, nullptr);

  std::vector<VkExtensionProperties> available_extensions(ext_count);

  vkEnumerateDeviceExtensionProperties(device, nullptr, &ext_count, 
    available_extensions.data());

  std::set<std::string> required_ext(device_extensions_.begin(),
    device_extensions_.end());

  for(const auto& ext : available_extensions){
    required_ext.erase(ext.extensionName);
  }

  return required_ext.empty();
}

SwapChainSupportDetails
VulkanApp::querySwapChainSupport(VkPhysicalDevice device){
  SwapChainSupportDetails details;

  // Query surface capabilities
  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface_,
    &details.capabilities);

  // Query supported surface formats need to make sure our glfw surface is
  // compatible.

  uint32_t format_count = 0;

  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count,
    nullptr);

  if(format_count > 0){
    details.formats.resize(format_count);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface_, &format_count,
      details.formats.data());
  }

  // Query supported presentation modes
  uint32_t present_count = 0;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_count,
    nullptr);

  if(present_count > 0){
    details.present_modes.resize(present_count);
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface_, &present_count,
      details.present_modes.data());
  }

  return details;
}

VkSurfaceFormatKHR VulkanApp::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &available_formats){
  if(available_formats.empty())
    throw std::runtime_error("No available formats");

  // Format specifies color channel and types eg.VK_FORMAT_B8G8R8A8_SRGB

  // color space specifies supported color space eg. SRGB

  for(const auto& avail_format : available_formats){
    if(avail_format.format == VK_FORMAT_B8G8R8A8_SRGB &&
       avail_format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR){
      return avail_format;
    }
  }

  // Otherwise settle with first format
  return available_formats[0];
}

VkPresentModeKHR VulkanApp::chooseSwapPresentMode(
const std::vector<VkPresentModeKHR> &available_present_modes){
  if(available_present_modes.empty())
    throw std::runtime_error("No available present mode");

  for(const auto& present_mode : available_present_modes){
    // Look for triple buffering
    if(present_mode == VK_PRESENT_MODE_MAILBOX_KHR)
      return present_mode;
  }

  // This is guaranteed to be available.
  return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D VulkanApp::chooseSwapExtent(
const VkSurfaceCapabilitiesKHR& capabilities){
  if(capabilities.currentExtent.width != UINT32_MAX)
    return capabilities.currentExtent;

  // glfw window size specified in screen coordinates, not pixels
  int width_px ,height_px;
  glfwGetFramebufferSize(window_, &width_px, &height_px);

  // Get the actual pixel dimensions of the window
  VkExtent2D actualExtent = {
    static_cast<uint32_t>(width_px),
    static_cast<uint32_t>(height_px)
  };

  // Extent needs to be within the bounds of the swap chain capabilities.
  actualExtent.width = std::max(capabilities.minImageExtent.width,
    std::min(capabilities.maxImageExtent.width, actualExtent.width));
  actualExtent.height = std::max(capabilities.minImageExtent.height,
    std::min(capabilities.maxImageExtent.height, actualExtent.height));

  return actualExtent;
}

void VulkanApp::createSwapChain(){
  SwapChainSupportDetails swap_chain_support =
    querySwapChainSupport(physical_device_);

  VkSurfaceFormatKHR surface_format = chooseSwapSurfaceFormat(
    swap_chain_support.formats);

  VkPresentModeKHR present_mode = chooseSwapPresentMode(
    swap_chain_support.present_modes);

  VkExtent2D extent = chooseSwapExtent(swap_chain_support.capabilities);

  // Minimum number of images in the swap chain
  uint32_t min_img_count = swap_chain_support.capabilities.minImageCount;
  // Request at least 1 more image than minimum for good luck
  ++min_img_count;

  if(swap_chain_support.capabilities.maxImageCount > 0 &&
    min_img_count > swap_chain_support.capabilities.maxImageCount){
    min_img_count = swap_chain_support.capabilities.maxImageCount;
  }

  // Create the struct for creating a swap chain
  VkSwapchainCreateInfoKHR sc_create_info{};

  sc_create_info.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
  sc_create_info.surface = surface_;

  sc_create_info.minImageCount = min_img_count;
  sc_create_info.imageFormat = surface_format.format;
  sc_create_info.imageColorSpace = surface_format.colorSpace;
  sc_create_info.imageExtent = extent;
  sc_create_info.imageArrayLayers = 1; // 1 unless steroscopic 3D application

  // alternative eg. VK_IMAGE_USAGE_TRANSFER_DST_BIT
  sc_create_info.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

  /* Handle possibility of multiple queue handles,
  * If graphics and presentation queue families are different, ownership
  * of swap chain needs to be shared (slower).
  * Ownership can be transfered but there's work to do for that.
  */ 

  auto indices = findQueueFamilies(physical_device_);
  uint32_t queue_fam_indices[]={indices.graphics_family.value(),
  indices.present_family.value()};

  if(indices.graphics_family != indices.present_family){
    sc_create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    sc_create_info.queueFamilyIndexCount = 2;
    sc_create_info.pQueueFamilyIndices = queue_fam_indices;
  }else{
    sc_create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    sc_create_info.queueFamilyIndexCount = 0; //optional but shown here
    sc_create_info.pQueueFamilyIndices = nullptr; // same.
  }

  sc_create_info.preTransform = 
    swap_chain_support.capabilities.currentTransform;
  sc_create_info.presentMode = present_mode;
  // How the window should blend with other windows.
  sc_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  // If another window is infront
  sc_create_info.clipped = VK_TRUE;
  
  // If window is resized, a new one has to be created and a refernce to the
  // old one put here. We'll only have 1 swapchain, 1 size.
  sc_create_info.oldSwapchain = VK_NULL_HANDLE;

  if(vkCreateSwapchainKHR(logical_device_, &sc_create_info, nullptr,
    &swap_chain_) != VK_SUCCESS){
    throw std::runtime_error("Failed to create swap chain.");
  }

  uint32_t img_count;
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &img_count, nullptr);

  swapchain_images_.resize(img_count);
  vkGetSwapchainImagesKHR(logical_device_, swap_chain_, &img_count,
    swapchain_images_.data());

  // Save these for later
  swapchain_img_format_ = surface_format.format;
  swapchain_img_extent_ = extent;
 }

void VulkanApp::createImageViews(){
  swapchain_imgviews_.resize(swapchain_images_.size());

  VkImageViewCreateInfo imgview_create_info{};
  for(size_t i = 0; i < swapchain_images_.size(); ++i){
    imgview_create_info.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imgview_create_info.image = swapchain_images_[i];
    imgview_create_info.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imgview_create_info.format = swapchain_img_format_;

    imgview_create_info.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    imgview_create_info.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    imgview_create_info.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    imgview_create_info.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // Image purpose and which part of image to access
    imgview_create_info.subresourceRange.aspectMask = 
      VK_IMAGE_ASPECT_COLOR_BIT;
    // any mipmapping?
    imgview_create_info.subresourceRange.baseMipLevel = 0;
    imgview_create_info.subresourceRange.levelCount = 1;
    imgview_create_info.subresourceRange.baseArrayLayer = 0;
    imgview_create_info.subresourceRange.layerCount = 1;

    if(vkCreateImageView(logical_device_, &imgview_create_info, nullptr,
      &swapchain_imgviews_[i])
      !=VK_SUCCESS){
      throw std::runtime_error("Failed to create image view.");
    }
  }
}

void VulkanApp::createGraphicsPipeline(){
  auto vert_shader_code = readFile("vert.spv");
  auto frag_shader_code = readFile("frag.spv");

  VkShaderModule vert_shader_module = createShaderModule(vert_shader_code);
  VkShaderModule frag_shader_module = createShaderModule(frag_shader_code);

  VkPipelineShaderStageCreateInfo vert_shader_ci{};
  vert_shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

  // Use vertex shader code in vertex shading stage
  vert_shader_ci.stage = VK_SHADER_STAGE_VERTEX_BIT;

  vert_shader_ci.module = vert_shader_module;
  vert_shader_ci.pName = "main"; // The function to invoke (entrypoint)
  vert_shader_ci.pSpecializationInfo = nullptr; // specify shader constants

  VkPipelineShaderStageCreateInfo frag_shader_ci{};
  frag_shader_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;

  // Use vertex shader code in vertex shading stage
  frag_shader_ci.stage = VK_SHADER_STAGE_FRAGMENT_BIT;

  frag_shader_ci.module = frag_shader_module;
  frag_shader_ci.pName = "main"; // The function to invoke (entrypoint)

  VkPipelineShaderStageCreateInfo shader_stages[]=
  {vert_shader_ci, frag_shader_ci};

  vkDestroyShaderModule(logical_device_, vert_shader_module, nullptr);
  vkDestroyShaderModule(logical_device_, frag_shader_module, nullptr);

  // TODO: Use shader create infos to create the shader later
}

std::vector<char> VulkanApp::readFile(const std::string& filename){
  // Read spir-v shaders as bytes and store them in return value;
  // 'ate' start reading from end of file (for getting file size)
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t file_size = (size_t) file.tellg();
  std::cout << "shader file size:" << file_size << std::endl;
  std::vector<char> buffer(file_size);

  // Back to beginning
  file.seekg(0);
  // Read all data from beginning
  file.read(buffer.data(), file_size);

  file.close();
  return buffer;
}

VkShaderModule VulkanApp::createShaderModule(
  const std::vector<char> &code){
  
  VkShaderModuleCreateInfo shader_ci{};

  shader_ci.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
  shader_ci.codeSize = code.size();

  // For reinterpret cast, data needs to satisfy alignment requirement of uint32
  // std::vector handles that.
  shader_ci.pCode = reinterpret_cast<const uint32_t*>(code.data());

  VkShaderModule shader;
  if(vkCreateShaderModule(logical_device_, &shader_ci, nullptr, &shader) !=
    VK_SUCCESS){
    throw std::runtime_error("Failed to create shader module");
  }

  return shader;
}
