#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <vulkan/vulkan.h>
#include <stdexcept>
#include <vector>
#include <set>
#include <algorithm>
#include <string>
#include <cstring>
#include <limits>
#include <fstream>

#include <iostream>
#include <sstream>
#include <cstdlib> //for exit

#ifdef _WIN32

int main();
int WinMain()
{
    return main();
}

#endif // _WIN32 

static void exitWithError(const char* error, int code = 0) 
{
    std::ostringstream errText;
    errText << error;
    if (code != 0)
        errText << "; error code: " << code;

    std::cout << "\nFatal error: " << errText.str() << "\n";

    exit(code);
    throw std::runtime_error(errText.str());
}

static std::vector<char> readFile(const std::string& filename);
static VkShaderModule createShader(const VkDevice& device, const std::string& filePath);

GLFWwindow* initGLFW() {
    if (!glfwInit()) {
        return nullptr;
    }

    // Optional: prevent OpenGL context if using Vulkan later
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    GLFWwindow* window = glfwCreateWindow(800, 600, "Empty GLFW Window", nullptr, nullptr);
    if (!window) {
        glfwTerminate();
        return nullptr;
    }
    return window;
}



class PropList
{
    std::vector<const char*> _props;
public:
    void addProp(const char *prop) {
        _props.emplace_back(prop);
    }
    void addProps(const char** props, size_t cnt)
    {
        for (size_t i = 0; i < cnt; ++i)
        {
            _props.emplace_back(props[i]);
        }
    }
    const char** getProps() {
        return _props.data();
    }
    size_t getPropCnt() {
        return _props.size();
    }
};

 static VkBool32 debugCallback(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageType,
    const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
    void* /*pUserData*/) {

    bool loaderError = false;
    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        if (messageType & VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT &&
            strcmp(pCallbackData->pMessageIdName, "Loader Message") == 0)
        {
            loaderError = true; // Vulkan loader prijavljuje fatalnu gresku kad ne moze pravilno da ucita neki layer i ako se uopste ne koristi. Ovo je dodano kako bi se te gre�ke ignorisale
        }
    }

    std::ostringstream errText;
    if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) ||
        (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) ||
        loaderError)
    {
        if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT)
            return VK_FALSE;
        std::cout << "Vulkan debug layer info: " << pCallbackData->pMessage << "\n";
        return VK_FALSE;
    }


    errText << "Error severity: " << messageSeverity << "\n";
    errText << "Error message: " << pCallbackData->pMessage;
    exitWithError(errText.str().c_str(), pCallbackData->messageIdNumber);
    return VK_FALSE;
}

 template<typename T>
 T loadVkFunc(VkInstance instance, const char* name) {
     static auto func = reinterpret_cast<T>(vkGetInstanceProcAddr(instance, name));

     if (!func) {
         std::ostringstream errText;
         errText << "Failed to load Vulkan function: " << name;
         exitWithError(errText.str().c_str());
     }
     return func;
 }

 struct SwapChainSupportDetails {
     VkSurfaceCapabilitiesKHR capabilities;
     std::vector<VkSurfaceFormatKHR> formats;
     std::vector<VkPresentModeKHR> presentModes;
 };

 static SwapChainSupportDetails querySwapChainSupport(const VkPhysicalDevice &device, const VkSurfaceKHR &surface)
 {
     SwapChainSupportDetails details;
     vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

     uint32_t formatCount;
     vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

     if (formatCount != 0) {
         details.formats.resize(formatCount);
         vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());
     }

     uint32_t presentModeCount;
     vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, nullptr);

     if (presentModeCount != 0) {
         details.presentModes.resize(presentModeCount);
         vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());
     }

     return details;
 }

 struct SwapChainProfile
 {
     VkSurfaceTransformFlagBitsKHR surfaceTransform;
     int imgCount;
     VkExtent2D extent;
     VkSurfaceFormatKHR format;
     VkPresentModeKHR presentMode;
 };


 static SwapChainProfile getSwapChainProfile(const VkPhysicalDevice& device, const VkSurfaceKHR& surface, int windowWidth, int windowHeight)
 {
     SwapChainSupportDetails allOptions = querySwapChainSupport(device, surface);
     SwapChainProfile profile;

     //vector store colors and formats and tries to choose the color and format with the smallest index possible
     const std::vector<VkColorSpaceKHR> preferredColorSpace = { VK_COLOR_SPACE_SRGB_NONLINEAR_KHR , VK_COLOR_SPACE_EXTENDED_SRGB_LINEAR_EXT, VK_COLOR_SPACE_HDR10_ST2084_EXT };
     const std::vector<VkFormat> preferredFormats =
     { VK_FORMAT_B8G8R8A8_SRGB, VK_FORMAT_R8G8B8A8_SRGB, VK_FORMAT_B8G8R8A8_UNORM, VK_FORMAT_R8G8B8A8_UNORM, VK_FORMAT_R16G16B16A16_SFLOAT,
       VK_FORMAT_A2B10G10R10_UNORM_PACK32, VK_FORMAT_A2B10G10R10_UNORM_PACK32 };

     if (allOptions.formats.empty() || allOptions.presentModes.empty())
         exitWithError("getSwapChainProfile called with incompatible device and surface combination");
     
     std::vector<VkSurfaceFormatKHR> surfaceFormats;
     std::vector<VkSurfaceFormatKHR>::iterator surfaceFormatIterator;

     //find preffered colorSpace
     for (const VkColorSpaceKHR& colorSpace : preferredColorSpace)
     {
         surfaceFormatIterator = std::find_if(allOptions.formats.begin(), allOptions.formats.end(), [&colorSpace](VkSurfaceFormatKHR sFormat) {return sFormat.colorSpace == colorSpace; });
         if (surfaceFormatIterator != allOptions.formats.end())
             surfaceFormats.push_back(*surfaceFormatIterator);
     }
     
     //if wanted colorSpace not found then every surface format is a valid choice
     if (surfaceFormats.size() == 0)
     {
         surfaceFormats = std::move(allOptions.formats);
     }

     bool found = false;

     //out of all valid surfaceFormats find the one with prefferd memory layout
     for (const VkFormat& format : preferredFormats)
     {
         surfaceFormatIterator = std::find_if(surfaceFormats.begin(), surfaceFormats.end(), [&format](VkSurfaceFormatKHR sFormat) {return sFormat.format == format; });
         if (surfaceFormatIterator != surfaceFormats.end()) {
             found = true;
             profile.format = *surfaceFormatIterator;
             break;
         }
     }

     
     //if preffered format was not found pick any
     if (!found)
         profile.format = surfaceFormats.at(0);


     //sorted from pick first to pick last (same as before)
     std::vector<VkPresentModeKHR> preferredPresentations =
     { VK_PRESENT_MODE_MAILBOX_KHR, VK_PRESENT_MODE_FIFO_KHR, VK_PRESENT_MODE_FIFO_RELAXED_KHR, VK_PRESENT_MODE_IMMEDIATE_KHR };
     //VK_PRESENT_MODE_FIFO_KHR is guaranteed to be abiable but other format are listed in case vector is reordered in future implementations

     auto presentationIterator = std::find_first_of(
         preferredPresentations.begin(), preferredPresentations.end(),
         allOptions.presentModes.begin(), allOptions.presentModes.end());

     if (presentationIterator == preferredPresentations.end())
         presentationIterator = allOptions.presentModes.begin();

     profile.presentMode = *presentationIterator;


     if (allOptions.capabilities.currentExtent.width !=
         std::numeric_limits<uint32_t>::max()) {
         profile.extent = allOptions.capabilities.currentExtent;
     }
     else {

         VkExtent2D actualExtent = {
             static_cast<uint32_t>(windowWidth),
             static_cast<uint32_t>(windowHeight)
         };
         
         actualExtent.width = std::clamp(actualExtent.width,
             allOptions.capabilities.minImageExtent.width,
             allOptions.capabilities.maxImageExtent.width);
         
         actualExtent.height = std::clamp(actualExtent.height,
             allOptions.capabilities.minImageExtent.height,
             allOptions.capabilities.maxImageExtent.height);
             
         profile.extent = actualExtent;
     }

     if(allOptions.capabilities.maxImageCount != 0)
        profile.imgCount = std::clamp(allOptions.capabilities.minImageCount + 1,
            allOptions.capabilities.minImageCount,
            allOptions.capabilities.maxImageCount);
        else
            profile.imgCount = allOptions.capabilities.minImageCount + 1;

     profile.surfaceTransform = allOptions.capabilities.currentTransform;

     return profile;
 }

 static SwapChainProfile getSwapChainProfile(const VkPhysicalDevice& device, const VkSurfaceKHR& surface, GLFWwindow *window)
 {
     int width, height;
     glfwGetFramebufferSize(window, &width, &height);
     return getSwapChainProfile(device, surface, width, height);
 }

 static VkPhysicalDevice pickPhysicalDevice(VkInstance &instance, const VkSurfaceKHR& surface, std::vector<VkPhysicalDevice> dissalowedDevices = {})
{
     uint32_t deviceCount = 0;
     vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
     std::vector<VkPhysicalDevice> devs(deviceCount);
     vkEnumeratePhysicalDevices(instance, &deviceCount, devs.data());

     if (deviceCount == 0)
         exitWithError("No Vulkan GPU devices found");
     
     VkPhysicalDeviceProperties deviceProperties;
     VkPhysicalDeviceFeatures deviceFeatures;
     VkPhysicalDeviceMemoryProperties memProps;
     std::vector<long long int> points(deviceCount);
     for (uint32_t i = 0; i < deviceCount; ++i) 
     {
         points[i] = 1;
         vkGetPhysicalDeviceProperties(devs[i], &deviceProperties);
         vkGetPhysicalDeviceFeatures(devs[i], &deviceFeatures);
         vkGetPhysicalDeviceMemoryProperties(devs[i], &memProps);


         for (uint32_t j = 0; j < memProps.memoryHeapCount; ++j) {
             points[i] += memProps.memoryHeaps[j].size / (1024 * 1024); //mb
         }

         points[i] *= (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) ? 10 : 1;
         points[i] *= (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_INTEGRATED_GPU) ? 3 : 1;

         if (std::find(dissalowedDevices.begin(), dissalowedDevices.end(), devs[i]) != dissalowedDevices.end())
             points[i] *= 0;

         uint32_t extensionCount = 0;
         vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &extensionCount, nullptr);
         std::vector<VkExtensionProperties> deviceExtensions(extensionCount);
         vkEnumerateDeviceExtensionProperties(devs[i], nullptr, &extensionCount, deviceExtensions.data());

         //make device that does not have VK_KHR_SWAPCHAIN_EXTENSION_NAME extension unsuitable
         if (std::find_if(deviceExtensions.begin(), deviceExtensions.end(),
             [](const VkExtensionProperties& prop)->bool {return strcmp(prop.extensionName, VK_KHR_SWAPCHAIN_EXTENSION_NAME) == 0; }) == deviceExtensions.end())
         {//VK_KHR_SWAPCHAIN extension is not supported
             points[i] *= 0;
         }
         else 
         {
             SwapChainSupportDetails swapDet = querySwapChainSupport(devs[i], surface);
             if (swapDet.presentModes.empty() || swapDet.formats.empty())
             { //swap chain is not capable to present on this surface
                points[i] *= 0;
             }
         }
     }

     auto it = std::max_element(points.begin(), points.end()); //deviceCount is checked > 0 so points has at least one element
     if (*it <= 0) 
     {
         //exitWithError("No suitable GPU device found");
         return nullptr;
     }

     return devs[it - points.begin()];
 }

 struct QueueFamily {
     int graphics = -1;
     int compute = -1;
     int transfer = -1;
     int presentation = -1;
 };

 static QueueFamily getQueueFamily(const VkPhysicalDevice& device, const VkSurfaceKHR &surface)
 {
     QueueFamily queueInd{};
     uint32_t cnt;
     vkGetPhysicalDeviceQueueFamilyProperties(device, &cnt, nullptr);
     std::vector<VkQueueFamilyProperties> props(cnt);
     vkGetPhysicalDeviceQueueFamilyProperties(device, &cnt, props.data());

     auto findDedicatedFamily = [&cnt, &props](VkQueueFlagBits required, const std::vector<VkQueueFlagBits>& disallowed) -> int{
         std::vector<int> foundQueue;
         for (uint32_t i = 0; i < cnt; ++i)
         {
             if (props[i].queueFlags & required)
             {
                 for (const auto flag : disallowed) 
                 {
                     if (props[i].queueFlags & flag)
                         goto CONTINUE_LOOP;
                 }
                foundQueue.push_back(i);
             }
         CONTINUE_LOOP:
             ;
         }
         if (foundQueue.empty())
             return -1;
         return *std::max_element(foundQueue.begin(), foundQueue.end(), 
             [&props](int ind1, int ind2)->bool{return props[ind1].queueCount < props[ind2].queueCount; });
     };

     auto findBiggestFamily = [&cnt, &props](VkQueueFlagBits requiredFlag) -> int { //TODO add second argument that lists families to skip if possible (another match can be found)
         uint32_t max_queues = 0;
         int index = -1;
         for (uint32_t i = 0; i < cnt; ++i)
         {
             if (props[i].queueFlags & requiredFlag)
             {
                 if (props[i].queueCount > max_queues)
                 {
                     max_queues = props[i].queueCount;
                     index = i;
                 }
             }
         }
         return index;
         };

     //find a family with most queues that has DMA transfer; chosen to be transfer queue
     {
         int found = findBiggestFamily(VK_QUEUE_TRANSFER_BIT);
         if (found != -1)
             queueInd.transfer = found;
     }



     //find dedicated graphics family, if not found then pick the one with most queues
     {
         int found = findDedicatedFamily(VK_QUEUE_GRAPHICS_BIT, {VK_QUEUE_COMPUTE_BIT, VK_QUEUE_TRANSFER_BIT});
         if (found == -1)
         {//dedicated graphics family not found
             found = findBiggestFamily(VK_QUEUE_GRAPHICS_BIT); //TODO: if found family is the same as the tranfer family then TRY to find second largest family
         }
         if (found != -1)
             queueInd.graphics = found;
     }

     //check if graphics queue supports presentation, if not set presentation to the first family that does
     VkBool32 supported = false;
     if (queueInd.graphics >= 0)
        vkGetPhysicalDeviceSurfaceSupportKHR(device, queueInd.graphics, surface, &supported);

     if(supported)
     {//graphics family supports presentation
        queueInd.presentation = queueInd.graphics;
     }
     else {
         VkBool32 supported;
         for (uint32_t i = 0; i < cnt; ++i)
         {
             vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supported);
             if (supported)
             {
                 queueInd.presentation = i;
                 break;
             }
         }
     }

     //find dedicated compute family, if not found then pick the one with most queues
     {
         int found = findDedicatedFamily(VK_QUEUE_COMPUTE_BIT, { VK_QUEUE_GRAPHICS_BIT, VK_QUEUE_TRANSFER_BIT });
         if (found == -1)
         {//dedicated compute family not found
             found = findBiggestFamily(VK_QUEUE_COMPUTE_BIT); //TODO: try to find family that has not been already chosen
         }
         if (found != -1)
             queueInd.compute = found;
     }

     return queueInd;
 }



