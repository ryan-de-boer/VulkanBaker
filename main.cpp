#include <vulkan/vulkan.h>
#include <glm/glm.hpp>
#include <GLFW/glfw3.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <array>
#include <cstring>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include "/home/ryan-de-boer/renderdoc/renderdoc_1.43/include/renderdoc_app.h"
#include <dlfcn.h> // For dlsym

const uint32_t TEXTURE_WIDTH = 512;
const uint32_t TEXTURE_HEIGHT = 512;
//const uint32_t WINDOW_WIDTH = 800;
//const uint32_t WINDOW_HEIGHT = 600;
const uint32_t WINDOW_WIDTH = 1024*2;
const uint32_t WINDOW_HEIGHT = 768*2;

//0 = quad, 1==tri
                            int shapeType = 1;

RENDERDOC_API_1_7_0* rdoc_api = nullptr;

void initRenderDoc() {
    // Attempt to load the RenderDoc library
    std::cout << "initRenderDoc\n";
    void *mod = dlopen("/home/ryan-de-boer/renderdoc_src/renderdoc/build/lib/librenderdoc.so", RTLD_NOW);
    if (mod) {
        std::cout << "initRenderDoc mod\n";
        pRENDERDOC_GetAPI RENDERDOC_GetAPI = (pRENDERDOC_GetAPI)dlsym(mod, "RENDERDOC_GetAPI");
        int ret = RENDERDOC_GetAPI(eRENDERDOC_API_Version_1_1_2, (void **)&rdoc_api);
        if (ret != 1) rdoc_api = nullptr;
    }

    if (!rdoc_api)
    {
    std::cout << "RenderDoc is not loaded" << std::endl;
    }
else if (rdoc_api->IsRemoteAccessConnected()) {
    std::cout << "RenderDoc is connected and active!" << std::endl;
} else {
    std::cout << "RenderDoc is loaded, but NOT capturing/connected." << std::endl;
}

}

// In your main loop
void captureFrame() {
    if (rdoc_api) {
        rdoc_api->StartFrameCapture(NULL, NULL);
        // ... perform your draw calls ...
        rdoc_api->EndFrameCapture(NULL, NULL);
    }
}

bool g_capture = false;

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods);

class VulkanApp {
public:
    // Vulkan objects
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    VkDevice device;
    VkQueue queue;
    VkQueue presentQueue;
    uint32_t queueFamilyIndex;
    uint32_t presentQueueFamilyIndex;

    bool useBakedTextureHere = false;
    
    VkSurfaceKHR surface;
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    std::vector<VkFramebuffer> swapchainFramebuffers;
    
    VkRenderPass renderPass;
    VkPipeline graphicsPipeline;
    VkPipelineLayout graphicsPipelineLayout;
    
    // Compute objects
    VkImage computeOutputImage;
    VkDeviceMemory computeOutputImageMemory;
    VkImageView computeOutputImageView;
    VkImageView bakedImageView;
    VkDeviceMemory bakedTextureMemory;
    VkImage bakedTextureImage;
    
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorPool computeDescriptorPool;
    VkDescriptorSet computeDescriptorSet;
    VkDescriptorSet bakedDescriptorSet;

    VkPipelineLayout computePipelineLayout;
    VkPipeline computePipeline;
    
    // Graphics descriptor objects
    VkDescriptorSetLayout graphicsDescriptorSetLayout;
    VkDescriptorPool graphicsDescriptorPool;
    VkDescriptorSet graphicsDescriptorSet;
    VkSampler textureSampler;
    
    VkCommandPool commandPool;
    std::vector<VkCommandBuffer> commandBuffers;
    
    // Synchronization
    VkSemaphore imageAvailableSemaphore;
    VkSemaphore renderFinishedSemaphore;
    VkFence inFlightFence;
    
    GLFWwindow* window;
    bool framebufferResized = false;

