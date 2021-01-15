#include "VulkanApp.h"

#include <algorithm>
#include <chrono>
#include <iostream>
#include <set>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>


namespace va{
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

  window_ = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan", nullptr, nullptr);
  glfwSetWindowUserPointer(window_, this);

  glfwSetFramebufferSizeCallback(window_, frameBufferResizeCallback);
}

void VulkanApp::initVulkan() {
  createInstance();
  setupDebugMessenger();
  createSurface(); // The platform specific 'thing' to draw on
  pickPhysicalDevice();
  createLogicalDevice();
  createSwapChain();
  createImageViews();
  createRenderPass();
  createDescriptorSetLayout();
  createGraphicsPipeline();
  createCommandPool();
  createDepthResources();
  createFrameBuffers(); // After pipeline , color, depth
  createTextureImage();
  createTextureImageView();
  createTextureSampler();
  createVertexBuffer();
  createIndexBuffer();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();
  createSyncObjects();
}

void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
    drawFrame();
  }

  vkDeviceWaitIdle(logical_device_);
}

void VulkanApp::cleanUp() {
  cleanUpSwapChain();

  // Depth image
  vkDestroyImage(logical_device_, depth_image_, nullptr);
  vkFreeMemory(logical_device_, depth_image_memory_, nullptr);
  vkDestroyImageView(logical_device_, depth_image_view_, nullptr);

  // Texture sampler
  vkDestroySampler(logical_device_, texture_sampler_, nullptr);

  // Texture image
  vkDestroyImageView(logical_device_, texture_img_view_, nullptr);
  vkDestroyImage(logical_device_, texture_image_, nullptr);
  vkFreeMemory(logical_device_, texture_image_memory_, nullptr);

  // Descriptor set
  vkDestroyDescriptorSetLayout(logical_device_, descriptor_layout_, nullptr);

  // Vertex index buffer
  vkDestroyBuffer(logical_device_, index_buffer_, nullptr);
  vkFreeMemory(logical_device_, index_buffer_memory_, nullptr);

  // Vertex buffer
  vkDestroyBuffer(logical_device_, vertex_buffer_, nullptr);
  vkFreeMemory(logical_device_, vertex_buffer_memory_, nullptr);

  // Semaphores and fences
  for(size_t i = 0; i < img_available_sems_.size(); ++i){
    vkDestroySemaphore(logical_device_, img_available_sems_[i], nullptr);
    vkDestroySemaphore(logical_device_, render_finish_sems_[i], nullptr);
    vkDestroyFence(logical_device_, inflight_fences_[i], nullptr);
  }

  // Command pool (also destroys command buffers allocated from this pool)
  vkDestroyCommandPool(logical_device_, command_pool_, nullptr);

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

  // If return is true, errors would block the execution of commands on the gpu.
  // Return false so behaviour is the same in debug and release mode.
  return VK_FALSE;
}

void VulkanApp::populateDebugMessengerCreateInfo(
VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
  createInfo = {};

  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity =
      //VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
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

  // Check if device supports texture sampler anisotropy
  if(!deviceFeatures.samplerAnisotropy) return false;

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
  VkPhysicalDeviceFeatures device_features{};
  device_features.samplerAnisotropy = VK_TRUE;

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
    try {
    swapchain_imgviews_[i] = 
      createImageView(swapchain_images_[i], swapchain_img_format_,
        VK_IMAGE_ASPECT_COLOR_BIT);
    }catch(const std::exception &e){
      throw std::runtime_error("Failed to create swap chain image view.");
    }
  }
}