int main()
{
    GLFWwindow* window;
    window = initGLFW();
    if (window == nullptr)
        exitWithError("glfw cant initialize");
    

    VkDebugUtilsMessengerCreateInfoEXT vkDebugCreateInfo{}; // for "VK_EXT_debug_utils" extensions
    vkDebugCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
    vkDebugCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
    vkDebugCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
    vkDebugCreateInfo.pfnUserCallback = debugCallback;
    vkDebugCreateInfo.pUserData = nullptr;

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "Hello Triangle";
    appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.pEngineName = "No Engine";
    appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;


    VkInstanceCreateInfo vkInfo{};
    vkInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    vkInfo.pNext = &vkDebugCreateInfo;
    vkInfo.pApplicationInfo = &appInfo;

    

    PropList extensions;
    {
        uint32_t extensions_cnt;
        const char** extensions_list;
        extensions_list = glfwGetRequiredInstanceExtensions(&extensions_cnt);
        extensions.addProps(extensions_list, extensions_cnt);
    }
    
    extensions.addProp("VK_EXT_debug_utils");

    {
        
        std::vector<VkExtensionProperties> aviableExtensions;
        uint32_t aviableExtensionsCnt = 0;
        vkEnumerateInstanceExtensionProperties(nullptr, &aviableExtensionsCnt, nullptr);
        aviableExtensions.resize(aviableExtensionsCnt);

        vkEnumerateInstanceExtensionProperties(nullptr, &aviableExtensionsCnt, aviableExtensions.data());

        //check if any requested extensions are not aviable
        const auto requestedExtensions = extensions.getProps();
        for (int i = 0; i < extensions.getPropCnt(); ++i) {
            int j = 0;
            for (; j < aviableExtensions.size(); ++j) {
                if (std::strcmp(requestedExtensions[i], aviableExtensions[j].extensionName) == 0)
                    break;
            }
            if (j == aviableExtensions.size()) {
                std::ostringstream error;
                error << "Extension \"" << requestedExtensions[i] << "\" not found";
                
                exitWithError(error.str().c_str(), VK_ERROR_INITIALIZATION_FAILED);
            }
        }
    }

    vkInfo.enabledExtensionCount = (uint32_t)extensions.getPropCnt();
    vkInfo.ppEnabledExtensionNames = extensions.getProps();
  

    PropList layers;

    layers.addProp("VK_LAYER_KHRONOS_validation");

    {
        uint32_t aviableLayerCnt;
        std::vector<VkLayerProperties> aviableLayers;
        vkEnumerateInstanceLayerProperties(&aviableLayerCnt, nullptr);
        aviableLayers.resize(aviableLayerCnt);
        vkEnumerateInstanceLayerProperties(&aviableLayerCnt, aviableLayers.data());

        //check if any requested layers are not aviable
        const auto requestedLayers = layers.getProps();
        for (int i = 0; i < layers.getPropCnt(); ++i) {
            int j = 0;
            for (; j < aviableLayers.size(); ++j) {
                if (std::strcmp(requestedLayers[i], aviableLayers[j].layerName) == 0)
                    break;
            }
            if (j == aviableLayers.size()) {
                std::ostringstream error;
                error << "Layer \"" << requestedLayers[i] << "\" not found";
                exitWithError(error.str().c_str(), VK_ERROR_INITIALIZATION_FAILED);
            }
        }
    }

    vkInfo.ppEnabledLayerNames = layers.getProps();
    vkInfo.enabledLayerCount = (uint32_t)layers.getPropCnt();

    VkInstance vkInstance{};
    auto code = vkCreateInstance(&vkInfo, nullptr, &vkInstance);
    if (code != VK_SUCCESS) 
    {
        exitWithError("Vulkan init error", code);
    }

    VkSurfaceKHR surface;
    {
        auto code = glfwCreateWindowSurface(vkInstance, window, nullptr, &surface);
        if (code != VK_SUCCESS)
            exitWithError("Error in creating surface");
    }

    VkPhysicalDevice device = pickPhysicalDevice(vkInstance, surface);
    QueueFamily queueIndices = getQueueFamily(device, surface);

    if (queueIndices.graphics < 0)
        exitWithError("no graphics queue family");
    if (queueIndices.presentation < 0)
        exitWithError("no presentation queue family");

    std::set<uint32_t> uniqueIndices; 
    for (int i : {queueIndices.compute, queueIndices.graphics, queueIndices.presentation, queueIndices.transfer})
        if (i >= 0) uniqueIndices.insert(i);


    std::vector<VkDeviceQueueCreateInfo> queueCreateInfo(uniqueIndices.size());
    {
        int i = 0;
        for (int index : uniqueIndices)
        {
            queueCreateInfo.at(i).sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.at(i).queueFamilyIndex = index;
            queueCreateInfo.at(i).queueCount = 1;
            queueCreateInfo.at(i).pQueuePriorities = []()->const float *{static constexpr float queuePriority = 1.0f; return &queuePriority;}();
            ++i;
        }
    }

    VkPhysicalDeviceFeatures deviceFeatures{};

    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = (uint32_t)queueCreateInfo.size();
    deviceInfo.pQueueCreateInfos = queueCreateInfo.data();
    deviceInfo.pEnabledFeatures = &deviceFeatures;

    deviceInfo.enabledLayerCount = 0; //DEPRECATED, IGNORED BY VULKAN

    
    std::vector<const char*> deviceExtentions{ VK_KHR_SWAPCHAIN_EXTENSION_NAME};
    //VK_KHR_SWAPCHAIN_EXTENSION_NAME checked for avilability by pickPhysicalDevice()
    deviceInfo.enabledExtensionCount = (uint32_t)deviceExtentions.size();
    deviceInfo.ppEnabledExtensionNames = deviceExtentions.data();
    

    VkDevice logicalDevice;
    {
        auto code = vkCreateDevice(device, &deviceInfo, nullptr, &logicalDevice);
        if (code != VK_SUCCESS)
            exitWithError("Cant create logical device");
    }

    VkQueue graphQueue, presentQueue;

    vkGetDeviceQueue(logicalDevice, queueIndices.graphics, 0, &graphQueue);
    vkGetDeviceQueue(logicalDevice, queueIndices.presentation, 0, &presentQueue);

    SwapChainProfile swapchainProfile = getSwapChainProfile(device, surface, window);

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageColorSpace = swapchainProfile.format.colorSpace;
    swapchainInfo.imageExtent = swapchainProfile.extent;
    swapchainInfo.imageFormat = swapchainProfile.format.format;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = swapchainProfile.imgCount;
    swapchainInfo.presentMode = swapchainProfile.presentMode;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapchainInfo.preTransform = swapchainProfile.surfaceTransform;
    swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainInfo.clipped = VK_TRUE;

    swapchainInfo.oldSwapchain = VK_NULL_HANDLE; //TODO

    uint32_t queueFamilyIndicesUi32[] = { (uint32_t)queueIndices.graphics, (uint32_t)queueIndices.presentation };

    if (queueIndices.graphics == queueIndices.presentation)
    {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }
    else 
    {
        swapchainInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainInfo.queueFamilyIndexCount = 2;
        swapchainInfo.pQueueFamilyIndices = queueFamilyIndicesUi32;
    }

    VkSwapchainKHR swapChain;

    {
        auto code = vkCreateSwapchainKHR(logicalDevice, &swapchainInfo, nullptr, &swapChain);
        if (code != VK_SUCCESS)
            exitWithError("Swapchain creation failed");
    }

    uint32_t swapchainImgCnt;
    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapchainImgCnt, nullptr);

    std::vector<VkImage> swapChainImages(swapchainImgCnt);
    vkGetSwapchainImagesKHR(logicalDevice, swapChain, &swapchainImgCnt, swapChainImages.data());

    std::vector<VkImageView> swapchaingImageView(swapchainImgCnt);
    for (int i = 0; i<swapchainImgCnt; ++i)
    {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.format = swapchainProfile.format.format;
        viewInfo.image = swapChainImages[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;

        viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.layerCount = 1;

        auto code = vkCreateImageView(logicalDevice, &viewInfo, nullptr, &swapchaingImageView[i]);
        if (code != VK_SUCCESS)
            exitWithError("failed to create imageView with index ${error_code}", i);
    }

    

    VkShaderModule vertexShader;
    VkShaderModule fragmentShader;

    {
        std::string path = SHADERS_FOLDER_LOCATION;
        vertexShader = createShader(logicalDevice, path + "/vert.spv");
        fragmentShader = createShader(logicalDevice, path + "/frag.spv");
    }

    VkPipelineShaderStageCreateInfo shaderStages[2]{};

    shaderStages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[0].module = vertexShader;
    shaderStages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    shaderStages[0].pName = "main";

    shaderStages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    shaderStages[1].module = fragmentShader;
    shaderStages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    shaderStages[1].pName = "main";

    std::vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = static_cast<uint32_t>(dynamicStates.size());
    dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineVertexInputStateCreateInfo vertexInputInfo{};
    vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInputInfo.vertexBindingDescriptionCount = 0;
    vertexInputInfo.pVertexBindingDescriptions = nullptr;
    vertexInputInfo.vertexAttributeDescriptionCount = 0;
    vertexInputInfo.pVertexAttributeDescriptions = nullptr;

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    inputAssembly.primitiveRestartEnable = VK_FALSE;

    
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = 0.0f;
    viewport.width = static_cast<float>(swapchainProfile.extent.width);
    viewport.height = static_cast<float>(swapchainProfile.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
    scissor.offset = { 0, 0 };
    scissor.extent = swapchainProfile.extent;

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
    rasterizer.depthBiasConstantFactor = 0.0f; // ignored
    rasterizer.depthBiasClamp = 0.0f;          // ignored
    rasterizer.depthBiasSlopeFactor = 0.0f;    // ignored

    VkPipelineMultisampleStateCreateInfo multisampling{};
    multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisampling.sampleShadingEnable = VK_FALSE;
    multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    multisampling.minSampleShading = 1.0f; // Optional
    multisampling.pSampleMask = nullptr; // Optional
    multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
    multisampling.alphaToOneEnable = VK_FALSE; // Optional


    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    colorBlendAttachment.blendEnable = VK_TRUE;
    colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
    colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
    colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
    colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
    colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
    colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;

    VkPipelineColorBlendStateCreateInfo colorBlending{};
    colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlending.logicOpEnable = VK_FALSE;                
    colorBlending.logicOp = VK_LOGIC_OP_COPY;   // ignored 
    colorBlending.attachmentCount = 1;
    colorBlending.pAttachments = &colorBlendAttachment;
    //ignored since no blend attacment is using constants
    colorBlending.blendConstants[0] = 0.0f;   // R
    colorBlending.blendConstants[1] = 0.0f;   // G
    colorBlending.blendConstants[2] = 0.0f;   // B
    colorBlending.blendConstants[3] = 0.0f;   // A

    VkRenderPass renderPass;
    VkPipelineLayout pipelineLayout;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo{};
    pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    pipelineLayoutInfo.setLayoutCount = 0;
    pipelineLayoutInfo.pSetLayouts = nullptr;
    pipelineLayoutInfo.pushConstantRangeCount = 0;
    pipelineLayoutInfo.pPushConstantRanges = nullptr;

    if (vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &pipelineLayout) != VK_SUCCESS)
    {
        exitWithError("cant create pipelineLayout");
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = swapchainProfile.format.format;
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
    

    VkSubpassDependency dependency{}; //make sure load operations are done just before frame write
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.srcAccessMask = 0;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    
    // Create the render pass info structure
    VkRenderPassCreateInfo renderPassInfo{};
    renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassInfo.attachmentCount = 1;            // Only one attachment (color attachment)
    renderPassInfo.pAttachments = &colorAttachment; // Pointer to your color attachment description
    renderPassInfo.subpassCount = 1;                // One subpass
    renderPassInfo.pSubpasses = &subpass;           // Pointer to your subpass description
    renderPassInfo.dependencyCount = 1;
    renderPassInfo.pDependencies = &dependency;

    // Create the render pass
    if (vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &renderPass) != VK_SUCCESS) {
        exitWithError("failed to create render pass!");
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
    pipelineInfo.pDepthStencilState = nullptr; 
    pipelineInfo.pColorBlendState = &colorBlending;
    pipelineInfo.pDynamicState = &dynamicState;

    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;

    VkPipeline graphicsPipeline;
    if (vkCreateGraphicsPipelines(logicalDevice, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &graphicsPipeline) != VK_SUCCESS)
    {
       exitWithError("failed to create graphics pipeline!");
    }


    vkDestroyShaderModule(logicalDevice, vertexShader, nullptr);
    vkDestroyShaderModule(logicalDevice, fragmentShader, nullptr);



    std::vector<VkFramebuffer> swapChainFramebuffers;
    swapChainFramebuffers.resize(swapchaingImageView.size());
    for (int i = 0; i < swapchaingImageView.size(); ++i)
    {
        VkImageView attachments[] = { swapchaingImageView[i] };

        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = renderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.pAttachments = attachments;
        framebufferInfo.width = swapchainProfile.extent.width;
        framebufferInfo.height = swapchainProfile.extent.height;
        framebufferInfo.layers = 1;

        if (vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &swapChainFramebuffers[i]) != VK_SUCCESS)
            exitWithError("failed to create framebuffer!", i);

    }

    VkCommandPool commandPool;
    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT | VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolInfo.queueFamilyIndex = queueIndices.graphics;
    if (vkCreateCommandPool(logicalDevice, &poolInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        exitWithError("failed to create command pool!");
    }

    VkCommandBuffer commandBuffer;

    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.commandPool = commandPool;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandBufferCount = 1;

    if (vkAllocateCommandBuffers(logicalDevice, &allocInfo, &commandBuffer) != VK_SUCCESS) {
        exitWithError("failed to allocate command buffers!");
    }





    const auto setUpCommand = [&renderPass, &swapChainFramebuffers, &swapchainProfile, &graphicsPipeline, &viewport, &scissor](int imageIndex, const VkCommandBuffer &cmdBuffer)
        {
            VkCommandBufferBeginInfo beginInfo{};
            beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            beginInfo.flags = 0;
            beginInfo.pInheritanceInfo = nullptr;

            if (vkBeginCommandBuffer(cmdBuffer, &beginInfo) != VK_SUCCESS)
                exitWithError("failed to begin recording command buffer!");

            VkRenderPassBeginInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
            renderPassInfo.renderPass = renderPass;
            renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];

            renderPassInfo.renderArea.offset = { 0, 0 };
            renderPassInfo.renderArea.extent = swapchainProfile.extent;
            VkClearValue clearColor = { { {0.0f, 0.0f, 0.0f, 1.0f} } };
            renderPassInfo.clearValueCount = 1;
            renderPassInfo.pClearValues = &clearColor;

            vkCmdBeginRenderPass(cmdBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
            vkCmdBindPipeline(cmdBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

            vkCmdSetViewport(cmdBuffer, 0, 1, &viewport);
            vkCmdSetScissor(cmdBuffer, 0, 1, &scissor);
            vkCmdDraw(cmdBuffer, 3, 1, 0, 0);
            vkCmdEndRenderPass(cmdBuffer);
            if (vkEndCommandBuffer(cmdBuffer) != VK_SUCCESS)
                exitWithError("Failed to create command buffer");

        };





    VkSemaphore imageAvailableSemaphore{};
    VkFence inFlightFence{};

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    
    if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &imageAvailableSemaphore) != VK_SUCCESS ||
        vkCreateFence(logicalDevice, &fenceInfo, nullptr, &inFlightFence) != VK_SUCCESS) {
        exitWithError("failed to create synchronisation objects!");
    }



    VkSemaphore waitSemaphores[] = { imageAvailableSemaphore };
    VkPipelineStageFlags waitStages[] = { VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT };


    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = waitSemaphores;
    submitInfo.pWaitDstStageMask = waitStages;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    submitInfo.signalSemaphoreCount = 1;

    std::vector<VkSemaphore> renderFinishedSemaphore(swapchainProfile.imgCount);
    for (int i = 0; i < renderFinishedSemaphore.size(); ++i) 
    {
        if (vkCreateSemaphore(logicalDevice, &semaphoreInfo, nullptr, &renderFinishedSemaphore[i]) != VK_SUCCESS)
        {
            exitWithError("failed to create semaphore!");
        }
    }
    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
   


    uint32_t imageIndex = 0;

    VkSwapchainKHR swapChains[] = { swapChain };
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = swapChains;
    presentInfo.pImageIndices = &imageIndex;
    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents(); 

        vkWaitForFences(logicalDevice, 1, &inFlightFence, VK_TRUE, UINT64_MAX);
        vkResetFences(logicalDevice, 1, &inFlightFence);
        vkAcquireNextImageKHR(logicalDevice, swapChain, UINT64_MAX, imageAvailableSemaphore, VK_NULL_HANDLE, &imageIndex);
        vkResetCommandBuffer(commandBuffer, 0);
        setUpCommand(imageIndex, commandBuffer);
        submitInfo.pSignalSemaphores = &renderFinishedSemaphore[imageIndex];
        if (vkQueueSubmit(graphQueue, 1, &submitInfo, inFlightFence) != VK_SUCCESS)
            exitWithError("cmd buffer failed to submit");

        presentInfo.pWaitSemaphores = &renderFinishedSemaphore[imageIndex];
        vkQueuePresentKHR(presentQueue, &presentInfo);
    }

    vkDeviceWaitIdle(logicalDevice);

    vkDestroySemaphore(logicalDevice, imageAvailableSemaphore, nullptr);
    for(int i = 0; i<renderFinishedSemaphore.size(); ++i)
        vkDestroySemaphore(logicalDevice, renderFinishedSemaphore[i], nullptr);
    vkDestroyFence(logicalDevice, inFlightFence, nullptr);

    vkDestroyCommandPool(logicalDevice, commandPool, nullptr);

    for (auto framebuffer : swapChainFramebuffers) 
    {
        vkDestroyFramebuffer(logicalDevice, framebuffer, nullptr);
    }
    
    vkDestroyPipeline(logicalDevice, graphicsPipeline, nullptr);
    vkDestroyPipelineLayout(logicalDevice, pipelineLayout, nullptr);
    vkDestroyRenderPass(logicalDevice, renderPass, nullptr);

    for (const auto& view : swapchaingImageView)
    {
        vkDestroyImageView(logicalDevice, view, nullptr);
    }

    vkDestroySwapchainKHR(logicalDevice, swapChain, nullptr);
    vkDestroyDevice(logicalDevice, nullptr);
    vkDestroySurfaceKHR(vkInstance, surface, nullptr);
    vkDestroyInstance(vkInstance, nullptr);
    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}