    // Add this method to handle key presses
    void onKeyPress(int key, int scancode, int action, int mods) {
        // Only trigger on key press (not release)
        if (action == GLFW_PRESS) {
            switch(key) {
                case GLFW_KEY_ESCAPE:
                    std::cout << "hello" << std::endl;
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                    break;
                case GLFW_KEY_SPACE:
                    std::cout << "Space pressed - hello!" << std::endl;
                    break;
                case GLFW_KEY_S:
                    std::cout << "S pressed - Saving texture..." << std::endl;
                    saveTextureToPNG("output.png");
                    break;
                case GLFW_KEY_R:
                    std::cout << "R pressed - Running compute shader..." << std::endl;
                    // 1. Ensure the GPU is not busy with current frame operations
                    vkQueueWaitIdle(queue);         
                    // 2. Now it's safe to run the compute shader again
                    runComputeOnce();
                    std::cout << "Compute shader re-run successful." << std::endl;
                    break;
                case GLFW_KEY_T:
                    std::cout << "T pressed - toggling between baked and compute texture..." << std::endl;
                    useBakedTextureHere = !useBakedTextureHere;
                    if (useBakedTextureHere)
                        std::cout << "Using baked texture" << std::endl;
                    else
                        std::cout << "Using compute texture" << std::endl;
                    break;
                case GLFW_KEY_Q:
                    glfwSetWindowShouldClose(window, GLFW_TRUE);
                    break;
                case GLFW_KEY_0:
                g_capture = true;
                break;
                    
            // H - print help
            case GLFW_KEY_H:
                std::cout << "\n=== Keyboard Controls ===" << std::endl;
                std::cout << "SPACE - Print 'hello'" << std::endl;
                std::cout << "S     - Save texture to PNG" << std::endl;
                std::cout << "R     - Rerun compute shader" << std::endl;
                std::cout << "T     - Toggles baked texture" << std::endl;
                std::cout << "H     - Print this help" << std::endl;
                std::cout << "ESC   - Exit" << std::endl;
                std::cout << "=======================\n" << std::endl;
                break;
                    
                default:
                    std::cout << "Key " << key << " pressed" << std::endl;
                    break;
            }
        }
    else if (action == GLFW_REPEAT) {
        // Handle key repeat (when key is held down)
        // std::cout << "Key " << key << " repeating" << std::endl;
    }
    else if (action == GLFW_RELEASE) {
        // Handle key release
        // std::cout << "Key " << key << " released" << std::endl;
    }
    
    }

VkCommandBuffer beginSingleTimeCommands() {
if (device == VK_NULL_HANDLE) {
    throw std::runtime_error("CRITICAL: Device handle is VK_NULL_HANDLE!");
}
if (commandPool == VK_NULL_HANDLE) {
    throw std::runtime_error("CRITICAL: Command Pool is VK_NULL_HANDLE. Did you forget to call createCommandPool()?");
}

VkCommandBufferAllocateInfo allocInfo{};
allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
allocInfo.commandPool = commandPool;
allocInfo.commandBufferCount = 1;

VkCommandBuffer commandBuffer;
// Now it is safe to call
if (vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
}

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}

void endSingleTimeCommands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(queue); // Wait for the copy operation to finish before returning

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void copyBufferToImage(VkBuffer buffer, VkImage image, uint32_t width, uint32_t height) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands(); // Requires a single-time command buffer

    VkBufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = 0;
    region.bufferImageHeight = 0;
    region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = 1;
    region.imageOffset = {0, 0, 0};
    region.imageExtent = {width, height, 1};

    vkCmdCopyBufferToImage(commandBuffer, buffer, image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    endSingleTimeCommands(commandBuffer);
}

void createBuffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = size;
    bufferInfo.usage = usage;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE; // Standard for single-queue apps

    if (vkCreateBuffer(device, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
        throw std::runtime_error("failed to create buffer!");
    }

    // Query requirements to know how much memory to allocate
    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    
    // This is the crucial part: finding the right memory heap on your GPU
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &bufferMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate buffer memory!");
    }

    vkBindBufferMemory(device, buffer, bufferMemory, 0);
}

void createImage(uint32_t width, uint32_t height, VkFormat format, VkImageTiling tiling, 
                            VkImageUsageFlags usage, VkMemoryPropertyFlags properties, 
                            VkImage& image, VkDeviceMemory& imageMemory) {
    
    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.extent.width = width;
    imageInfo.extent.height = height;
    imageInfo.extent.depth = 1;
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.format = format;
    imageInfo.tiling = tiling;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageInfo.usage = usage;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.flags = 0; // Optional flags for things like sparse images

    if (vkCreateImage(device, &imageInfo, nullptr, &image) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image!");
    }

    // Allocate memory just like a buffer
    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(device, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, properties);

    if (vkAllocateMemory(device, &allocInfo, nullptr, &imageMemory) != VK_SUCCESS) {
        throw std::runtime_error("failed to allocate image memory!");
    }

    vkBindImageMemory(device, image, imageMemory, 0);
}

void transitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;

    // Determine the access masks and pipeline stages based on the transition
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

        sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
        destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } 
    else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
        destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } 
    else {
        throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, nullptr,
        0, nullptr,
        1, &barrier
    );

    endSingleTimeCommands(commandBuffer);
}

void createImageView(VkImage image, VkFormat format, VkImageAspectFlags aspectFlags, VkImageView& imageView) {
    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = format;

    // The components field allows for swizzling (e.g., mapping R to G), 
    // but default identity mapping is usually what you want.
    viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

    // SubresourceRange defines what part of the image the view looks at.
    // For a standard texture, we look at level 0, 1 mipmap, layer 0, 1 array layer.
    viewInfo.subresourceRange.aspectMask = aspectFlags;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(device, &viewInfo, nullptr, &imageView) != VK_SUCCESS) {
        throw std::runtime_error("failed to create image view!");
    }
}