void VulkanApp::createGraphicsPipeline(){
  auto vert_shader_code = readFile("shader/vert.spv");
  auto frag_shader_code = readFile("shader/frag.spv");

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

  // Describes format of vertex data. Can be vertex-wise or instance-wise
  auto binding_desc = Vertex::getBindingDescription();
  auto attribute_desc = Vertex::getAttributeDescription();

  VkPipelineVertexInputStateCreateInfo vertex_ci{};
  vertex_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_ci.vertexBindingDescriptionCount = 1;
  vertex_ci.pVertexBindingDescriptions = &binding_desc;
  vertex_ci.vertexAttributeDescriptionCount =
    static_cast<uint32_t>(attribute_desc.size());
  vertex_ci.pVertexAttributeDescriptions = attribute_desc.data();

  VkPipelineInputAssemblyStateCreateInfo input_assembly_ci{};
  
  // Describe How vertex data is arranged
  input_assembly_ci.sType = 
    VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
  input_assembly_ci.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
  input_assembly_ci.primitiveRestartEnable = VK_FALSE;

  // The region of the frame buffer to draw on
  VkViewport viewport{};
  // Top left corner
  viewport.x = 0.0f;
  viewport.y = 0.0f;
  // Width and height
  viewport.width = (float) swapchain_img_extent_.width;
  viewport.height = (float) swapchain_img_extent_.height;
  viewport.minDepth = 0.0f;
  viewport.maxDepth = 1.0f;

  // Scissor rectancle effectively clips in the rectangle
  VkRect2D scissor{};
  scissor.offset = {0,0};
  scissor.extent = swapchain_img_extent_;

  // Combine viewport and scissor in a struct
  VkPipelineViewportStateCreateInfo viewport_state_ci{};
  viewport_state_ci.sType = 
    VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
  viewport_state_ci.viewportCount = 1;
  viewport_state_ci.pViewports = &viewport;
  viewport_state_ci.scissorCount = 1;
  viewport_state_ci.pScissors = &scissor;

  //Rasterizer
  VkPipelineRasterizationStateCreateInfo rasterizer{};
  rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
  rasterizer.depthClampEnable = VK_FALSE;

  // If true, geometry doesn't pass through rasterizer. no visual output.
  rasterizer.rasterizerDiscardEnable = VK_FALSE;

  rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
  rasterizer.lineWidth = 1.0f;

  rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
  rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

  // Possible to add a constant to depth values for shadow mapping
  rasterizer.depthBiasEnable = VK_FALSE;
  rasterizer.depthBiasConstantFactor = 0.0f; // Optional
  rasterizer.depthBiasClamp = 0.0f; // Optional
  rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

  VkPipelineMultisampleStateCreateInfo multisampling{};
  multisampling.sType = 
    VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
  multisampling.sampleShadingEnable = VK_FALSE;
  multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
  multisampling.minSampleShading = 1.0f; // Optional
  multisampling.pSampleMask = nullptr; // optional
  multisampling.alphaToCoverageEnable = VK_FALSE; // optional
  multisampling.alphaToOneEnable = VK_FALSE; // optional

  // Depth/stencil buffers don't have one right now.
  //VkPipelineDepthStencilStateCreateInfo depthstencil{};

  VkPipelineColorBlendAttachmentState colorblend_attach{};
  colorblend_attach.colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
    VK_COLOR_COMPONENT_G_BIT | 
    VK_COLOR_COMPONENT_B_BIT |
    VK_COLOR_COMPONENT_A_BIT;

  colorblend_attach.blendEnable = VK_FALSE;
  colorblend_attach.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; //optional
  colorblend_attach.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  colorblend_attach.colorBlendOp = VK_BLEND_OP_ADD;              // Optional
  colorblend_attach.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;   // Optional
  colorblend_attach.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;  // Optional
  colorblend_attach.alphaBlendOp = VK_BLEND_OP_ADD;              // Optional

  // Define constants for color blending procedure
  VkPipelineColorBlendStateCreateInfo colorBlending{};
  colorBlending.sType = 
  VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
  colorBlending.logicOpEnable = VK_FALSE;
  colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
  colorBlending.attachmentCount = 1;
  colorBlending.pAttachments = &colorblend_attach;
  colorBlending.blendConstants[0] = 0.0f; // Optional
  colorBlending.blendConstants[1] = 0.0f; // Optional
  colorBlending.blendConstants[2] = 0.0f; // Optional
  colorBlending.blendConstants[3] = 0.0f; // Optional

  VkDynamicState dynamic_states[]={
    VK_DYNAMIC_STATE_VIEWPORT,
    VK_DYNAMIC_STATE_LINE_WIDTH
  };

  // Some configs can be changed at runtime, put them here if you want.
  VkPipelineDynamicStateCreateInfo dynamic_state_ci{};
  dynamic_state_ci.sType =
    VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
  dynamic_state_ci.dynamicStateCount = 2;
  dynamic_state_ci.pDynamicStates = dynamic_states;

  VkPipelineLayoutCreateInfo pipeline_layout_ci{};
  pipeline_layout_ci.sType =
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.setLayoutCount = 1;
  pipeline_layout_ci.pSetLayouts = &descriptor_layout_;
  pipeline_layout_ci.pushConstantRangeCount = 0;
  pipeline_layout_ci.pPushConstantRanges = nullptr;

  if(vkCreatePipelineLayout(logical_device_, &pipeline_layout_ci, nullptr,
    &pipeline_layout_) != VK_SUCCESS){
    throw std::runtime_error("Failed to create pipeline layout");
  }

  VkGraphicsPipelineCreateInfo pipeline_ci{};
  pipeline_ci.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
  pipeline_ci.stageCount = 2;

  // Shader stage create infos
  pipeline_ci.pStages = shader_stages;

  // Fixed function stages
  pipeline_ci.pVertexInputState = &vertex_ci;
  pipeline_ci.pInputAssemblyState = &input_assembly_ci;
  pipeline_ci.pViewportState = &viewport_state_ci;
  pipeline_ci.pRasterizationState = &rasterizer;
  pipeline_ci.pMultisampleState = &multisampling;
  pipeline_ci.pDepthStencilState = nullptr;
  pipeline_ci.pColorBlendState = &colorBlending;
  pipeline_ci.pDynamicState = nullptr;

  // Layout
  pipeline_ci.layout = pipeline_layout_;
  pipeline_ci.renderPass = render_pass_;
  pipeline_ci.subpass = 0; // Index of subpass where graphics pipeline is used

  pipeline_ci.basePipelineHandle = VK_NULL_HANDLE;
  pipeline_ci.basePipelineIndex = -1;

  if(vkCreateGraphicsPipelines(logical_device_, VK_NULL_HANDLE, 1, &pipeline_ci,
    nullptr, &graphics_pipeline_) != VK_SUCCESS){
    throw std::runtime_error("Failed to create graphics pipeline");
  }

  vkDestroyShaderModule(logical_device_, vert_shader_module, nullptr);
  vkDestroyShaderModule(logical_device_, frag_shader_module, nullptr);
}