static std::vector<char> readFile(const std::string& filename)
{
    // Open the file in binary mode and set the read position to the end (ate)
    std::ifstream shaderFile(filename, std::ios::ate | std::ios::binary); //filename is allowed to use "/" on windows
    if (!shaderFile.is_open()) {
        std::string err = "failed to open file \"";
        err += filename;
        err += "\"";
        exitWithError(err.c_str());
    }

    // Get the size of the file
    std::size_t fileSize = static_cast<std::size_t>(shaderFile.tellg());
    // Allocate a buffer to hold the file contents
    std::vector<char> buffer(fileSize);


    shaderFile.seekg(0);
    shaderFile.read(buffer.data(), fileSize);


    shaderFile.close();

    return buffer;
}

static VkShaderModule createShader(const VkDevice &device, const std::string& filePath) 
{
    std::vector<char> code = readFile(filePath);

    VkShaderModuleCreateInfo createInfo{};
    createInfo.codeSize = code.size();
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.pCode = reinterpret_cast<const uint32_t*>(code.data()); //vector properly aligns data to be accessable by uint32_t so access is optimized

    VkShaderModule shader;
    auto returnCode = vkCreateShaderModule(device, &createInfo, nullptr, &shader);
    if (returnCode != VK_SUCCESS) {
        std::string error = "Cant create shader: ";
        error += filePath;
        exitWithError(error.c_str());
    }

    return shader;
    //its ok to delete vector code now
}