void createTextureImage() {
    int texWidth, texHeight, texChannels;
    // Load image from disk
    stbi_uc* pixels = stbi_load("baked.png", &texWidth, &texHeight, &texChannels, STBI_rgb_alpha);
    VkDeviceSize imageSize = texWidth * texHeight * 4;

    if (!pixels) {
        throw std::runtime_error("failed to load texture image!");
    }

    // 1. Create Staging Buffer (CPU visible)
    VkBuffer stagingBuffer;
    VkDeviceMemory stagingBufferMemory;
    createBuffer(imageSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, 
                 VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, 
                 stagingBuffer, stagingBufferMemory);

    // 2. Map pixels to the buffer
    void* data;
    vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
    memcpy(data, pixels, static_cast<size_t>(imageSize));
    vkUnmapMemory(device, stagingBufferMemory);
    stbi_image_free(pixels); // We're done with CPU memory now

    // 3. Create the GPU-local Image
    createImage(texWidth, texHeight, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_TILING_OPTIMAL, 
                VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, 
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, bakedTextureImage, bakedTextureMemory);

    // 4. Copy Buffer to Image (Requires command buffer)
    transitionImageLayout(bakedTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    copyBufferToImage(stagingBuffer, bakedTextureImage, static_cast<uint32_t>(texWidth), static_cast<uint32_t>(texHeight));
    
    // 5. Prepare for Shader Reading
    transitionImageLayout(bakedTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

    // Cleanup staging buffer
    vkDestroyBuffer(device, stagingBuffer, nullptr);
    vkFreeMemory(device, stagingBufferMemory, nullptr);
    
    // Create the View
    createImageView(bakedTextureImage, VK_FORMAT_R8G8B8A8_SRGB, VK_IMAGE_ASPECT_COLOR_BIT, bakedImageView);
}

// You need a helper to create the set for the baked texture
void createBakedDescriptorSet() {
    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = graphicsDescriptorPool; // Or your compute pool
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &graphicsDescriptorSetLayout;

VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &bakedDescriptorSet);

if (result != VK_SUCCESS) {
    // This will print the specific error (e.g., VK_ERROR_OUT_OF_POOL_MEMORY)
    std::cerr << "Failed to allocate descriptor set! Result code: " << result << std::endl;
    throw std::runtime_error("Descriptor set allocation failed.");
}

    VkDescriptorImageInfo imageInfo{};
    imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    imageInfo.imageView = bakedImageView; // The view created from your PNG
    imageInfo.sampler = textureSampler;

    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = bakedDescriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.pImageInfo = &imageInfo;

    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
}

    bool initialize() {

//                initRenderDoc();


        std::cout << "initWindow\n";
        if (!initWindow()) return false;
        std::cout << "createInstance\n";
        if (!createInstance()) return false;
        std::cout << "createSurface\n";
        if (!createSurface()) return false;

        std::cout << "pickPhysicalDevice\n";
        if (!pickPhysicalDevice()) return false;
        std::cout << "createDevice\n";
        if (!createDevice()) return false;
        std::cout << "createSwapchain\n";
        if (!createSwapchain()) return false;
        std::cout << "createImageViews\n";
        if (!createImageViews()) return false;
        std::cout << "createComputeImage\n";
        if (!createComputeImage()) return false;
        std::cout << "createRenderPass\n";

        if (!createRenderPass()) return false;
        std::cout << "createFramebuffers\n";
        if (!createFramebuffers()) return false;
        std::cout << "createComputeDescriptorLayout\n";
        if (!createComputeDescriptorLayout()) return false;
        std::cout << "createComputeDescriptorPool\n";
        if (!createComputeDescriptorPool()) return false;
        std::cout << "createComputeDescriptorSet\n";
        if (!createComputeDescriptorSet()) return false;
        std::cout << "createComputePipeline\n";
        if (!createComputePipeline()) return false;
        std::cout << "createGraphicsDescriptorLayout\n";
        if (!createGraphicsDescriptorLayout()) return false;
        std::cout << "createGraphicsDescriptorPool\n";
        if (!createGraphicsDescriptorPool()) return false;
        std::cout << "createTextureSampler\n";
        if (!createTextureSampler()) return false;
        std::cout << "createGraphicsDescriptorSet\n";
        if (!createGraphicsDescriptorSet()) return false;
        std::cout << "createGraphicsPipeline\n";
        if (!createGraphicsPipeline()) return false;
        std::cout << "createCommandPool\n";
        if (!createCommandPool()) return false;

        std::cout << "createTextureImage\n";
        createTextureImage();
        std::cout << "createBakedDescriptorSet\n";
        createBakedDescriptorSet();

        std::cout << "createSynchronizationObjects\n";
        if (!createSynchronizationObjects()) return false;
        std::cout << "end init\n";

        return true;
    }

    bool initWindow() {

//        glfwDefaultWindowHints();
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11); 

        if (!glfwInit()) {
            std::cerr << "Failed to initialize GLFW" << std::endl;
            return false;
        }

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);





        window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Vulkan Compute & Graphics", nullptr, nullptr);
        if (!window) {
            std::cerr << "Failed to create GLFW window" << std::endl;
            return false;
        }
//        if (window) {
//    glfwSetWindowSize(window, WINDOW_WIDTH, WINDOW_HEIGHT);
//}

        glfwSetWindowUserPointer(window, this);
        glfwSetFramebufferSizeCallback(window, framebufferResizeCallback);
        glfwSetKeyCallback(window, keyCallback);
        
        return true;
    }

    static void framebufferResizeCallback(GLFWwindow* window, int width, int height) {
        auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
        app->framebufferResized = true;
    }

    // bool createInstance() {
    //     VkApplicationInfo appInfo{};
    //     appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    //     appInfo.pApplicationName = "Compute + Graphics";
    //     appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    //     appInfo.apiVersion = VK_API_VERSION_1_2;

    //     uint32_t glfwExtensionCount = 0;
    //     const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    //     std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);
    //     extensions.push_back(VK_KHR_SURFACE_EXTENSION_NAME);

    //     VkInstanceCreateInfo createInfo{};
    //     createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    //     createInfo.pApplicationInfo = &appInfo;
    //     createInfo.enabledExtensionCount = extensions.size();
    //     createInfo.ppEnabledExtensionNames = extensions.data();

    //     return vkCreateInstance(&createInfo, nullptr, &instance) == VK_SUCCESS;
    // }