std::vector<char> VulkanApp::readFile(const std::string& filename){
  // Read spir-v shaders as bytes and store them in return value;
  // 'ate' start reading from end of file (for getting file size)
  std::ifstream file(filename, std::ios::ate | std::ios::binary);

  if (!file.is_open()) {
    throw std::runtime_error("failed to open file!");
  }

  size_t file_size = (size_t) file.tellg();
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

void VulkanApp::createRenderPass(){
  VkAttachmentDescription color_attachment{};

  // Single color buffer
  color_attachment.format = swapchain_img_format_;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

  // What to do with data in attachment before and after rendering.
  // Applies to color and depth data, not stencil data.

  // Clear the values to a constant at the start (we'll clear to black)
  color_attachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;

  // Rendered content stored to memory, can be read later on.
  color_attachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;

  // Applies to stencil data, we don't have a stencil buffer
  color_attachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
  color_attachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  
  // Which layout the image will have before render pass begins
  color_attachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  // Which layout to automatically transition to when render pass finishes.
  color_attachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

  // Subpasses, we'll just have one
  VkAttachmentReference colorattachmentref{};

  // Which attachment to reference to by idx in the attachment descriptions
  colorattachmentref.attachment = 0;

  // Which layout to have during the subpass
  colorattachmentref.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

  // Depth attachment
  VkAttachmentDescription depth_attach{};
  depth_attach.format = findDepthFormat();
  depth_attach.samples = VK_SAMPLE_COUNT_1_BIT;
  depth_attach.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
  depth_attach.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE; 
  depth_attach.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
  depth_attach.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  depth_attach.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkAttachmentReference depth_ref{};
  depth_ref.attachment = 1;
  depth_ref.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

  VkSubpassDescription subpass_desc{};
  // May support compute subpasses in the future, be explicit
  subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &colorattachmentref;
  subpass_desc.pDepthStencilAttachment = &depth_ref; // Only one allowed.

  std::array<VkAttachmentDescription,2> attachments{color_attachment,
  depth_attach};

  // We have attachment and basic subpass referencing. Create render pass
  VkRenderPassCreateInfo rp_ci{};

  rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_ci.attachmentCount = attachments.size();
  rp_ci.pAttachments = attachments.data();
  rp_ci.subpassCount = 1;
  rp_ci.pSubpasses = &subpass_desc;

  /* Subpass transitions are controlled by dependencies.
  * We have a single subpass, operations before and after count as 2 implicit
  * subpasses.
  */
  VkSubpassDependency subpass_dependency{};
  subpass_dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
  subpass_dependency.dstSubpass = 0; // Our subpass
  // Wait till the implicit subpass before ours has the color attachment bit set
  subpass_dependency.srcStageMask =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dependency.srcAccessMask = 0;

  // The operations that should wait for the color output bit in the next subpass
  // are writing color.
  subpass_dependency.dstStageMask =
    VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
  subpass_dependency.dstAccessMask = 
    VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT

    // Since depth load operation clears the depth image, the first operation
    // is a write.
    | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
  
  rp_ci.dependencyCount = 1;
  rp_ci.pDependencies = &subpass_dependency;

  if(vkCreateRenderPass(logical_device_, &rp_ci, nullptr, &render_pass_)
    != VK_SUCCESS){
    throw std::runtime_error("Failed to create render pass");
  }
}

void VulkanApp::createFrameBuffers(){
  // Create one frame buffer per image view in the swap chain
  swapchain_frame_buffers_.resize(swapchain_imgviews_.size());

  for(size_t i = 0; i < swapchain_imgviews_.size(); ++i){
    std::array<VkImageView,2> attachments{
      swapchain_imgviews_[i], depth_image_view_
    };

    VkFramebufferCreateInfo fb_ci{};
    fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    // Renderpass needs to be compatible with this frame buffer
    // i.e. roughly same number and type of attachments
    fb_ci.renderPass = render_pass_;
    fb_ci.attachmentCount = attachments.size();
    fb_ci.pAttachments = attachments.data();
    fb_ci.width = swapchain_img_extent_.width;
    fb_ci.height = swapchain_img_extent_.height;
    fb_ci.layers = 1; // # of layers in img arrays

    if(vkCreateFramebuffer(logical_device_, &fb_ci, nullptr,
      &swapchain_frame_buffers_[i]) != VK_SUCCESS){
      throw std::runtime_error("Failed to create framebuffer.");
    }
  }
}

void VulkanApp::createCommandPool(){
  QueueFamilyIndices queuefamilyindices = findQueueFamilies(physical_device_);

  VkCommandPoolCreateInfo cp_ci{};

  cp_ci.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
  cp_ci.queueFamilyIndex = queuefamilyindices.graphics_family.value();
  cp_ci.flags = 0; // Optional

  if(vkCreateCommandPool(logical_device_, &cp_ci, nullptr, &command_pool_) !=
    VK_SUCCESS){
    throw std::runtime_error("Failed to create command queue");
  }
}

void VulkanApp::createCommandBuffers(){
  command_buffers_.resize(swapchain_frame_buffers_.size());

  VkCommandBufferAllocateInfo cb_alloc_info{};
  cb_alloc_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  cb_alloc_info.commandPool = command_pool_;

  // Primary level can be submitted to queue but not called from other
  // command buffers. Secondary cannot be submitted to queue, but can be called
  // from primary command buffers.
  cb_alloc_info.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  cb_alloc_info.commandBufferCount = (uint32_t)command_buffers_.size();

  if(vkAllocateCommandBuffers(logical_device_, &cb_alloc_info,
    command_buffers_.data()) != VK_SUCCESS){
    throw std::runtime_error("Failed to allocate command buffers");
  }

  for (size_t i = 0; i < command_buffers_.size(); ++i){
    VkCommandBufferBeginInfo begin_info{};
    begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;

    // How the command buffer will be used.
    begin_info.flags = 0; // Optional
    begin_info.pInheritanceInfo = nullptr; // Optional

    if(vkBeginCommandBuffer(command_buffers_[i], &begin_info) != VK_SUCCESS){
      throw std::runtime_error("Failed to begin recording command buffer");
    }

    // Starting a render pass
    VkRenderPassBeginInfo renderpass_info{};
    renderpass_info.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderpass_info.renderPass = render_pass_;
    renderpass_info.framebuffer = swapchain_frame_buffers_[i];
    renderpass_info.renderArea.offset = { 0, 0 };
    renderpass_info.renderArea.extent = swapchain_img_extent_;

    std::array<VkClearValue,2> clear_values;
    // Order should be identical to order of attachments in render pass def
    clear_values[0].color = {0.0f, 0.0f, 0.0f, 1.0f};
    clear_values[1].depthStencil = {1.0f, 0};
    renderpass_info.clearValueCount = clear_values.size();
    renderpass_info.pClearValues = clear_values.data();

    // Start recording command to buffer, commands go to command buffer [i]
    vkCmdBeginRenderPass(command_buffers_[i], &renderpass_info,
      VK_SUBPASS_CONTENTS_INLINE);

    // bind graphics pipeline with command buffer
    vkCmdBindPipeline(command_buffers_[i], VK_PIPELINE_BIND_POINT_GRAPHICS,
      graphics_pipeline_);

    VkBuffer vertex_buffers_[]={vertex_buffer_};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(command_buffers_[i], 0, 1, vertex_buffers_, offsets);
    vkCmdBindIndexBuffer(command_buffers_[i], index_buffer_, 0,
      VK_INDEX_TYPE_UINT16);

    // Bind the right descriptor set
    vkCmdBindDescriptorSets(command_buffers_[i],
      VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline_layout_, 0, 1,
      &descriptor_sets_[i], 0, nullptr);

    vkCmdDrawIndexed(
      command_buffers_[i], static_cast<uint32_t>(indices_.size()),1,0,0,0);
    vkCmdEndRenderPass(command_buffers_[i]);
    if(vkEndCommandBuffer(command_buffers_[i])!=VK_SUCCESS){
      throw std::runtime_error("Failed to record command buffer");
    }
  }
}

void VulkanApp::drawFrame(){
  vkWaitForFences(logical_device_, 1, &inflight_fences_[current_frame_],
    VK_TRUE, UINT64_MAX);

  uint32_t img_idx;
  // Acquire image from swap chain
  VkResult acquire_result = 
  vkAcquireNextImageKHR(logical_device_, swap_chain_, UINT64_MAX,
    img_available_sems_[current_frame_], VK_NULL_HANDLE, &img_idx);

  if(acquire_result == VK_ERROR_OUT_OF_DATE_KHR){
    recreateSwapChain();
    return;
  }else if (acquire_result != VK_SUCCESS
    && acquire_result != VK_SUBOPTIMAL_KHR){
    throw std::runtime_error("Failed to acquire swapchain image.");
  }

  updateUniformBuffer(img_idx);

  // Check if previous frame is using this img
  if(images_in_flight_[img_idx] != VK_NULL_HANDLE){
    vkWaitForFences(logical_device_, 1, &images_in_flight_[img_idx],
      VK_TRUE, UINT64_MAX);
  }

  // Mark image as in use by this frame
  images_in_flight_[img_idx] = inflight_fences_[current_frame_];

  // Submit command to command buffer
  VkSubmitInfo submit_info{};
  submit_info.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

  // Idx of wait_semaphores correspond to wait_stages.
  VkSemaphore wait_semaphores[] = {img_available_sems_[current_frame_]};
  VkPipelineStageFlags wait_stages[] = {
  VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};

  submit_info.waitSemaphoreCount = 1;
  submit_info.pWaitSemaphores = wait_semaphores;
  submit_info.pWaitDstStageMask = wait_stages;

  submit_info.commandBufferCount = 1;
  submit_info.pCommandBuffers = &command_buffers_[img_idx];

  // Which semaphore to signal once operation is complete
  VkSemaphore signal_sem[] = {render_finish_sems_[current_frame_]};
  submit_info.signalSemaphoreCount = 1;
  submit_info.pSignalSemaphores = signal_sem;

  vkResetFences(logical_device_, 1, &inflight_fences_[current_frame_]);
  if(vkQueueSubmit(graphics_queue_, 1, &submit_info, 
    inflight_fences_[current_frame_]) != VK_SUCCESS){

    throw std::runtime_error("Failed to submit draw command buffer");
  }

  // Submit back to swap chain
  VkPresentInfoKHR present_info{};
  present_info.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
  present_info.waitSemaphoreCount = 1;
  present_info.pWaitSemaphores = signal_sem;

  VkSwapchainKHR swap_chains[]={swap_chain_};
  present_info.swapchainCount = 1;
  present_info.pSwapchains = swap_chains;
  present_info.pImageIndices = &img_idx;
  present_info.pResults = nullptr; // Optional

  VkResult pres_result = vkQueuePresentKHR(present_queue_,&present_info);
  if(pres_result == VK_ERROR_OUT_OF_DATE_KHR
    || pres_result == VK_SUBOPTIMAL_KHR 
    || frame_buffer_resized_){
    recreateSwapChain();
  }else if(pres_result != VK_SUCCESS){
    throw std::runtime_error("Failed to present swap chain image");
  }
  current_frame_ = (current_frame_+1)%MAX_FRAMES_IN_FLIGHT;
}

void VulkanApp::createSyncObjects(){
  img_available_sems_.resize(MAX_FRAMES_IN_FLIGHT);
  render_finish_sems_.resize(MAX_FRAMES_IN_FLIGHT);
  inflight_fences_.resize(MAX_FRAMES_IN_FLIGHT);
  images_in_flight_.resize(swapchain_images_.size(), VK_NULL_HANDLE);

  VkSemaphoreCreateInfo sem_info{};
  sem_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fence_info{};
  fence_info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fence_info.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < img_available_sems_.size(); ++i) {
    if (vkCreateSemaphore(logical_device_, &sem_info, nullptr,
                          &img_available_sems_[i]) != VK_SUCCESS ||
        vkCreateSemaphore(logical_device_, &sem_info, nullptr,
                          &render_finish_sems_[i]) != VK_SUCCESS ||
        vkCreateFence(logical_device_, &fence_info, nullptr, &inflight_fences_[i])
      != VK_SUCCESS) {
      throw std::runtime_error("Failed to create sync objects for frame");
    }
  }
}

