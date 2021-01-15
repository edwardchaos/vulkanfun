#pragma once

#include <array>
#include <fstream>
#include <optional>
#include <vector>

#define GLFW_INCLUDE_VULKAN
#include "GLFW/glfw3.h"

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/hash.hpp>

namespace va {

struct UniformBufferObject {
  glm::mat4 model;
  glm::mat4 view;
  glm::mat4 proj;
};

struct Vertex {
  glm::vec3 pos;
  glm::vec3 color;
  glm::vec2 texCoord;

  bool operator==(const Vertex&other) const{
    return pos == other.pos
      && color == other.color
      && texCoord == other.texCoord;
  }

  static VkVertexInputBindingDescription getBindingDescription() {
    VkVertexInputBindingDescription bd{};

    bd.binding = 0;
    bd.stride = sizeof(Vertex);
    bd.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    return bd;
  }

  static std::array<VkVertexInputAttributeDescription, 3>
  getAttributeDescription() {
    std::array<VkVertexInputAttributeDescription, 3> ads{};
    ads[0].binding = 0;   // from which binding does vertex data come from?
    ads[0].location = 0;  // location directive in vertex shader
    ads[0].format = VK_FORMAT_R32G32B32_SFLOAT;  // bytesize of attribute data
    ads[0].offset = offsetof(Vertex, pos);    // # bytes from start of vertex

    ads[1].binding = 0;
    ads[1].location = 1;
    ads[1].format = VK_FORMAT_R32G32B32_SFLOAT;
    ads[1].offset = offsetof(Vertex, color);

    ads[2].binding = 0;
    ads[2].location = 2;
    ads[2].format = VK_FORMAT_R32G32_SFLOAT;
    ads[2].offset = offsetof(Vertex, texCoord);
    return ads;
  }
};

struct QueueFamilyIndices {
  std::optional<uint32_t> graphics_family;
  std::optional<uint32_t> present_family;

  bool isComplete() {
    return graphics_family.has_value() && present_family.has_value();
  }
};

// Contain details about the swap chain to see if it's suitable.
struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> present_modes;
};

class VulkanApp {
 public:
  void run();

 private:
  const int MAX_FRAMES_IN_FLIGHT = 2;
  GLFWwindow* window_;
  const uint32_t WIDTH = 800;
  const uint32_t HEIGHT = 600;