bool createInstance() {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.apiVersion = VK_API_VERSION_1_2;

    uint32_t glfwExtensionCount = 0;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

    
// If the list is empty or returns an error, don't just proceed!
if (glfwExtensionCount == 0) {
    // 1. Log a warning
    // 2. FORCE a fallback:
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // Force X11 if you are on Linux to bypass Wayland/RenderDoc conflicts
//    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11); 
//    glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_WAYLAND); 
    
    // Now re-fetch
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
}


    std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);


    // DYNAMIC LAYER HANDLING: 
    // Do not hardcode layers. Only enable if they are actually supported.
    std::vector<const char*> layers;
    
    // Check if validation is actually available on this system before requesting it
    uint32_t layerCount;
    vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
    std::vector<VkLayerProperties> availableLayers(layerCount);
    vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

    bool validationSupported = false;
    for (const auto& layer : availableLayers) {
        if (strcmp(layer.layerName, "VK_LAYER_KHRONOS_validation") == 0) {
            validationSupported = true;
            break;
        }
    }

    // Only request layers if they exist AND we are NOT in RenderDoc (or keep them off for dev)
    // Note: RenderDoc often injects its own layer, so we check for that too
    if (validationSupported && !getenv("ENABLE_VULKAN_RENDERDOC_CAPTURE")) {
        layers.push_back("VK_LAYER_KHRONOS_validation");
    }

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;
    createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
    createInfo.ppEnabledExtensionNames = extensions.data();
    createInfo.enabledLayerCount = static_cast<uint32_t>(layers.size());
    createInfo.ppEnabledLayerNames = layers.data();

    VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
    
    // If it fails, log the exact result code to a file so you can read it later
    if (result != VK_SUCCESS) {
        std::ofstream log("error.log");
        log << "Instance creation failed with code: " << result << std::endl;
        return false;
    }
    return true;
}

// bool createInstance() {
//     VkApplicationInfo appInfo{};
//     appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
//     appInfo.apiVersion = VK_API_VERSION_1_2;

//     // Hardcode your Linux WSI extensions instead of querying GLFW
//     // If you are on Wayland, use VK_KHR_wayland_surface
//     // If you are on X11, use VK_KHR_xcb_surface or VK_KHR_xlib_surface
//     std::vector<const char*> extensions = {
//         VK_KHR_SURFACE_EXTENSION_NAME,
//         "VK_KHR_xcb_surface" // or "VK_KHR_wayland_surface" if on Wayland
//     };

//     VkInstanceCreateInfo createInfo{};
//     createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
//     createInfo.pApplicationInfo = &appInfo;
//     createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
//     createInfo.ppEnabledExtensionNames = extensions.data();
    
//     // DISABLE validation layers to avoid the -7 conflict
//     createInfo.enabledLayerCount = 0; 