void VulkanApp::recreateSwapChain(){
  int width = 0, height = 0;
  glfwGetFramebufferSize(window_, &width, &height);
  while(width == 0 && height == 0){
    glfwGetFramebufferSize(window_, &width, &height);
    glfwWaitEvents();
  }
  vkDeviceWaitIdle(logical_device_);
  // Clean up existing objects related to the swap chain.
  cleanUpSwapChain();

  createSwapChain();
  createImageViews();
  createRenderPass();
  createGraphicsPipeline();
  createFrameBuffers();
  createUniformBuffers();
  createDescriptorPool();
  createDescriptorSets();
  createCommandBuffers();
}

void VulkanApp::cleanUpSwapChain(){
  for(auto framebuffer : swapchain_frame_buffers_){
    vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
  }

  for(size_t i = 0; i < swapchain_images_.size(); ++i){
    vkDestroyBuffer(logical_device_, uniform_buffers_[i], nullptr);
    vkFreeMemory(logical_device_, uniform_buffers_memory_[i], nullptr);
  }

  vkDestroyDescriptorPool(logical_device_, descriptor_pool_, nullptr);
  
  vkFreeCommandBuffers(
    logical_device_,
    command_pool_,
    static_cast<uint32_t>(command_buffers_.size()),
    command_buffers_.data());

  vkDestroyPipeline(logical_device_, graphics_pipeline_, nullptr);
  vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);
  vkDestroyRenderPass(logical_device_, render_pass_, nullptr);

  for(auto imgview: swapchain_imgviews_){
    vkDestroyImageView(logical_device_, imgview, nullptr);
  }

  // Destroy swap chain before logical device
  vkDestroySwapchainKHR(logical_device_, swap_chain_, nullptr);
}

