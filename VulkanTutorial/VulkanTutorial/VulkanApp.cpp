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
  createRenderPass();
  createGraphicsPipeline();
  createFrameBuffers();
  createCommandPool();
}

void VulkanApp::mainLoop() {
  while (!glfwWindowShouldClose(window_)) {
    glfwPollEvents();
  }
}

void VulkanApp::cleanUp() {
  vkDestroyCommandPool(logical_device_, command_pool_, nullptr);
  vkDestroyPipeline(logical_device_, graphics_pipeline_, nullptr);
  vkDestroyPipelineLayout(logical_device_, pipeline_layout_, nullptr);

  for(auto framebuffer : swapchain_frame_buffers_){
    vkDestroyFramebuffer(logical_device_, framebuffer, nullptr);
  }
  vkDestroyRenderPass(logical_device_, render_pass_, nullptr);

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

  // TODO: Use shader create infos to create the shader later

  // Describes format of vertex data. Can be vertex-wise or instance-wise
  VkPipelineVertexInputStateCreateInfo vertex_ci{};
  vertex_ci.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
  vertex_ci.vertexBindingDescriptionCount = 0;
  vertex_ci.pVertexBindingDescriptions = nullptr;
  vertex_ci.vertexAttributeDescriptionCount = 0;
  vertex_ci.pVertexAttributeDescriptions = nullptr;

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
  rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;

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

  // Required to create this struct even if we don't need one now.
  VkPipelineLayoutCreateInfo pipeline_layout_ci{};
  pipeline_layout_ci.sType =
    VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
  pipeline_layout_ci.setLayoutCount = 0;
  pipeline_layout_ci.pSetLayouts = nullptr;
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

  // Sigle color buffer
  color_attachment.format = swapchain_img_format_;
  color_attachment.samples = VK_SAMPLE_COUNT_1_BIT;

  // What to do with data in attachment before and after rendering,
  // applies to color and depth data, not stencil data.

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

  VkSubpassDescription subpass_desc{};
  // May support compute subpasses in the future, be explicit
  subpass_desc.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
  subpass_desc.colorAttachmentCount = 1;
  subpass_desc.pColorAttachments = &colorattachmentref;

  // We have attachment and basic subpass referencing. Create render pass
  VkRenderPassCreateInfo rp_ci{};

  rp_ci.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
  rp_ci.attachmentCount = 1;
  rp_ci.pAttachments = &color_attachment;
  rp_ci.subpassCount = 1;
  rp_ci.pSubpasses = &subpass_desc;

  if(vkCreateRenderPass(logical_device_, &rp_ci, nullptr, &render_pass_)
    != VK_SUCCESS){
    throw std::runtime_error("Failed to create render pass");
  }
}

void VulkanApp::createFrameBuffers(){
  // Create one frame buffer per image view in the swap chain
  swapchain_frame_buffers_.resize(swapchain_imgviews_.size());

  for(size_t i = 0; i < swapchain_imgviews_.size(); ++i){
    VkImageView attachments[]={
      swapchain_imgviews_[i]
    };

    VkFramebufferCreateInfo fb_ci{};
    fb_ci.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    // Renderpass needs to be compatible with this frame buffer
    // i.e. roughly same number and type of attachments
    fb_ci.renderPass = render_pass_;
    fb_ci.attachmentCount = 1;
    fb_ci.pAttachments = attachments;
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