//     VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
//     return (result == VK_SUCCESS);
// }

    bool createSurface() {

        VkResult r = glfwCreateWindowSurface(instance, window, nullptr, &surface);

        if (r==VK_SUCCESS)
        {
        std::ofstream log("error2.log");
        log << "VK_SUCCESS" << std::endl;

        }
        else
        {
        std::ofstream log("error2.log");
        log << "FAILED: " << r << std::endl;

        }

        return r == VK_SUCCESS;
    }

    bool pickPhysicalDevice() {
        uint32_t deviceCount = 0;
        vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
        
        if (deviceCount == 0) {
            std::cerr << "No GPU found" << std::endl;
            return false;
        }

        std::vector<VkPhysicalDevice> devices(deviceCount);
        vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
        physicalDevice = devices[0];

        // Find queue families
        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());

        for (uint32_t i = 0; i < queueFamilyCount; i++) {            
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &presentSupport);

            if ((queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) && (queueFamilies[i].queueFlags & VK_QUEUE_COMPUTE_BIT) && presentSupport) {
                queueFamilyIndex = i;
                presentQueueFamilyIndex = i;
                break;
            }
        }
        return true;
    }

    bool createDevice() {

        std::cout << "Compute Queue: " << queueFamilyIndex << " | Present Queue: " << presentQueueFamilyIndex << std::endl;

        std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
        std::array<float, 1> queuePriority = {1.0f};

        // Compute queue
        VkDeviceQueueCreateInfo computeQueueInfo{};
        computeQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        computeQueueInfo.queueFamilyIndex = queueFamilyIndex;
        computeQueueInfo.queueCount = 1;
        computeQueueInfo.pQueuePriorities = queuePriority.data();
        queueCreateInfos.push_back(computeQueueInfo);

        // Present queue (if different)
        if (presentQueueFamilyIndex != queueFamilyIndex) {
            VkDeviceQueueCreateInfo presentQueueInfo{};
            presentQueueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            presentQueueInfo.queueFamilyIndex = presentQueueFamilyIndex;
            presentQueueInfo.queueCount = 1;
            presentQueueInfo.pQueuePriorities = queuePriority.data();
            queueCreateInfos.push_back(presentQueueInfo);
        }

        std::vector<const char*> deviceExtensions = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
        };

        VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = queueCreateInfos.size();
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.enabledExtensionCount = deviceExtensions.size();
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();

        if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
            std::cerr << "Failed to create device" << std::endl;
            return false;
        }

        vkGetDeviceQueue(device, queueFamilyIndex, 0, &queue);
        vkGetDeviceQueue(device, presentQueueFamilyIndex, 0, &presentQueue);
        return true;
    }

    bool createSwapchain() {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto& format : formats) {
            if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
                surfaceFormat = format;
                break;
            }
        }

        VkExtent2D extent = capabilities.currentExtent;
        if (extent.width == UINT32_MAX) {
            extent = {WINDOW_WIDTH, WINDOW_HEIGHT};
        }

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
            imageCount = capabilities.maxImageCount;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

        if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) != VK_SUCCESS) {
            std::cerr << "Failed to create swapchain" << std::endl;
            return false;
        }

        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
        swapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapchainImages.data());

        return true;
    }

    bool createImageViews() {
        swapchainImageViews.resize(swapchainImages.size());

        for (size_t i = 0; i < swapchainImages.size(); i++) {
            VkImageViewCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            createInfo.image = swapchainImages[i];
            createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            createInfo.format = VK_FORMAT_B8G8R8A8_SRGB;
            createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            createInfo.subresourceRange.baseMipLevel = 0;
            createInfo.subresourceRange.levelCount = 1;
            createInfo.subresourceRange.baseArrayLayer = 0;
            createInfo.subresourceRange.layerCount = 1;

            if (vkCreateImageView(device, &createInfo, nullptr, &swapchainImageViews[i]) != VK_SUCCESS) {
                std::cerr << "Failed to create image view" << std::endl;
                return false;
            }
        }
        return true;
    }

    bool createComputeImage() {
        VkImageCreateInfo imageInfo{};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = VK_IMAGE_TYPE_2D;
        imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        imageInfo.extent = {TEXTURE_WIDTH, TEXTURE_HEIGHT, 1};
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
        imageInfo.usage = VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

        if (vkCreateImage(device, &imageInfo, nullptr, &computeOutputImage) != VK_SUCCESS) {
            std::cerr << "Failed to create compute image" << std::endl;
            return false;
        }

        VkMemoryRequirements memRequirements;
        vkGetImageMemoryRequirements(device, computeOutputImage, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        if (vkAllocateMemory(device, &allocInfo, nullptr, &computeOutputImageMemory) != VK_SUCCESS) {
            std::cerr << "Failed to allocate image memory" << std::endl;
            return false;
        }

        vkBindImageMemory(device, computeOutputImage, computeOutputImageMemory, 0);

        // Create image view
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = computeOutputImage;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        return vkCreateImageView(device, &viewInfo, nullptr, &computeOutputImageView) == VK_SUCCESS;
    }

    bool createRenderPass() {
        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = VK_FORMAT_B8G8R8A8_SRGB;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;

        return vkCreateRenderPass(device, &renderPassInfo, nullptr, &renderPass) == VK_SUCCESS;
    }

    bool createFramebuffers() {
        swapchainFramebuffers.resize(swapchainImageViews.size());

        for (size_t i = 0; i < swapchainImageViews.size(); i++) {
            VkImageView attachments[] = {swapchainImageViews[i]};

            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = WINDOW_WIDTH;
            framebufferInfo.height = WINDOW_HEIGHT;
            framebufferInfo.layers = 1;

            if (vkCreateFramebuffer(device, &framebufferInfo, nullptr, &swapchainFramebuffers[i]) != VK_SUCCESS) {
                std::cerr << "Failed to create framebuffer" << std::endl;
                return false;
            }
        }
        return true;
    }

    bool createComputeDescriptorLayout() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        return vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &computeDescriptorSetLayout) == VK_SUCCESS;
    }

    bool createComputeDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 1;

        return vkCreateDescriptorPool(device, &poolInfo, nullptr, &computeDescriptorPool) == VK_SUCCESS;
    }

    bool createComputeDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = computeDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &computeDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &computeDescriptorSet) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageView = computeOutputImageView;
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = computeDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        return true;
    }

    bool createGraphicsDescriptorLayout() {
        VkDescriptorSetLayoutBinding binding{};
        binding.binding = 0;
        binding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        binding.descriptorCount = 1;
        binding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

        VkDescriptorSetLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.bindingCount = 1;
        layoutInfo.pBindings = &binding;

        return vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr, &graphicsDescriptorSetLayout) == VK_SUCCESS;
    }

    bool createGraphicsDescriptorPool() {
        VkDescriptorPoolSize poolSize{};
        poolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        poolSize.descriptorCount = 1;

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = 1;
        poolInfo.pPoolSizes = &poolSize;
        poolInfo.maxSets = 2;

        return vkCreateDescriptorPool(device, &poolInfo, nullptr, &graphicsDescriptorPool) == VK_SUCCESS;
    }

    bool createTextureSampler() {
        VkSamplerCreateInfo samplerInfo{};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        samplerInfo.maxAnisotropy = 1.0f;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;

        return vkCreateSampler(device, &samplerInfo, nullptr, &textureSampler) == VK_SUCCESS;
    }

    bool createGraphicsDescriptorSet() {
        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = graphicsDescriptorPool;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &graphicsDescriptorSetLayout;

        if (vkAllocateDescriptorSets(device, &allocInfo, &graphicsDescriptorSet) != VK_SUCCESS) {
            return false;
        }

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        imageInfo.imageView = computeOutputImageView;
        imageInfo.sampler = textureSampler;

        VkWriteDescriptorSet descriptorWrite{};
        descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        descriptorWrite.dstSet = graphicsDescriptorSet;
        descriptorWrite.dstBinding = 0;
        descriptorWrite.dstArrayElement = 0;
        descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        descriptorWrite.descriptorCount = 1;
        descriptorWrite.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);
        return true;
    }

    std::vector<char> readShaderFile(const std::string& filename) {
        std::ifstream file(filename, std::ios::ate | std::ios::binary);
        if (!file.is_open()) {
            std::cerr << "Failed to open shader file: " << filename << std::endl;
            return {};
        }

        size_t fileSize = (size_t)file.tellg();
        std::vector<char> buffer(fileSize);
        file.seekg(0);
        file.read(buffer.data(), fileSize);
        file.close();
        return buffer;
    }

    bool createComputePipeline() {
        auto shaderCode = readShaderFile("compute_shader.spv");
        if (shaderCode.empty()) return false;

        VkShaderModuleCreateInfo moduleInfo{};
        moduleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        moduleInfo.codeSize = shaderCode.size();
        moduleInfo.pCode = reinterpret_cast<const uint32_t*>(shaderCode.data());

        VkShaderModule shaderModule;
        if (vkCreateShaderModule(device, &moduleInfo, nullptr, &shaderModule) != VK_SUCCESS) {
            std::cerr << "Failed to create compute shader module" << std::endl;
            return false;
        }

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = shaderModule;
        stageInfo.pName = "main";

        VkPipelineLayoutCreateInfo layoutInfo{};
        layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &computeDescriptorSetLayout;

        if (vkCreatePipelineLayout(device, &layoutInfo, nullptr, &computePipelineLayout) != VK_SUCCESS) {
            std::cerr << "Failed to create compute pipeline layout" << std::endl;
            return false;
        }

        VkComputePipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.layout = computePipelineLayout;
        pipelineInfo.stage = stageInfo;

        if (vkCreateComputePipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &computePipeline) != VK_SUCCESS) {
            std::cerr << "Failed to create compute pipeline" << std::endl;
            return false;
        }

        vkDestroyShaderModule(device, shaderModule, nullptr);
        return true;
    }

    bool createGraphicsPipeline() {
        auto vertShaderCode = readShaderFile("vertex_shader.spv");
        auto fragShaderCode = readShaderFile("fragment_shader.spv");

        if (vertShaderCode.empty() || fragShaderCode.empty()) return false;

        VkShaderModuleCreateInfo vertModuleInfo{};
        vertModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        vertModuleInfo.codeSize = vertShaderCode.size();
        vertModuleInfo.pCode = reinterpret_cast<const uint32_t*>(vertShaderCode.data());

        VkShaderModule vertShaderModule;
        if (vkCreateShaderModule(device, &vertModuleInfo, nullptr, &vertShaderModule) != VK_SUCCESS) {
            return false;
        }

        VkShaderModuleCreateInfo fragModuleInfo{};
        fragModuleInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
        fragModuleInfo.codeSize = fragShaderCode.size();
        fragModuleInfo.pCode = reinterpret_cast<const uint32_t*>(fragShaderCode.data());

        VkShaderModule fragShaderModule;
        if (vkCreateShaderModule(device, &fragModuleInfo, nullptr, &fragShaderModule) != VK_SUCCESS) {
            return false;
        }

        VkPipelineShaderStageCreateInfo shaderStages[2]{};
        shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
        shaderStages[0].module = vertShaderModule;
        shaderStages[0].pName = "main";

        shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        shaderStages[1].module = fragShaderModule;
        shaderStages[1].pName = "main";

        VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.vertexAttributeDescriptionCount = 0;

        VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

        VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float)WINDOW_WIDTH;
        viewport.height = (float)WINDOW_HEIGHT;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

        VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};

        VkPipelineViewportStateCreateInfo viewportState{};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.pViewports = &viewport;
        viewportState.scissorCount = 1;
        viewportState.pScissors = &scissor;

        VkPipelineRasterizationStateCreateInfo rasterizer{};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.cullMode = VK_CULL_MODE_BACK_BIT;
        rasterizer.frontFace = VK_FRONT_FACE_CLOCKWISE;
        rasterizer.depthBiasEnable = VK_FALSE;

        VkPipelineMultisampleStateCreateInfo multisampling{};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

        VkPipelineColorBlendAttachmentState colorBlendAttachment{};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                              VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;

        VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;

        VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &graphicsDescriptorSetLayout;

        if (vkCreatePipelineLayout(device, &pipelineLayoutInfo, nullptr, &graphicsPipelineLayout) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline layout" << std::endl;
            return false;
        }

        VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.layout = graphicsPipelineLayout;
        pipelineInfo.renderPass = renderPass;
        pipelineInfo.subpass = 0;

        if (vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS) {
            std::cerr << "Failed to create graphics pipeline" << std::endl;
            return false;
        }

        vkDestroyShaderModule(device, fragShaderModule, nullptr);
        vkDestroyShaderModule(device, vertShaderModule, nullptr);
        return true;
    }

    bool createCommandPool() {
        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.queueFamilyIndex = queueFamilyIndex;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

        return vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool) == VK_SUCCESS;
    }

    bool createSynchronizationObjects() {
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        return vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailableSemaphore) == VK_SUCCESS &&
               vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinishedSemaphore) == VK_SUCCESS &&
               vkCreateFence(device, &fenceInfo, nullptr, &inFlightFence) == VK_SUCCESS;
    }

    void recordComputeCommandBuffer(VkCommandBuffer commandBuffer) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Transition image layout to GENERAL for compute write
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = computeOutputImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = 0;
        barrier.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
                           VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &barrier);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE,
                               computePipelineLayout, 0, 1, &computeDescriptorSet, 0, nullptr);
        vkCmdDispatch(commandBuffer, (TEXTURE_WIDTH + 7) / 8, (TEXTURE_HEIGHT + 7) / 8, 1);

        // Transition image for reading
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                           VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0, 0, nullptr,
                           0, nullptr, 1, &barrier);

        vkEndCommandBuffer(commandBuffer);
    }

    void recordGraphicsCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex) {
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        VkClearValue clearColor = {{0.0f, 0.0f, 0.0f, 1.0f}};

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass;
        renderPassInfo.framebuffer = swapchainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {WINDOW_WIDTH, WINDOW_HEIGHT};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

                bool useBakedTexture2 = useBakedTextureHere;
        VkDescriptorSet activeSet = useBakedTexture2 ? bakedDescriptorSet : graphicsDescriptorSet;
        int flipV = 0;
//        if (!useBakedTexture2)
            flipV = 1;


        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                               graphicsPipelineLayout, 0, 1, &activeSet, 0, nullptr);

   
                               uint32_t vertexCount = (shapeType == 0) ? 6 : 3;
                               
        vkCmdPushConstants(commandBuffer, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(int), &shapeType);
        vkCmdPushConstants(commandBuffer, graphicsPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 4, sizeof(int), &flipV);
        vkCmdDraw(commandBuffer, vertexCount, 1, 0, 0);

        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);
    }

    void runComputeOnce() {
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer computeCommandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &computeCommandBuffer);

        recordComputeCommandBuffer(computeCommandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &computeCommandBuffer;

//         // After your compute shader is done
// VkImageBlit blit{};
// blit.srcOffsets[0] = { 0, 0, 0 };
// blit.srcOffsets[1] = { TEXTURE_WIDTH, TEXTURE_HEIGHT, 1 };
// blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
// blit.srcSubresource.layerCount = 1;

// // The trick: Destination Y is flipped (Height -> 0)
// blit.dstOffsets[0] = { 0, TEXTURE_HEIGHT, 0 }; 
// blit.dstOffsets[1] = { TEXTURE_WIDTH, 0, 1 }; 
// blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
// blit.dstSubresource.layerCount = 1;

// vkCmdBlitImage(computeCommandBuffer, 
//     computeOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
//     bakedTextureImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
//     1, &blit, VK_FILTER_LINEAR);


        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &computeCommandBuffer);



    }

    void drawFrame() {

if (g_capture)
{
        if (rdoc_api) {
        rdoc_api->StartFrameCapture(NULL, NULL);
        }
}

//                    std::cout << "drawFrame start..." << std::endl;
        vkWaitForFences(device, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(device, 1, &inFlightFence);

        uint32_t imageIndex;
        vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);

        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

        recordGraphicsCommandBuffer(commandBuffer, imageIndex);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &imageAvailableSemaphore;
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore;

        vkQueueSubmit(queue, 1, &submitInfo, inFlightFence);

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &renderFinishedSemaphore;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain;
        presentInfo.pImageIndices = &imageIndex;

        vkQueuePresentKHR(presentQueue, &presentInfo);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
//                    std::cout << "drawFrame end..." << std::endl;

if (g_capture)
{
        if (rdoc_api) {
        rdoc_api->EndFrameCapture(NULL, NULL);
        }
        g_capture = false;
}

    }

    void flipBufferVertically(unsigned char* data, int width, int height, int channels) {
    int stride = width * channels;
    std::vector<unsigned char> row(stride);
    unsigned char* top = data;
    unsigned char* bottom = data + (height - 1) * stride;

    for (int i = 0; i < height / 2; ++i) {
        memcpy(row.data(), top, stride);
        memcpy(top, bottom, stride);
        memcpy(bottom, row.data(), stride);

        top += stride;
        bottom -= stride;
    }
}

    void saveTextureToPNG(const std::string& filename) {
        // Create staging buffer
        VkDeviceSize imageSize = TEXTURE_WIDTH * TEXTURE_HEIGHT * 4;

        VkBufferCreateInfo stagingBufferInfo{};
        stagingBufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        stagingBufferInfo.size = imageSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT;
        stagingBufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

        VkBuffer stagingBuffer;
        VkDeviceMemory stagingBufferMemory;
        vkCreateBuffer(device, &stagingBufferInfo, nullptr, &stagingBuffer);

        VkMemoryRequirements memRequirements;
        vkGetBufferMemoryRequirements(device, stagingBuffer, &memRequirements);

        VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits,
                                                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

        vkAllocateMemory(device, &allocInfo, nullptr, &stagingBufferMemory);
        vkBindBufferMemory(device, stagingBuffer, stagingBufferMemory, 0);

        // Copy from image to staging buffer
        VkCommandBufferAllocateInfo cmdAllocInfo{};
        cmdAllocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        cmdAllocInfo.commandPool = commandPool;
        cmdAllocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        cmdAllocInfo.commandBufferCount = 1;

        VkCommandBuffer commandBuffer;
        vkAllocateCommandBuffers(device, &cmdAllocInfo, &commandBuffer);

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        vkBeginCommandBuffer(commandBuffer, &beginInfo);

        // Transition image layout
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = computeOutputImage;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_SHADER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
                           VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy region{};
        region.bufferOffset = 0;
//        region.bufferRowPitch = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;
        region.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = {0, 0, 0};
        region.imageExtent = {TEXTURE_WIDTH, TEXTURE_HEIGHT, 1};

        vkCmdCopyImageToBuffer(commandBuffer, computeOutputImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                              stagingBuffer, 1, &region);

        vkEndCommandBuffer(commandBuffer);

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;

        vkQueueSubmit(queue, 1, &submitInfo, VK_NULL_HANDLE);
        vkQueueWaitIdle(queue);

        vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);

        // Read data from staging buffer
        void* data;
        vkMapMemory(device, stagingBufferMemory, 0, imageSize, 0, &data);
        uint8_t* pixels = (uint8_t*)data;