void VulkanApp::frameBufferResizeCallback(
  GLFWwindow* window, int width, int height){

  auto app = reinterpret_cast<VulkanApp*>(
    glfwGetWindowUserPointer(window));
  app->frame_buffer_resized_ = true;
}

void VulkanApp::createVertexBuffer(){
  auto buff_size = sizeof(vertices_[0]) * vertices_.size();

  // Create staging buffers for CPU side visiblity and a GPU buffer then
  // transfer data from staging to GPU.
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  createBuffer(buff_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    staging_buffer, staging_buffer_memory);

  void* data;
  // Map vertex buffer memory on gpu to cpu accessible memory
  vkMapMemory(logical_device_, staging_buffer_memory, 0, buff_size, 0, &data);

  // Copy vertex data to buffer
  memcpy(data, vertices_.data(), (size_t)buff_size);
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  // Vertex buffer local on the GPU. cannot use vkMapMemory on device memory
  createBuffer(buff_size,
    VK_BUFFER_USAGE_VERTEX_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    vertex_buffer_, vertex_buffer_memory_);

  copyBuffer(staging_buffer, vertex_buffer_, buff_size);

  // clean up staging buffer
  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
}

uint32_t VulkanApp::findMemoryType(
  uint32_t type_filter, VkMemoryPropertyFlags properties){
  VkPhysicalDeviceMemoryProperties mem_prop;

  vkGetPhysicalDeviceMemoryProperties(physical_device_, &mem_prop);

  // Find memory type that's suitable for the vertex buffer
  for(uint32_t i = 0; i < mem_prop.memoryTypeCount; ++i){
    if((type_filter & (1<<i)) &&
      (mem_prop.memoryTypes[i].propertyFlags & properties) == properties){
      // Types are fixed per index
      return i;
    }
  }
  throw std::runtime_error("Failed to find suitable memory type");
}

void VulkanApp::createBuffer(VkDeviceSize size, VkBufferUsageFlags usage_flags,
    VkMemoryPropertyFlags mem_prop_flags, VkBuffer& buffer,
    VkDeviceMemory &buffer_memory){
  VkBufferCreateInfo buff_ci{};
  buff_ci.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
  buff_ci.size = size;
  buff_ci.usage = usage_flags;
  buff_ci.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

  if(vkCreateBuffer(logical_device_, &buff_ci, nullptr, &buffer) != VK_SUCCESS){
    throw std::runtime_error("Failed to create vertex buffer");
  }

  VkMemoryRequirements mem_req;
  vkGetBufferMemoryRequirements(logical_device_, buffer, &mem_req);

  VkMemoryAllocateInfo mem_alloc_info{};
  mem_alloc_info.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  mem_alloc_info.allocationSize = mem_req.size;
  mem_alloc_info.memoryTypeIndex =
    findMemoryType(mem_req.memoryTypeBits, mem_prop_flags);
  
  if(vkAllocateMemory(logical_device_, &mem_alloc_info, nullptr, &buffer_memory) 
    != VK_SUCCESS){
    throw std::runtime_error("Failed to allocate vertex buffer memory");
  }

  // Bind memory to vertex buffer
  vkBindBufferMemory(logical_device_, buffer, buffer_memory, 0);
}

void VulkanApp::copyBuffer(VkBuffer src_buff, VkBuffer dst_buff,
    VkDeviceSize size){
  // Data transfer between memory buffers go through command buffers.
  // Create transient command pool for this purpose.
  VkCommandBuffer command_buffer =  beginSingleTimeCommands();

  VkBufferCopy copy_region{};
  copy_region.srcOffset = 0;
  copy_region.dstOffset = 0;
  copy_region.size = size;
  vkCmdCopyBuffer(command_buffer, src_buff, dst_buff, 1, &copy_region);

  endSingleTimeCommands(command_buffer);
}

void VulkanApp::createIndexBuffer(){
  VkDeviceSize buff_size = sizeof(indices_[0]) * indices_.size();

  // Create staging buffer
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;
  createBuffer(buff_size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT|VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    staging_buffer, staging_buffer_memory);

  void *data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, buff_size, 0, &data);
  memcpy(data, indices_.data(), (size_t)buff_size);
  vkUnmapMemory(logical_device_, staging_buffer_memory);

  createBuffer(buff_size,
    VK_BUFFER_USAGE_TRANSFER_DST_BIT|VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, index_buffer_, index_buffer_memory_);

  copyBuffer(staging_buffer, index_buffer_, buff_size);

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
}