  const std::string MODEL_PATH = "models/viking_room.obj";
  const std::string TEXTURE_PATH = "textures/viking_room.png";
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
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};

  VkPhysicalDevice physical_device_ = VK_NULL_HANDLE;
  VkDevice logical_device_;
  VkQueue graphics_queue_;
  VkQueue present_queue_;

  VkSwapchainKHR swap_chain_;
  std::vector<VkImage> swapchain_images_;
  VkFormat swapchain_img_format_;
  VkExtent2D swapchain_img_extent_;

  std::vector<VkImageView> swapchain_imgviews_;

  // Commonly used to pass transformation matrices to vertex shader
  VkRenderPass render_pass_;
  VkDescriptorSetLayout descriptor_layout_;
  VkPipelineLayout pipeline_layout_;
  VkPipeline graphics_pipeline_;

  // One for each image in swap chain
  std::vector<VkFramebuffer> swapchain_frame_buffers_;

  VkCommandPool command_pool_;

  // One for each frame buffer
  std::vector<VkCommandBuffer> command_buffers_;

  std::vector<VkSemaphore> img_available_sems_;
  std::vector<VkSemaphore> render_finish_sems_;
  std::vector<VkFence> inflight_fences_;   // per frame
  std::vector<VkFence> images_in_flight_;  // per image in swap chain
  size_t current_frame_ = 0;

  bool frame_buffer_resized_ = false;

  std::vector<Vertex> vertices_;
  std::vector<uint32_t> indices_;

  VkBuffer vertex_buffer_;
  VkDeviceMemory vertex_buffer_memory_;

  VkBuffer index_buffer_;
  VkDeviceMemory index_buffer_memory_;

  std::vector<VkBuffer> uniform_buffers_;
  std::vector<VkDeviceMemory> uniform_buffers_memory_;

  VkDescriptorPool descriptor_pool_;
  std::vector<VkDescriptorSet> descriptor_sets_;

  uint32_t texture_miplevels_;
  VkImage texture_image_;
  VkDeviceMemory texture_image_memory_;

  VkImageView texture_img_view_;

  VkSampler texture_sampler_;

  VkImage depth_image_;
  VkImageView depth_image_view_;
  VkDeviceMemory depth_image_memory_;

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

  SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device);

  VkSurfaceFormatKHR chooseSwapSurfaceFormat(
      const std::vector<VkSurfaceFormatKHR>& available_formats);

  VkPresentModeKHR chooseSwapPresentMode(
      const std::vector<VkPresentModeKHR>& available_present_modes);

  // Resolution of the swap chain images
  VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities);

  void createSwapChain();
  /*
   * Recreate the swap chain correctly without quick hacks to get an initial
   * triangle demo.
   */
  void recreateSwapChain();

  // Views into images of the swap chain. These allow us to access parts
  // of the image like format, extent, color space, and drawing.
  void createImageViews();

  void createGraphicsPipeline();

  static std::vector<char> readFile(const std::string& filename);

  VkShaderModule createShaderModule(const std::vector<char>& shader_bytecode);

  /* Tell vulkan about framebuffer attachments, # of color and depth
   * buffers, # of samples to use for each buffer, how samples
   * are handled throughout rendering operations
   */
  void createRenderPass();

  /* Wraps image views in the swap chain for rendering.
   */
  void createFrameBuffers();

  /* Handles memory used to store command buffers. Command buffers
   * are allocated by the command pool.
   */
  void createCommandPool();

  /* Allocate command buffers from the command pool, one for each
   * frame buffer.
   */
  void createCommandBuffers();

  /*
   * Uses everything above to draw something on screen.
   * Acquire image from swapchain,
   * Execute command buffer with that image as attachment in frame buffer
   * return image to swap chain for presentation.
   *
   * These steps are called asynchronously by 1 function each. The function
   * may return before the command is completed. Subsequent commands require
   * preceeding ones to finish. Therefore we need synchronization. Use
   * semaphores.
   */
  void drawFrame();

  /* Initialize semaphores and fences for synchronizing drawing.
   * Semaphores for gpu-gpu synchronization
   * Fences for cpu-gpu sync
   */
  void createSyncObjects();

  /*
   * Before recreating the swap chain, clean up current resources like
   * swapchain images, frame buffers, etc.
   */
  void cleanUpSwapChain();

  /*
   * Set the frame buffer resized flag to true when the window changes size.
   * Static function because glfw doesn't know how to correctly use 'this'
   * when called from an instance of this class.
   */
  static void frameBufferResizeCallback(GLFWwindow* window, int width,
                                        int height);

  /*
   * Create vertex buffer, allocating memory for it and such.
   */
  void createVertexBuffer();

  /* Gpus have different memory types with different performance, allowed
   * operations. Find the memory type that is available and suits our needs
   */
  uint32_t findMemoryType(uint32_t type_filter,
                          VkMemoryPropertyFlags properties);

  /* Helper function to create buffers
   */
  void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage_flags,
                    VkMemoryPropertyFlags mem_prop_flags, VkBuffer& buffer,
                    VkDeviceMemory& buffer_memory);

  void copyBuffer(VkBuffer src_buff, VkBuffer dst_buff, VkDeviceSize size);

  /* Create the index buffer that indicates which vertices to use
   * from the vertex buffer and in what order.
   */
  void createIndexBuffer();

  /* Create details about descriptor layout used in shaders
   * Descriptor sets is how we pass global information to shaders
   */
  void createDescriptorSetLayout();

  /* Create uniform buffers, 1 for each swap chain image.
   */
  void createUniformBuffers();

  /* Update information in uniform buffers
  */
  void updateUniformBuffer(uint32_t uniform_buffer_idx);

  void createDescriptorPool();

  void createDescriptorSets();

  void createTextureImage();

  void createImage(uint32_t width, uint32_t height, VkFormat format,
  VkImageTiling tiling, VkImageUsageFlags usage,
  VkMemoryPropertyFlags properties, VkImage &image, VkDeviceMemory &image_mem,
    uint32_t miplevels);

  /* Start a command buffer
  */
  VkCommandBuffer beginSingleTimeCommands();
  /* End recording of a single time command buffer
  */
  void endSingleTimeCommands(VkCommandBuffer command_buffer);

  /* Handle image layout transitions
  */
  void transitionImageLayout(VkImage image, VkFormat format, 
    VkImageLayout old_layout, VkImageLayout new_layout, uint32_t miplevels);

  void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width,
    uint32_t height);

  /* To access images, we need image view.
  */
  void createTextureImageView();

  /* Helper function to create an image view
  */
  VkImageView createImageView(VkImage image, VkFormat format,
    VkImageAspectFlags aspect_mask, uint32_t miplevels);

  void createTextureSampler();

  /* Depth buffer
  */
  void createDepthResources();

  /* Find supported image formats
  * Format depends on tiling mode and usage.
  */
  VkFormat findSupportedImageFormat(
  const std::vector<VkFormat> &candidates, VkImageTiling tiling,
    VkFormatFeatureFlags features);

  /*Find format specifically for depth img*/
  VkFormat findDepthFormat();

  /* Indicates whether the chosen format has a stencil component
  */
  bool hasStencilComponent(VkFormat format);

  /* Load obj model with tinyobjloader
  */
  void loadModel();

  /* Generate mipmap using Blit (transfer operations and img mem barrier)
  * Requires linear filtering interpolation. GPU needs to support that format.
  */
  void generateMipmaps(VkImage image, VkFormat format, int32_t width,
    int32_t height, uint32_t miplevels, VkCommandBuffer cb=VK_NULL_HANDLE);
};

}  // namespace va