// 1. Download from GPU to CPU buffer (as you do now)
// 2. Flip the buffer (as you do now)
//flipBufferVertically(pixels, TEXTURE_WIDTH, TEXTURE_HEIGHT, 4);

// 3. Apply Gamma Correction (Linear -> sRGB) on the CPU
for (int i = 0; i < imageSize; i += 4) {
    // Convert 0-255 back to 0.0-1.0 float range
    float r = pow(pixels[i + 0] / 255.0f, 1.0f / 2.2f);
    float g = pow(pixels[i + 1] / 255.0f, 1.0f / 2.2f);
    float b = pow(pixels[i + 2] / 255.0f, 1.0f / 2.2f);
    
    // Store back as 0-255
    pixels[i + 0] = static_cast<unsigned char>(r * 255.0f);
    pixels[i + 1] = static_cast<unsigned char>(g * 255.0f);
    pixels[i + 2] = static_cast<unsigned char>(b * 255.0f);
}

        // Write PNG
        stbi_flip_vertically_on_write(false);
        stbi_write_png(filename.c_str(), TEXTURE_WIDTH, TEXTURE_HEIGHT, 4, pixels, TEXTURE_WIDTH * 4);
        std::cout << "Saved texture to " << filename << std::endl;

        vkUnmapMemory(device, stagingBufferMemory);
        vkFreeMemory(device, stagingBufferMemory, nullptr);
        vkDestroyBuffer(device, stagingBuffer, nullptr);
    }

    void mainLoop() {
        // Run compute shader once
        std::cout << "Running compute shader..." << std::endl;
        runComputeOnce();

        // Save to PNG
        saveTextureToPNG("output.png");

        // Display in window
        while (!glfwWindowShouldClose(window)) {
            //std::cout << "Loop running..." << std::endl;
            glfwPollEvents();
            drawFrame();
        }

        vkDeviceWaitIdle(device);
    }

    uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
        VkPhysicalDeviceMemoryProperties memProperties;
        vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);

        for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
            if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties)) {
                return i;
            }
        }
        return 0;
    }

    void cleanup() {
        vkDestroySampler(device, textureSampler, nullptr);
        vkDestroyDescriptorPool(device, graphicsDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, graphicsDescriptorSetLayout, nullptr);
        vkDestroyPipeline(device, graphicsPipeline, nullptr);
        vkDestroyPipelineLayout(device, graphicsPipelineLayout, nullptr);
        vkDestroyRenderPass(device, renderPass, nullptr);

        for (auto framebuffer : swapchainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, nullptr);
        }

        for (auto imageView : swapchainImageViews) {
            vkDestroyImageView(device, imageView, nullptr);
        }

        vkDestroyImageView(device, computeOutputImageView, nullptr);
        vkFreeMemory(device, computeOutputImageMemory, nullptr);
        vkDestroyImage(device, computeOutputImage, nullptr);

        vkDestroyImageView(device, bakedImageView, nullptr);
        vkDestroyImage(device, bakedTextureImage, nullptr);
        vkFreeMemory(device, bakedTextureMemory, nullptr);

        vkDestroyPipeline(device, computePipeline, nullptr);
        vkDestroyPipelineLayout(device, computePipelineLayout, nullptr);
        vkDestroyDescriptorPool(device, computeDescriptorPool, nullptr);
        vkDestroyDescriptorSetLayout(device, computeDescriptorSetLayout, nullptr);

        vkDestroySwapchainKHR(device, swapchain, nullptr);
        vkDestroyCommandPool(device, commandPool, nullptr);

        vkDestroySemaphore(device, imageAvailableSemaphore, nullptr);
        vkDestroySemaphore(device, renderFinishedSemaphore, nullptr);
        vkDestroyFence(device, inFlightFence, nullptr);

        vkDestroyDevice(device, nullptr);
        vkDestroySurfaceKHR(instance, surface, nullptr);
        vkDestroyInstance(instance, nullptr);

        glfwDestroyWindow(window);
        glfwTerminate();
    }
};

static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods) {
    auto app = reinterpret_cast<VulkanApp*>(glfwGetWindowUserPointer(window));
    app->onKeyPress(key, scancode, action, mods);
}

int main() {

//    std::cout << "Waiting for debugger/RenderDoc... Press Enter to continue." << std::endl;
//    std::cin.get();

    VulkanApp app;

    if (!app.initialize()) {
        std::cerr << "Initialization failed!" << std::endl;
        return 1;
    }

    app.mainLoop();
    app.cleanup();

    return 0;
}