void VulkanApp::createDescriptorSetLayout(){
  VkDescriptorSetLayoutBinding ubo_layout_binding{};
  // There can be an array of buffers, eg. one for each tf for model bones
  ubo_layout_binding.binding = 0;
  ubo_layout_binding.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  ubo_layout_binding.descriptorCount = 1;

  // Only referencing descriptor during vertex shading.
  ubo_layout_binding.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
  
  ubo_layout_binding.pImmutableSamplers = nullptr; // Optional

  VkDescriptorSetLayoutBinding sampler_layout_binding{};
  sampler_layout_binding.binding = 1;
  sampler_layout_binding.descriptorCount = 1;
  sampler_layout_binding.descriptorType =
    VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  sampler_layout_binding.pImmutableSamplers = nullptr;
  sampler_layout_binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

  std::array<VkDescriptorSetLayoutBinding, 2> bindings = {
    ubo_layout_binding, sampler_layout_binding
  };

  VkDescriptorSetLayoutCreateInfo layout_info{};
  layout_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
  layout_info.bindingCount = static_cast<uint32_t>(bindings.size());
  layout_info.pBindings = bindings.data();

  if (vkCreateDescriptorSetLayout(logical_device_, &layout_info, nullptr,
                                  &descriptor_layout_) != VK_SUCCESS) {
    throw std::runtime_error("Failed to create descriptor set layout");
  }
}

void VulkanApp::createUniformBuffers(){
  auto buff_size = sizeof(UniformBufferObject);

  uniform_buffers_.resize(swapchain_images_.size());
  uniform_buffers_memory_.resize(swapchain_images_.size());

  for (size_t i = 0; i < uniform_buffers_.size(); ++i) {
    createBuffer(buff_size, VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                     VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                 uniform_buffers_[i], uniform_buffers_memory_[i]);
  }
}

void VulkanApp::updateUniformBuffer(uint32_t uniform_buffer_idx){
  static auto start_time = std::chrono::high_resolution_clock::now(); 

  auto current_time = std::chrono::high_resolution_clock::now();
  float time = std::chrono::duration<float, std::chrono::seconds::period>(
    current_time - start_time).count();

  UniformBufferObject ubo{};
  ubo.model = glm::rotate(glm::mat4(1.0f), time*glm::radians(90.0f),
    glm::vec3(0.0f, 0.0f, 1.0f));
  ubo.view = glm::lookAt(glm::vec3(2.0f,2.0f,2.0f), glm::vec3(0.0f,0.0f,0.0f),
    glm::vec3(0.0f,0.0f,1.0f));
  // 45 deg vertical fov, 0.1 near plane, 10 far plane.
  ubo.proj = glm::perspective(glm::radians(45.0f),
    swapchain_img_extent_.width/(float)swapchain_img_extent_.height, 0.1f, 10.0f);

  // positive y downward on screen.
  ubo.proj[1][1] *= -1;

  void *data;
  vkMapMemory(logical_device_, uniform_buffers_memory_[uniform_buffer_idx],
    0, sizeof(ubo), 0, &data);
  memcpy(data, &ubo, sizeof(ubo));
  vkUnmapMemory(logical_device_, uniform_buffers_memory_[uniform_buffer_idx]);
}

void VulkanApp::createDescriptorPool(){
  // What types of descriptors will be contained in descriptor set
  // How many there will be
  std::array<VkDescriptorPoolSize,2> dp_sizes{};

  // 1 for each swap chain image
  dp_sizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
  dp_sizes[0].descriptorCount = static_cast<uint32_t>(swapchain_images_.size());
  dp_sizes[1].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
  dp_sizes[1].descriptorCount = static_cast<uint32_t>(swapchain_images_.size());

  VkDescriptorPoolCreateInfo dp_info{};
  dp_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
  dp_info.poolSizeCount = static_cast<uint32_t>(dp_sizes.size());
  dp_info.pPoolSizes = dp_sizes.data();

  // Maximum # of descriptor sets that may be allocated
  dp_info.maxSets = static_cast<uint32_t>(swapchain_images_.size());

  if(vkCreateDescriptorPool(logical_device_, &dp_info, nullptr,
    &descriptor_pool_) != VK_SUCCESS){
    throw std::runtime_error("Failed to create descriptor pool");
  }
}

void VulkanApp::createDescriptorSets(){
  std::vector<VkDescriptorSetLayout> layouts(swapchain_images_.size(),
    descriptor_layout_);

  VkDescriptorSetAllocateInfo alloc_info{};
  alloc_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
  alloc_info.descriptorPool = descriptor_pool_;
  alloc_info.descriptorSetCount = 
    static_cast<uint32_t>(swapchain_images_.size());
  alloc_info.pSetLayouts = layouts.data();

  descriptor_sets_.resize(swapchain_images_.size());
  if(vkAllocateDescriptorSets(logical_device_, &alloc_info,
    descriptor_sets_.data()) != VK_SUCCESS){
    throw std::runtime_error("Failed to create descriptor sets");
  }

  // Bind sampler and texture image to descriptor sets
  VkDescriptorImageInfo imageinfo{};
  imageinfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
  imageinfo.sampler = texture_sampler_;
  imageinfo.imageView = texture_img_view_;

  // Descriptor sets created, but empty. populate them.
  // Link up resources of descriptors to descriptor sets.
  for(size_t i = 0; i < descriptor_sets_.size(); ++i){
    VkDescriptorBufferInfo buffer_info{};
    buffer_info.buffer = uniform_buffers_[i];
    buffer_info.offset = 0;
    buffer_info.range = sizeof(UniformBufferObject);

    std::array<VkWriteDescriptorSet,2> writes{};
    
    VkWriteDescriptorSet write{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = descriptor_sets_[i];
    writes[0].dstBinding = 0;
    writes[0].dstArrayElement = 0; // Descriptors can be array. Use the first one.
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].descriptorCount = 1;
    writes[0].pBufferInfo = &buffer_info; // refers to buffer data

    writes[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[1].dstSet = descriptor_sets_[i];
    writes[1].dstBinding = 1;
    writes[1].dstArrayElement = 0;
    writes[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    writes[1].descriptorCount = 1;
    writes[1].pImageInfo = &imageinfo;
    
    vkUpdateDescriptorSets(logical_device_, 
      static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
  }
}

void VulkanApp::createTextureImage(){
  // Load image
  int t_width, t_height, t_channels;

  stbi_uc* pixels = stbi_load("textures/texture.jpg", &t_width, &t_height,
    &t_channels, STBI_rgb_alpha);

  VkDeviceSize image_size = t_width*t_height*4;

  if(!pixels){
    throw std::runtime_error("Failed to load texture image");
  }

  // Image staging buffer
  VkBuffer staging_buffer;
  VkDeviceMemory staging_buffer_memory;

  createBuffer(image_size,
    VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
    staging_buffer, staging_buffer_memory);

  // Copy image data to vulkan buffer
  void* data;
  vkMapMemory(logical_device_, staging_buffer_memory, 0, image_size, 0, &data);
  memcpy(data, pixels, image_size);
  vkUnmapMemory(logical_device_, staging_buffer_memory);
  stbi_image_free(pixels);

  // Shader can read image from the buffer, but it's better to move to Image
  createImage(t_width,t_height,VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_TRANSFER_DST_BIT 
    | VK_IMAGE_USAGE_SAMPLED_BIT,VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
    texture_image_, texture_image_memory_);

  // Transition image to proper layout for copying
  transitionImageLayout(texture_image_,
    VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

  copyBufferToImage(staging_buffer, texture_image_,
    static_cast<uint32_t>(t_width), static_cast<uint32_t>(t_height));

  // One last transition so shader can sample texels
  transitionImageLayout(texture_image_, VK_FORMAT_R8G8B8A8_SRGB,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
    VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

  vkDestroyBuffer(logical_device_, staging_buffer, nullptr);
  vkFreeMemory(logical_device_, staging_buffer_memory, nullptr);
}

void VulkanApp::createImage(uint32_t width, uint32_t height, VkFormat format,
  VkImageTiling tiling, VkImageUsageFlags usage,
  VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &image_mem){

  VkImageCreateInfo imageinfo{};
  imageinfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
  imageinfo.imageType = VK_IMAGE_TYPE_2D;
  imageinfo.extent.width = static_cast<uint32_t>(width);
  imageinfo.extent.height = static_cast<uint32_t>(height);
  imageinfo.extent.depth = 1;
  imageinfo.mipLevels = 1;
  imageinfo.arrayLayers = 1; 
  // Use same format for texel as the pixels in the buffer or copy will fail
  imageinfo.format = format;
  imageinfo.tiling = tiling;
  imageinfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
  imageinfo.usage = usage;
  imageinfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
  imageinfo.samples = VK_SAMPLE_COUNT_1_BIT;
  imageinfo.flags = 0; // Optional

  if(vkCreateImage(logical_device_, &imageinfo, nullptr, &image) !=
    VK_SUCCESS){
    throw std::runtime_error("Failed to create texture image");
  }

  VkMemoryRequirements mem_req;
  vkGetImageMemoryRequirements(logical_device_, image, &mem_req);

  VkMemoryAllocateInfo memalloc{};
  memalloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
  memalloc.allocationSize = mem_req.size;
  memalloc.memoryTypeIndex = findMemoryType(mem_req.memoryTypeBits, properties);

  if (vkAllocateMemory(logical_device_, &memalloc, nullptr,
                       &image_mem) != VK_SUCCESS) {
    throw std::runtime_error("Failed to allocate image memory");
  }

  vkBindImageMemory(logical_device_, image, image_mem, 0);
}

VkCommandBuffer VulkanApp::beginSingleTimeCommands(){
  VkCommandBufferAllocateInfo allocinfo{};
  allocinfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
  allocinfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
  allocinfo.commandPool = command_pool_;
  allocinfo.commandBufferCount = 1;

  VkCommandBuffer command_buffer;
  vkAllocateCommandBuffers(logical_device_, &allocinfo, &command_buffer);

  VkCommandBufferBeginInfo begininfo{};
  begininfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
  begininfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

  vkBeginCommandBuffer(command_buffer, &begininfo);

  return command_buffer;
}

void VulkanApp::endSingleTimeCommands(VkCommandBuffer command_buffer){
  vkEndCommandBuffer(command_buffer);

  VkSubmitInfo si{};
  si.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
  si.commandBufferCount = 1;
  si.pCommandBuffers = &command_buffer;

  vkQueueSubmit(graphics_queue_, 1, &si, VK_NULL_HANDLE);
  vkQueueWaitIdle(graphics_queue_);

  vkFreeCommandBuffers(logical_device_, command_pool_, 1, &command_buffer);
}

void VulkanApp::transitionImageLayout(VkImage image, VkFormat format, 
    VkImageLayout old_layout, VkImageLayout new_layout){
  VkCommandBuffer cb = beginSingleTimeCommands();

  VkImageMemoryBarrier barrier{};
  barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
  barrier.oldLayout = old_layout;
  barrier.newLayout = new_layout;

  barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
  barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;

  barrier.image = image;

  if(new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL){
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

    // Turn on stencil bit as well.
    if(hasStencilComponent(format))
      barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;

  }else{
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  }
  barrier.subresourceRange.baseMipLevel = 0;
  barrier.subresourceRange.levelCount = 1;
  barrier.subresourceRange.baseArrayLayer = 0;
  barrier.subresourceRange.layerCount = 1;

  VkPipelineStageFlags srcstage;
  VkPipelineStageFlags dststage;

  if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED
    && new_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL){
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

    // earliest possible pipeline stage, wait for top of pipeline.
    srcstage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    // pseudo stage
    dststage = VK_PIPELINE_STAGE_TRANSFER_BIT;
  }else if(old_layout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
    && new_layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL){
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    srcstage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    dststage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
  }else if(old_layout == VK_IMAGE_LAYOUT_UNDEFINED
    && new_layout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL){
    barrier.srcAccessMask = 0;
    barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT
      | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    
    srcstage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    dststage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;

  }else{
    throw std::invalid_argument("Unsupported image layout transition");
  }

  vkCmdPipelineBarrier(
    cb,
    srcstage,dststage,
    0,
    0, nullptr,
    0, nullptr,
    1, &barrier
  );

  endSingleTimeCommands(cb);
}

void VulkanApp::copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
  uint32_t height){
  VkCommandBuffer cb = beginSingleTimeCommands();

  VkBufferImageCopy region{};
  region.bufferOffset = 0;
  region.bufferRowLength = 0;
  region.bufferImageHeight = 0;

  region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
  region.imageSubresource.mipLevel = 0;
  region.imageSubresource.baseArrayLayer = 0;
  region.imageSubresource.layerCount = 1;

  region.imageOffset = {0,0,0};
  region.imageExtent = {width, height, 1};
  
  vkCmdCopyBufferToImage(cb, buffer, image,
    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

  endSingleTimeCommands(cb);
}

void VulkanApp::createTextureImageView(){
  try {
    texture_img_view_ = createImageView(texture_image_, VK_FORMAT_R8G8B8A8_SRGB,
      VK_IMAGE_ASPECT_COLOR_BIT);
  }catch(const std::exception &e){
    throw std::runtime_error("Failed to create texture image view.");
  }
}

VkImageView VulkanApp::createImageView(VkImage image, VkFormat format,
  VkImageAspectFlags aspect_mask){
  VkImageViewCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
  ci.image = image;
  ci.viewType = VK_IMAGE_VIEW_TYPE_2D;
  ci.format = format;
  ci.subresourceRange.aspectMask = aspect_mask;
  ci.subresourceRange.baseMipLevel = 0;
  ci.subresourceRange.levelCount = 1;
  ci.subresourceRange.baseArrayLayer = 0;
  ci.subresourceRange.layerCount = 1;

  VkImageView iv;
  if(vkCreateImageView(logical_device_, &ci, nullptr, &iv) !=
    VK_SUCCESS){
    throw std::runtime_error("Failed to create image view.");
  }

  return iv;
}

void VulkanApp::createTextureSampler(){
  VkSamplerCreateInfo ci{};
  ci.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;

  // How to interpolate texels that are magnified or minified
  ci.magFilter = VK_FILTER_LINEAR; // related to oversampling
  ci.minFilter = VK_FILTER_LINEAR; // related to undersampling

  // Axis-wise handling of sampling outside the texture
  ci.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ci.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
  ci.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;

  ci.anisotropyEnable = VK_TRUE;

  // Maximum texel samples to determine the final color. more = looks better,
  // but may be slower in performance
  VkPhysicalDeviceProperties properties{};
  vkGetPhysicalDeviceProperties(physical_device_, &properties);
  ci.maxAnisotropy = properties.limits.maxSamplerAnisotropy; // max quality.

  // This is the color shown when sampling beyond texture border and addressmode
  // is set to clamp to border.
  // Only black,white,or transparent. no arbitrary color.
  ci.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;

  ci.unnormalizedCoordinates = VK_FALSE;

  // Mainly for 'percentage-closer filtering' for shadow maps.
  ci.compareEnable = VK_FALSE;
  ci.compareOp = VK_COMPARE_OP_ALWAYS;

  // For mipmapping
  ci.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
  ci.mipLodBias = 0.0f;
  ci.minLod = 0.0f;
  ci.maxLod = 0.0f;

  if(vkCreateSampler(logical_device_, &ci, nullptr, &texture_sampler_)
    != VK_SUCCESS){
    throw std::runtime_error("Failed to create texture sampler.");
  }
}

void VulkanApp::createDepthResources(){
  VkFormat format = findDepthFormat();
  createImage(swapchain_img_extent_.width, swapchain_img_extent_.height,
   format, VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
    VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, depth_image_, depth_image_memory_); 

  depth_image_view_ = createImageView(depth_image_, format,
    VK_IMAGE_ASPECT_DEPTH_BIT);

  transitionImageLayout(depth_image_, format, VK_IMAGE_LAYOUT_UNDEFINED,
    VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

VkFormat VulkanApp::findSupportedImageFormat(
  const std::vector<VkFormat> &candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features){

  for(const auto & format : candidates){
    VkFormatProperties props{};
    vkGetPhysicalDeviceFormatProperties(physical_device_, format, &props);

    if(tiling == VK_IMAGE_TILING_LINEAR
      && (props.linearTilingFeatures & features) == features){
      return format;
    }else if(tiling == VK_IMAGE_TILING_OPTIMAL
      && (props.optimalTilingFeatures & features) == features){
      return format;
    }
  }

  throw std::runtime_error("Failed to find supported image format");
}

VkFormat VulkanApp::findDepthFormat(){
  return findSupportedImageFormat(
    {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, 
    VK_FORMAT_D24_UNORM_S8_UINT},
    VK_IMAGE_TILING_OPTIMAL, // let vulkan handle internal gpu memory tiling
    VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
}

bool VulkanApp::hasStencilComponent(VkFormat format){
  return format == VK_FORMAT_D32_SFLOAT
    || format == VK_FORMAT_D24_UNORM_S8_UINT;
}
}// namespace va
