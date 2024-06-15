#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define AVEN_MAX_ALIGNMENT 16
#include "aven.h"

#define ARENA_SIZE 1024 * 1024 * 8
#define MAX_FRAMES_IN_FLIGHT 2

#ifdef ENABLE_VALIDATION_LAYERS
const char *VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};
#endif // ENABLE_VALIDATION_LAYERS

const char *DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    VK_KHR_DYNAMIC_RENDERING_EXTENSION_NAME,
};

typedef struct { float pos[2]; float color[3]; } Vertex;

const Vertex VERTICES[] = {
    { {-0.5f, -0.5f}, {1.0f, 0.0f, 0.0f} },
    { {0.5f, -0.5f}, {0.0f, 1.0f, 0.0f} },
    { {0.5f, 0.5f}, {0.0f, 0.0f, 1.0f} },
    { {-0.5f, 0.5f}, {1.0f, 1.0f, 1.0f} }
};

const uint16_t INDICES[] = { 0, 1, 2, 2, 3, 0 };

VkVertexInputBindingDescription VERTEX_BINDING_DESCRIPTIONS[] = {
    {
        .binding = 0,
        .stride = sizeof(Vertex),
        .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
    },
};

VkVertexInputAttributeDescription VERTEX_ATTRIBUTE_DESCRIPTIONS[] = {
    {
        .binding = 0,
        .location = 0,
        .format = VK_FORMAT_R32G32_SFLOAT,
        .offset = offsetof(Vertex, pos),
    },
    {
        .binding = 0,
        .location = 1,
        .format = VK_FORMAT_R32G32B32_SFLOAT,
        .offset = offsetof(Vertex, color),
    },
};

typedef Slice(VkImage) VkImageSlice;
typedef Slice(VkImageView) VkImageViewSlice;

typedef struct {
    uint32_t width;
    uint32_t height;

    GLFWwindow *window;

    VkInstance instance;

#ifdef ENABLE_VALIDATION_LAYERS
    VkDebugUtilsMessengerEXT debug_messenger;
#endif // ENABLE_VALIDATION_LAYERS

    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    VkImageSlice swapchain_images;
    VkImageViewSlice swapchain_image_views;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;

    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

    Arena base_swapchain_arena;
    uint32_t current_frame;
    bool framebuffer_resized;
} HelloTriangleApp;

typedef enum {
    HT_ERROR_NONE = 0,
    HT_ERROR_MAIN_MALLOC,
    HT_ERROR_INIT_WINDOW,
    HT_ERROR_INIT_VULKAN_VOLK,
    HT_ERROR_CREATE_INSTANCE_ALLOC,
    HT_ERROR_CREATE_INSTANCE_CREATE,
    HT_ERROR_CREATE_SURFACE,
    HT_ERROR_FIND_QUEUE_FAMILIES_ALLOC,
    HT_ERROR_CHECK_DEVICE_EXTENSION_SUPPORT_ALLOC,
    HT_ERROR_QUERY_SWAP_CHAIN_SUPPORT_ALLOC,
    HT_ERROR_PICK_PHYSICAL_DEVICE_ENUMERATE,
    HT_ERROR_PICK_PHYSICAL_DEVICE_ALLOC,
    HT_ERROR_PICK_PHYSICAL_DEVICE_SUITABLE,
    HT_ERROR_CREATE_LOGICAL_DEVICE,
    HT_ERROR_CREATE_SWAP_CHAIN_CREATE,
    HT_ERROR_CREATE_SWAP_CHAIN_ALLOC,
    HT_ERROR_CREATE_IMAGE_VIEWS_ALLOC,
    HT_ERROR_CREATE_IMAGE_VIEWS_CREATE,
    HT_ERROR_READ_FILE_OPEN,
    HT_ERROR_READ_FILE_SEEK,
    HT_ERROR_READ_FILE_TELL,
    HT_ERROR_READ_FILE_ALLOC,
    HT_ERROR_READ_FILE_READ,
    HT_ERROR_CREATE_SHADER_MODULE,
    HT_ERROR_CREATE_RENDER_PASS,
    HT_ERROR_CREATE_GRAPHICS_PIPELINE_LAYOUT,
    HT_ERROR_CREATE_GRAPHICS_PIPELINE_CREATE,
    HT_ERROR_CREATE_FRAMEBUFFER_ALLOC,
    HT_ERROR_CREATE_FRAMEBUFFER_CREATE,
    HT_ERROR_CREATE_COMMAND_POOL,
    HT_ERROR_CREATE_COMMAND_BUFFER,
    HT_ERROR_RECORD_COMMAND_BUFFER_BEGIN,
    HT_ERROR_RECORD_COMMAND_BUFFER_END,
    HT_ERROR_CREATE_SYNC_OBJECTS_AVAILABLE,
    HT_ERROR_CREATE_SYNC_OBJECTS_FINISHED,
    HT_ERROR_CREATE_SYNC_OBJECTS_FENCE,
    HT_ERROR_DRAW_FRAME_SWAPCHAIN,
    HT_ERRROR_DRAW_FRAME_SUBMIT,
    HT_ERROR_FIND_MEMORY_TYPE,
    HT_ERROR_CREATE_BUFFER_CREATE,
    HT_ERROR_CREATE_BUFFER_MEMORY,
    HT_ERROR_COPY_BUFFER_ALLOCATE,
    HT_ERROR_COPY_BUFFER_COMMAND,
    HT_ERROR_COPY_BUFFER_SUBMIT,
#ifdef ENABLE_VALIDATION_LAYERS
    HT_ERROR_CHECK_VALIDATION_LAYER_SUPPORT_ALLOC,
    HT_ERROR_SETUP_DEBUG_MESSENGER,
    HT_ERROR_CREATE_INSTANCE_VALIDATION,
#endif // ENABLE_VALIDATION_LAYERS
} HelloTriangleError;

void framebuffer_resize_callback(GLFWwindow *window, int width, int height) {
    (void)width;
    (void)height;

    HelloTriangleApp *app = glfwGetWindowUserPointer(window);
    app->framebuffer_resized = true;
}

static int init_window(HelloTriangleApp *app) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    app->window = glfwCreateWindow(
        (int)app->width,
        (int)app->height,
        "Vulkan",
        NULL,
        NULL
    );
    if (app->window == NULL) {
        return HT_ERROR_INIT_WINDOW;
    }

    glfwSetWindowUserPointer(app->window, app);
    glfwSetFramebufferSizeCallback(app->window, framebuffer_resize_callback);

    return 0;
}

typedef Result(bool) BoolResult;

#ifdef ENABLE_VALIDATION_LAYERS
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
) {
    (void)message_severity;
    (void)message_type;
    (void)user_data;

    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);

    return VK_FALSE;
}

static BoolResult check_validation_layer_support(Arena temp_arena) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties *available_layers = arena_create_array(
        VkLayerProperties,
        &temp_arena,
        layer_count
    );
    if (available_layers == NULL) {
        return (BoolResult){
            .error = HT_ERROR_CHECK_VALIDATION_LAYER_SUPPORT_ALLOC
        };
    }

    vkEnumerateInstanceLayerProperties(&layer_count, available_layers);

    for (size_t j = 0; j < countof(VALIDATION_LAYERS); ++j) {
        bool layer_found = false;
        for (uint32_t i = 0; i < layer_count; ++i) {
            int diff = strcmp(
                VALIDATION_LAYERS[j],
                available_layers[i].layerName
            );
            if (diff == 0) {
                layer_found = true;
                break;
            }
        }
        if (!layer_found) {
            return (BoolResult){ .payload = false };
        }
    }

    return (BoolResult){ .payload = true };
}

static VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info(void) {
    return (VkDebugUtilsMessengerCreateInfoEXT){
        .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
        .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = NULL,
    };
}

static int setup_debug_messenger(HelloTriangleApp *app) {
    VkDebugUtilsMessengerCreateInfoEXT create_info =
        debug_messenger_create_info();

    VkResult result = vkCreateDebugUtilsMessengerEXT(
        app->instance,
        &create_info,
        NULL,
        &app->debug_messenger
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_SETUP_DEBUG_MESSENGER;
    }
    
    return 0;
}
#endif // ENABLE_VALIDATION_LAYERS

static int create_instance(HelloTriangleApp *app, Arena temp_arena) {
#ifdef ENABLE_VALIDATION_LAYERS
    {
        BoolResult layer_support_result = check_validation_layer_support(
            temp_arena
        );
        if (layer_support_result.error != 0) {
            return layer_support_result.error;
        }

        if (!layer_support_result.payload) {
            return HT_ERROR_CREATE_INSTANCE_VALIDATION;
        }
    }

    VkDebugUtilsMessengerCreateInfoEXT debug_create_info =
        debug_messenger_create_info();
#endif // ENABLE_VALIDATION_LAYERS

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_3,
    };

    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(
        &glfw_extension_count
    );

    uint32_t extension_count = glfw_extension_count;
    const char **extensions = arena_create_array(
        const char *,
        &temp_arena,
        extension_count
    );
    if (extensions == NULL) {
        return HT_ERROR_CREATE_INSTANCE_ALLOC;
    }

    memcpy(
        extensions,
        glfw_extensions,
        sizeof(*glfw_extensions) * glfw_extension_count
    );

#ifdef ENABLE_VALIDATION_LAYERS
    extensions[extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;
    extension_count += 1;
#endif // ENABLE_VALIDATION_LAYERS

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
#ifdef ENABLE_VALIDATION_LAYERS
        .enabledLayerCount = countof(VALIDATION_LAYERS),
        .ppEnabledLayerNames = VALIDATION_LAYERS,
        .pNext = &debug_create_info,
#endif // ENABLE_VALIDATION_LAYERS
    };

    VkResult result = vkCreateInstance(&create_info, NULL, &app->instance);
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_INSTANCE_CREATE;
    }

    volkLoadInstanceOnly(app->instance);

    return 0;
}

static int create_surface(HelloTriangleApp *app) {
    VkResult result = glfwCreateWindowSurface(
        app->instance,
        app->window,
        NULL,
        &app->surface
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_SURFACE;
    }

    return 0;
}

typedef struct {
    Optional(uint32_t) graphics_family;
    Optional(uint32_t) present_family;
} QueueFamilyIndices;

typedef Result(QueueFamilyIndices) QueueFamilyIndicesResult;

static QueueFamilyIndicesResult find_queue_families(
    HelloTriangleApp *app,
    VkPhysicalDevice device,
    Arena temp_arena
) {
    QueueFamilyIndices indices = { 0 };

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = arena_create_array(
        VkQueueFamilyProperties,
        &temp_arena,
        queue_family_count
    );
    if (queue_families == NULL) {
        return (QueueFamilyIndicesResult){
            .error = HT_ERROR_FIND_QUEUE_FAMILIES_ALLOC
        };
    }

    vkGetPhysicalDeviceQueueFamilyProperties(
        device,
        &queue_family_count,
        queue_families
    );

    for (uint32_t i = 0; i < queue_family_count; ++i) {
        if (queue_families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphics_family.value = i;
            indices.graphics_family.valid = true;
        }

        VkBool32 present_support = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(
            device,
            i,
            app->surface,
            &present_support
        );
        if (present_support) {
            indices.present_family.value = i;
            indices.present_family.valid = true;
        }

        if (indices.graphics_family.valid and indices.present_family.valid) {
            break;
        }
    }

    return (QueueFamilyIndicesResult){ .payload = indices };
}

static BoolResult check_device_extension_support(
    VkPhysicalDevice device,
    Arena temp_arena
) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties *available_extensions = arena_create_array(
        VkExtensionProperties,
        &temp_arena,
        extension_count
    );
    if (available_extensions == NULL) {
        return (BoolResult){
            .error = HT_ERROR_CHECK_DEVICE_EXTENSION_SUPPORT_ALLOC
        };
    }
 
    vkEnumerateDeviceExtensionProperties(
        device,
        NULL,
        &extension_count,
        available_extensions
    );

    for (size_t j = 0; j < countof(DEVICE_EXTENSIONS); ++j) {
        bool extension_found = false;
        for (uint32_t i = 0; i < extension_count; ++i) {
            int diff = strcmp(
                DEVICE_EXTENSIONS[j],
                available_extensions[i].extensionName
            );
            if (diff == 0) {
                extension_found = true;
                break;
            }
        }
        if (!extension_found) {
            return (BoolResult){ .payload = false };
        }
    }

    return (BoolResult){ .payload = true };
}

typedef Slice(VkSurfaceFormatKHR) VkSurfaceFormatKHRSlice;
typedef Slice(VkPresentModeKHR) VkPresentModeKHRSlice;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHRSlice formats;
    VkPresentModeKHRSlice present_modes;
} SwapchainSupportDetails;

typedef Result(SwapchainSupportDetails) SwapchainSupportDetailsResult;

static SwapchainSupportDetailsResult query_swapchain_support(
    HelloTriangleApp *app,
    VkPhysicalDevice device,
    Arena *perm_arena
) {
    SwapchainSupportDetails details = { 0 };
    
    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        device,
        app->surface,
        &details.capabilities
    );

    uint32_t format_count;
    vkGetPhysicalDeviceSurfaceFormatsKHR(
        device,
        app->surface,
        &format_count,
        NULL
    );

    if (format_count !=0 ) {
        details.formats.ptr = arena_create_array(
            VkSurfaceFormatKHR,
            perm_arena,
            format_count
        );
        if (details.formats.ptr == NULL) {
            return (SwapchainSupportDetailsResult){
                .error = HT_ERROR_QUERY_SWAP_CHAIN_SUPPORT_ALLOC
            };
        }

        vkGetPhysicalDeviceSurfaceFormatsKHR(
            device,
            app->surface,
            &format_count,
            details.formats.ptr
        );
        details.formats.len = format_count;
    }

    uint32_t present_mode_count;
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device,
        app->surface,
        &present_mode_count,
        NULL
    );

    if (present_mode_count != 0) {
        details.present_modes.ptr = arena_create_array(
            VkPresentModeKHR,
            perm_arena,
            present_mode_count
        );
        if (details.present_modes.ptr == NULL) {
            return (SwapchainSupportDetailsResult){
                .error = HT_ERROR_QUERY_SWAP_CHAIN_SUPPORT_ALLOC
            };
        }

        vkGetPhysicalDeviceSurfacePresentModesKHR(
            device,
            app->surface,
            &present_mode_count,
            details.present_modes.ptr
        );
        details.present_modes.len = present_mode_count;
    }

    return (SwapchainSupportDetailsResult){ .payload = details };
}

static BoolResult is_device_suitable(
    HelloTriangleApp *app,
    VkPhysicalDevice device,
    Arena temp_arena
) {
    {
        QueueFamilyIndicesResult result = find_queue_families(
            app,
            device,
            temp_arena
        );
        if (result.error != 0) {
            return (BoolResult){ .error = result.error };
        }
        
        if (
            !result.payload.graphics_family.valid or
            !result.payload.present_family.valid
        ) {
            return (BoolResult){ .payload = false };
        }
    }
    {
        BoolResult result = check_device_extension_support(
            device,
            temp_arena
        );
        if (result.error != 0 or !result.payload) {
            return result;
        }
    }
    {
        SwapchainSupportDetailsResult result = query_swapchain_support(
            app,
            device,
            &temp_arena
        );
        if (result.error != 0) {
            return (BoolResult){ .error = result.error };
        }

        if (
            result.payload.formats.len == 0 or
            result.payload.present_modes.len == 0
        ) {
            return (BoolResult){ .payload = false };
        }
    }

    return (BoolResult){ .payload = true };
}

static int pick_physical_device(HelloTriangleApp *app, Arena temp_arena) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);
    if (device_count == 0) {
        return HT_ERROR_PICK_PHYSICAL_DEVICE_ENUMERATE;
    }

    VkPhysicalDevice *devices = arena_create_array(
        VkPhysicalDevice,
        &temp_arena,
        device_count
    );
    if (devices == NULL) {
        return HT_ERROR_PICK_PHYSICAL_DEVICE_ALLOC;
    }

    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    for (uint32_t i = 0; i < device_count; ++i) {
        BoolResult result = is_device_suitable(app, devices[i], temp_arena);
        if (result.error != 0) {
            return result.error;
        }

        if (result.payload) {
            app->physical_device = devices[i];
            break;
        }
    }

    if (app->physical_device == VK_NULL_HANDLE) {
        return HT_ERROR_PICK_PHYSICAL_DEVICE_SUITABLE;
    }

    return 0;
}

static int create_logical_device(HelloTriangleApp *app, Arena temp_arena) {
    QueueFamilyIndices indices;
    {
        QueueFamilyIndicesResult indices_result = find_queue_families(
            app,
            app->physical_device,
            temp_arena
        );
        if (indices_result.error != 0) {
            return indices_result.error;
        }

        indices = indices_result.payload;
    }
    assert(indices.graphics_family.valid and indices.present_family.valid);

    uint32_t queue_families[] = {
        indices.graphics_family.value,
        indices.present_family.value
    };
    uint32_t queue_family_count = 2;
    if (indices.graphics_family.value == indices.present_family.value) {
        queue_family_count = 1;
    }

    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_infos[2];
    for (uint32_t i = 0; i < queue_family_count; ++i) {
        queue_create_infos[i] = (VkDeviceQueueCreateInfo){
            .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
            .queueFamilyIndex = queue_families[i],
            .queueCount = 1,
            .pQueuePriorities = &queue_priority,
        };
    }

    VkPhysicalDeviceFeatures device_features = { 0 };

    VkPhysicalDeviceDynamicRenderingFeaturesKHR dynamic_rendering_features = {
        .sType =
            VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES_KHR,
        .dynamicRendering = VK_TRUE,
    };

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .pNext = &dynamic_rendering_features,
        .queueCreateInfoCount = queue_family_count,
        .pQueueCreateInfos = queue_create_infos,
        .pEnabledFeatures = &device_features,
        .enabledExtensionCount = (uint32_t)countof(DEVICE_EXTENSIONS),
        .ppEnabledExtensionNames = DEVICE_EXTENSIONS,
    };

    VkResult result = vkCreateDevice(
        app->physical_device,
        &create_info,
        NULL,
        &app->device
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_LOGICAL_DEVICE;
    }

    volkLoadDevice(app->device);

    vkGetDeviceQueue(
        app->device,
        indices.graphics_family.value,
        0,
        &app->graphics_queue
    );
    vkGetDeviceQueue(
        app->device,
        indices.present_family.value,
        0,
        &app->present_queue
    );

    return 0;
}

static VkSurfaceFormatKHR choose_swap_surface_format(
    VkSurfaceFormatKHRSlice available_formats
) {
    for (size_t i = 0; i < available_formats.len; ++i) {
        VkSurfaceFormatKHR format = slice_get(available_formats, i);
        if (
            format.format == VK_FORMAT_B8G8R8A8_SRGB and
            format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
        ) {
            return format;
        }
    }

    return slice_get(available_formats, 0);
}

static VkPresentModeKHR choose_swap_present_mode(
    VkPresentModeKHRSlice available_present_modes
) {
    for (size_t i = 0; i < available_present_modes.len; ++i) {
        VkPresentModeKHR present_mode = slice_get(available_present_modes, i);
        if (present_mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent(
    HelloTriangleApp *app,
    VkSurfaceCapabilitiesKHR *capabilities
) {
    if (capabilities->currentExtent.width != UINT32_MAX) {
        return capabilities->currentExtent;
    }

    int width, height;
    glfwGetFramebufferSize(app->window, &width, &height);

    VkExtent2D actual_extent = {
        .width = (uint32_t)width,
        .height = (uint32_t)height,
    };
    actual_extent.width = max(
        actual_extent.width,
        capabilities->minImageExtent.width
    );
    actual_extent.width = min(
        actual_extent.width,
        capabilities->maxImageExtent.width
    );
    actual_extent.height = max(
        actual_extent.height,
        capabilities->minImageExtent.height
    );
    actual_extent.height = min(
        actual_extent.height,
        capabilities->maxImageExtent.height
    );

    return actual_extent;
}

static int create_swapchain(
    HelloTriangleApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    SwapchainSupportDetails swapchain_support;
    {
        SwapchainSupportDetailsResult result = query_swapchain_support(
            app,
            app->physical_device,
            &temp_arena
        );
        if (result.error != 0) {
            return result.error;
        }
        
        swapchain_support = result.payload;
    }

    VkSurfaceFormatKHR surface_format = choose_swap_surface_format(
        swapchain_support.formats
    );
    VkPresentModeKHR present_mode = choose_swap_present_mode(
        swapchain_support.present_modes
    );
    VkExtent2D extent = choose_swap_extent(
        app,
        &swapchain_support.capabilities
    );

    uint32_t image_count = swapchain_support.capabilities.minImageCount + 1;

    if (
        swapchain_support.capabilities.maxImageCount > 0 and
        image_count > swapchain_support.capabilities.maxImageCount
    ) {
        image_count = swapchain_support.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR create_info = {
        .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
        .surface = app->surface,
        .minImageCount = image_count,
        .imageFormat = surface_format.format,
        .imageColorSpace = surface_format.colorSpace,
        .imageExtent = extent,
        .imageArrayLayers = 1,
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .preTransform = swapchain_support.capabilities.currentTransform,
        .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        .presentMode = present_mode,
        .clipped = VK_TRUE,
        .oldSwapchain = VK_NULL_HANDLE,
    };

    QueueFamilyIndices indices;
    {
        QueueFamilyIndicesResult result = find_queue_families(
            app,
            app->physical_device,
            temp_arena
        );
        if (result.error != 0) {
            return result.error;
        }
        
        indices = result.payload;
    };

    uint32_t queue_family_indices[] = {
        indices.graphics_family.value,
        indices.present_family.value
    };

    if (indices.graphics_family.value != indices.present_family.value) {
        create_info.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        create_info.queueFamilyIndexCount = 2;
        create_info.pQueueFamilyIndices = queue_family_indices;
    } else {
        create_info.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        create_info.queueFamilyIndexCount = 0;
        create_info.pQueueFamilyIndices = NULL;
    }

    VkResult result = vkCreateSwapchainKHR(
        app->device,
        &create_info,
        NULL,
        &app->swapchain
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_SWAP_CHAIN_CREATE;
    }

    vkGetSwapchainImagesKHR(app->device, app->swapchain, &image_count, NULL);

    app->swapchain_images.ptr = arena_create_array(
        VkImage,
        swapchain_arena,
        image_count
    );
    if (app->swapchain_images.ptr == NULL) {
        return HT_ERROR_CREATE_SWAP_CHAIN_ALLOC;
    }

    vkGetSwapchainImagesKHR(
        app->device,
        app->swapchain,
        &image_count, 
        app->swapchain_images.ptr
    );
    app->swapchain_images.len = image_count;

    app->swapchain_image_format = surface_format.format;
    app->swapchain_extent = extent;

    return 0;
}

static int create_image_views(HelloTriangleApp *app, Arena *swapchain_arena) {
    app->swapchain_image_views.ptr = arena_create_array(
        VkImageView,
        swapchain_arena,
        app->swapchain_images.len
    );
    app->swapchain_image_views.len = app->swapchain_images.len;
    if (app->swapchain_image_views.ptr == NULL) {
        return HT_ERROR_CREATE_IMAGE_VIEWS_ALLOC;
    }

    for (size_t i = 0; i < app->swapchain_images.len; ++i) {
        VkImageViewCreateInfo create_info = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
            .image = slice_get(app->swapchain_images, i),
            .viewType = VK_IMAGE_VIEW_TYPE_2D,
            .format = app->swapchain_image_format,
            .components = {
                .r = VK_COMPONENT_SWIZZLE_IDENTITY,
                .g = VK_COMPONENT_SWIZZLE_IDENTITY,
                .b = VK_COMPONENT_SWIZZLE_IDENTITY,
                .a = VK_COMPONENT_SWIZZLE_IDENTITY,
            },
            .subresourceRange = {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        };

        VkResult result = vkCreateImageView(
            app->device,
            &create_info,
            NULL,
            &slice_get(app->swapchain_image_views, i)
        );
        if (result != VK_SUCCESS) {
            return HT_ERROR_CREATE_IMAGE_VIEWS_CREATE;
        }
    }

    return 0;
}

typedef Result(ByteSlice) ByteSliceResult;

static ByteSliceResult read_file(const char *filename, Arena *perm_arena) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return (ByteSliceResult){ .error = HT_ERROR_READ_FILE_OPEN };
    }

    int error = fseek(file, 0L, SEEK_END);
    if (error != 0) {
        fclose(file);
        return (ByteSliceResult){ .error = HT_ERROR_READ_FILE_SEEK };
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return (ByteSliceResult){ .error = HT_ERROR_READ_FILE_TELL };
    }

    rewind(file);

    ByteSlice bytes = {
        .ptr = arena_alloc(perm_arena, (size_t)len, 1),
        .len = (size_t)len,
    };
    if (bytes.ptr == NULL) {
        fclose(file);
        return (ByteSliceResult){ .error = HT_ERROR_READ_FILE_ALLOC };
    }
    
    size_t bytes_read = fread(bytes.ptr, 1, bytes.len, file);

    fclose(file);

    if (bytes_read != bytes.len) {
        return (ByteSliceResult){ .error = HT_ERROR_READ_FILE_READ };
    }

    return (ByteSliceResult){ .payload = bytes };
}

typedef Result(VkShaderModule) VkShaderModuleResult;

static VkShaderModuleResult create_shader_module(
    HelloTriangleApp *app,
    ByteSlice code
) {
    VkShaderModuleCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = code.len,
        .pCode = (void *)code.ptr,
    };
    
    VkShaderModule shader_module;
    VkResult result = vkCreateShaderModule(
        app->device,
        &create_info,
        NULL,
        &shader_module
    );
    if (result != VK_SUCCESS) {
        return (VkShaderModuleResult){
            .error = HT_ERROR_CREATE_SHADER_MODULE
        };
    }

    return (VkShaderModuleResult){ .payload = shader_module };
}

static int create_graphics_pipeline(HelloTriangleApp *app, Arena temp_arena) {
    ByteSlice vert_shader_code;
    {
        ByteSliceResult result = read_file("shaders/vert.spv", &temp_arena);
        if (result.error != 0) {
            return result.error;
        }

        vert_shader_code = result.payload;
    }

    ByteSlice frag_shader_code;
    {
        ByteSliceResult result = read_file("shaders/frag.spv", &temp_arena);
        if (result.error != 0) {
            return result.error;
        }

        frag_shader_code = result.payload;
    }

    VkShaderModule vert_shader_module;
    {
        VkShaderModuleResult result = create_shader_module(
            app,
            vert_shader_code
        );
        if (result.error != 0) {
            return result.error;
        }

        vert_shader_module = result.payload;
    }

    VkShaderModule frag_shader_module;
    {
        VkShaderModuleResult result = create_shader_module(
            app,
            frag_shader_code
        );
        if (result.error != 0) {
            return result.error;
        }

        frag_shader_module = result.payload;
    }

    VkPipelineShaderStageCreateInfo vert_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_VERTEX_BIT,
        .module = vert_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo frag_shader_stage_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
        .stage = VK_SHADER_STAGE_FRAGMENT_BIT,
        .module = frag_shader_module,
        .pName = "main",
    };

    VkPipelineShaderStageCreateInfo shader_stages[] = {
        vert_shader_stage_info,
        frag_shader_stage_info
    };

    VkPipelineVertexInputStateCreateInfo vertex_input_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
        .vertexBindingDescriptionCount = countof(VERTEX_BINDING_DESCRIPTIONS),
        .pVertexBindingDescriptions = VERTEX_BINDING_DESCRIPTIONS,
        .vertexAttributeDescriptionCount = countof(
            VERTEX_ATTRIBUTE_DESCRIPTIONS
        ),
        .pVertexAttributeDescriptions = VERTEX_ATTRIBUTE_DESCRIPTIONS,
    };

    VkPipelineInputAssemblyStateCreateInfo input_assembly = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
        .topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        .primitiveRestartEnable = false,
    };

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)app->swapchain_extent.width,
        .height = (float)app->swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = app->swapchain_extent,
    };

    VkDynamicState dynamic_states[] = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };

    VkPipelineDynamicStateCreateInfo dynamic_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
        .dynamicStateCount = (uint32_t)countof(dynamic_states),
        .pDynamicStates = dynamic_states,
    };

    VkPipelineViewportStateCreateInfo viewport_state = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
        .viewportCount = 1,
        .pViewports = &viewport,
        .scissorCount = 1,
        .pScissors = &scissor,
    };

    VkPipelineRasterizationStateCreateInfo rasterizer = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
        .depthClampEnable = VK_FALSE,
        .rasterizerDiscardEnable = VK_FALSE,
        .polygonMode = VK_POLYGON_MODE_FILL,
        .lineWidth = 1.0f,
        .cullMode = VK_CULL_MODE_BACK_BIT,
        .frontFace = VK_FRONT_FACE_CLOCKWISE,
        .depthBiasEnable = VK_FALSE,
        .depthBiasConstantFactor = 0.0f,
        .depthBiasClamp = 0.0f,
        .depthBiasSlopeFactor = 0.0f,
    };

    VkPipelineMultisampleStateCreateInfo multisampling = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
        .sampleShadingEnable = VK_FALSE,
        .rasterizationSamples = VK_SAMPLE_COUNT_1_BIT,
        .minSampleShading = 1.0f,
        .pSampleMask = NULL,
        .alphaToCoverageEnable = VK_FALSE,
        .alphaToOneEnable = VK_FALSE,
    };

    VkPipelineColorBlendAttachmentState color_blend_attachment = {
        .colorWriteMask = VK_COLOR_COMPONENT_R_BIT |
            VK_COLOR_COMPONENT_G_BIT |
            VK_COLOR_COMPONENT_B_BIT |
            VK_COLOR_COMPONENT_A_BIT,
        .blendEnable = VK_FALSE,
        .srcColorBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstColorBlendFactor = VK_BLEND_FACTOR_ZERO,
        .colorBlendOp = VK_BLEND_OP_ADD,
        .srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE,
        .dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO,
        .alphaBlendOp = VK_BLEND_OP_ADD,
    };

    VkPipelineColorBlendStateCreateInfo color_blending = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
        .logicOpEnable = VK_FALSE,
        .logicOp = VK_LOGIC_OP_COPY,
        .attachmentCount = 1,
        .pAttachments = &color_blend_attachment,
        .blendConstants = { 0.0f, 0.0f, 0.0f, 0.0f },
    };

    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 0,
        .pSetLayouts = NULL,
        .pushConstantRangeCount = 0,
        .pPushConstantRanges = NULL,
    };

    VkResult result = vkCreatePipelineLayout(
        app->device,
        &pipeline_layout_info,
        NULL,
        &app->pipeline_layout
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_GRAPHICS_PIPELINE_LAYOUT;
    }

    VkPipelineRenderingCreateInfoKHR pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO_KHR,
        .colorAttachmentCount = 1,
        .pColorAttachmentFormats = &app->swapchain_image_format,
    };

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
        .pNext = &pipeline_rendering_create_info,
        .stageCount = 2,
        .pStages = shader_stages,
        .pVertexInputState = &vertex_input_info,
        .pInputAssemblyState = &input_assembly,
        .pViewportState = &viewport_state,
        .pRasterizationState = &rasterizer,
        .pMultisampleState = &multisampling,
        .pDepthStencilState = NULL,
        .pColorBlendState = &color_blending,
        .pDynamicState = &dynamic_state,
        .layout = app->pipeline_layout,
        .renderPass = NULL,
        .subpass = 0,
        .basePipelineHandle = VK_NULL_HANDLE,
        .basePipelineIndex = -1,
    };

    result = vkCreateGraphicsPipelines(
        app->device,
        VK_NULL_HANDLE,
        1,
        &pipeline_info,
        NULL,
        &app->graphics_pipeline
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_GRAPHICS_PIPELINE_CREATE;
    }

    vkDestroyShaderModule(app->device, frag_shader_module, NULL);
    vkDestroyShaderModule(app->device, vert_shader_module, NULL);

    return 0;
}

static int create_command_pool(HelloTriangleApp *app, Arena temp_arena) {
    QueueFamilyIndices queue_family_indices;
    {
        QueueFamilyIndicesResult result = find_queue_families(
            app,
            app->physical_device,
            temp_arena
        );
        if (result.error != 0) {
            return result.error;
        }
        
        queue_family_indices = result.payload;
    };

    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT,
        .queueFamilyIndex = queue_family_indices.graphics_family.value,
    };
    VkResult result = vkCreateCommandPool(
        app->device,
        &pool_info,
        NULL,
        &app->command_pool
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_COMMAND_POOL;
    }

    return 0;
}

typedef Result(uint32_t) MemoryTypeIndexResult;

static MemoryTypeIndexResult find_memory_type(
    HelloTriangleApp *app,
    uint32_t type_filter,
    VkMemoryPropertyFlags properties
) {
    VkPhysicalDeviceMemoryProperties mem_properties;
    vkGetPhysicalDeviceMemoryProperties(
        app->physical_device,
        &mem_properties
    );

    for (uint32_t i = 0; i < mem_properties.memoryTypeCount; ++i) {
        if (!(type_filter & (1U << i))) {
            continue;
        }

        VkMemoryPropertyFlags type_properties =
            mem_properties.memoryTypes[i].propertyFlags;
        if ((type_properties & properties) == properties) {
            return (MemoryTypeIndexResult){ .payload = i };
        }
    }

    return (MemoryTypeIndexResult){ .error = HT_ERROR_FIND_MEMORY_TYPE };
}

static int create_buffer(
    HelloTriangleApp *app,
    VkDeviceSize size,
    VkBufferUsageFlags usage,
    VkMemoryPropertyFlags properties,
    VkBuffer *buffer,
    VkDeviceMemory *buffer_memory
) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateBuffer(
        app->device,
        &buffer_info,
        NULL,
        buffer
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_BUFFER_CREATE;
    }

    VkMemoryRequirements mem_requirements;
    vkGetBufferMemoryRequirements(
        app->device,
        *buffer,
        &mem_requirements
    );

    uint32_t memory_type_index;
    {
        MemoryTypeIndexResult memory_type_index_result = find_memory_type(
            app,
            mem_requirements.memoryTypeBits,
            properties
        );
        if (memory_type_index_result.error != 0) {
            return memory_type_index_result.error;
        }

        memory_type_index = memory_type_index_result.payload;
    }

    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_requirements.size,
        .memoryTypeIndex = memory_type_index,
    };

    result = vkAllocateMemory(
        app->device,
        &alloc_info,
        NULL,
        buffer_memory
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_BUFFER_MEMORY;
    }

    vkBindBufferMemory(
        app->device,
        *buffer,
        *buffer_memory,
        0
    );

    return 0;
}

static int copy_buffer(
    HelloTriangleApp *app,
    VkBuffer dst_buffer,
    VkBuffer src_buffer,
    VkDeviceSize size
) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandPool = app->command_pool,
        .commandBufferCount = 1,
    };

    VkCommandBuffer command_buffer; 
    VkResult result = vkAllocateCommandBuffers(
        app->device,
        &alloc_info,
        &command_buffer
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_COPY_BUFFER_ALLOCATE;
    }

    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
    };
    vkBeginCommandBuffer(command_buffer, &begin_info);

    VkBufferCopy copy_region = {
        .srcOffset = 0,
        .dstOffset = 0,
        .size = size
    };
    vkCmdCopyBuffer(command_buffer, src_buffer, dst_buffer, 1, &copy_region);

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return HT_ERROR_COPY_BUFFER_COMMAND;
    }

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &command_buffer,
    };
    result = vkQueueSubmit(
        app->graphics_queue,
        1,
        &submit_info,
        VK_NULL_HANDLE
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_COPY_BUFFER_SUBMIT;
    }

    vkQueueWaitIdle(app->graphics_queue);

    vkFreeCommandBuffers(app->device, app->command_pool, 1, &command_buffer);

    return 0;
}

static int create_vertex_buffer(HelloTriangleApp *app) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    int error = create_buffer(
        app,
        sizeof(VERTICES),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer,
        &staging_buffer_memory
    );
    if (error != 0) {
        return 0;
    }

    {
        void *buffer;
        vkMapMemory(
            app->device,
            staging_buffer_memory,
            0,
            sizeof(VERTICES),
            0,
            &buffer
        );
        memcpy(buffer, VERTICES, (size_t)sizeof(VERTICES));
        vkUnmapMemory(app->device, staging_buffer_memory);
    }

    create_buffer(
        app,
        sizeof(VERTICES),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &app->vertex_buffer,
        &app->vertex_buffer_memory
    );

    error = copy_buffer(
        app,
        app->vertex_buffer,
        staging_buffer,
        sizeof(VERTICES)
    );
    if (error != 0) {
        return error;
    }

    vkDestroyBuffer(app->device, staging_buffer, NULL);
    vkFreeMemory(app->device, staging_buffer_memory, NULL);

    return 0;
}

static int create_index_buffer(HelloTriangleApp *app) {
    VkBuffer staging_buffer;
    VkDeviceMemory staging_buffer_memory;

    int error = create_buffer(
        app,
        sizeof(INDICES),
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
            VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
        &staging_buffer,
        &staging_buffer_memory
    );
    if (error != 0) {
        return error;
    }

    {
        void *data;
        vkMapMemory(
            app->device,
            staging_buffer_memory,
            0,
            sizeof(INDICES),
            0,
            &data
        );
        memcpy(data, INDICES, sizeof(INDICES));
        vkUnmapMemory(app->device, staging_buffer_memory);
    }

    error = create_buffer(
        app,
        sizeof(INDICES),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT |
            VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
        &app->index_buffer,
        &app->index_buffer_memory
    );
    if (error != 0) {
        return error;
    }

    error = copy_buffer(
        app,
        app->index_buffer,
        staging_buffer,
        sizeof(INDICES)
    );
    if (error != 0) {
        return error;
    }

    vkDestroyBuffer(app->device, staging_buffer, NULL);
    vkFreeMemory(app->device, staging_buffer_memory, NULL);

    return 0;
}

static int create_command_buffers(HelloTriangleApp *app) {
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = app->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = countof(app->command_buffers),
    };

    VkResult result = vkAllocateCommandBuffers(
        app->device,
        &alloc_info,
        app->command_buffers
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_COMMAND_BUFFER;
    }

    return 0;
}

static int create_sync_objects(HelloTriangleApp *app) {
    VkSemaphoreCreateInfo semaphore_info = {
        .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
    };

    VkFenceCreateInfo fence_info = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
        .flags = VK_FENCE_CREATE_SIGNALED_BIT,
    };

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkResult result = vkCreateSemaphore(
            app->device,
            &semaphore_info,
            NULL,
            &app->image_available_semaphores[i]
        );
        if (result != VK_SUCCESS) {
            return HT_ERROR_CREATE_SYNC_OBJECTS_AVAILABLE;
        }
        
        result = vkCreateSemaphore(
            app->device,
            &semaphore_info,
            NULL,
            &app->render_finished_semaphores[i]
        );
        if (result != VK_SUCCESS) {
            return HT_ERROR_CREATE_SYNC_OBJECTS_FINISHED;
        }

        result = vkCreateFence(
            app->device,
            &fence_info,
            NULL,
            &app->in_flight_fences[i]
        );
        if (result != VK_SUCCESS) {
            return HT_ERROR_CREATE_SYNC_OBJECTS_FENCE;
        }
    }

    return 0;
}

static int record_command_buffer(
    HelloTriangleApp *app,
    VkCommandBuffer command_buffer,
    uint32_t image_index
) {
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = 0,
        .pInheritanceInfo = NULL,
    };
    
    VkResult result = vkBeginCommandBuffer(command_buffer, &begin_info);
    if (result != VK_SUCCESS) {
        return HT_ERROR_RECORD_COMMAND_BUFFER_BEGIN;
    }

    VkImageMemoryBarrier image_memory_barrier_optimal = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .image = slice_get(app->swapchain_images, image_index),
        .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &image_memory_barrier_optimal
    );

    VkClearValue clear_color = {
        .color = {
            .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };

    VkRenderingAttachmentInfoKHR color_attachment_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO_KHR,
        .imageView = slice_get(app->swapchain_image_views, image_index),
        .imageLayout = VK_IMAGE_LAYOUT_ATTACHMENT_OPTIMAL_KHR,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_color,
    };

    VkRenderingInfoKHR render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = app->swapchain_extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_info,
    };

    vkCmdBeginRenderingKHR(command_buffer, &render_info);

    vkCmdBindPipeline(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        app->graphics_pipeline
    );

    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)app->swapchain_extent.width,
        .height = (float)app->swapchain_extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(command_buffer, 0, 1, &viewport);

    VkRect2D scissor = {
        .offset = { 0, 0 },
        .extent = app->swapchain_extent,
    };
    vkCmdSetScissor(command_buffer, 0, 1, &scissor);

    VkBuffer vertex_buffers[] = { app->vertex_buffer };
    VkDeviceSize offsets[] = { 0 };
    assert(countof(vertex_buffers) == countof(offsets));

    vkCmdBindVertexBuffers(
        command_buffer,
        0,
        countof(vertex_buffers),
        vertex_buffers,
        offsets
    );
    
    vkCmdBindIndexBuffer(
        command_buffer,
        app->index_buffer,
        0,
        VK_INDEX_TYPE_UINT16
    );

    vkCmdDrawIndexed(command_buffer, countof(INDICES), 1, 0, 0, 0);

    vkCmdEndRendering(command_buffer);
 
    VkImageMemoryBarrier image_memory_barrier_present = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .image = slice_get(app->swapchain_images, image_index),
        .subresourceRange = {
          .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
          .baseMipLevel = 0,
          .levelCount = 1,
          .baseArrayLayer = 0,
          .layerCount = 1,
        },
    };

    vkCmdPipelineBarrier(
        command_buffer,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        0,
        0,
        NULL,
        0,
        NULL,
        1,
        &image_memory_barrier_present
    );

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return HT_ERROR_RECORD_COMMAND_BUFFER_END;
    }

    return 0;
}

static void cleanup_swapchain(HelloTriangleApp *app) {
    for (size_t i = 0; i < app->swapchain_image_views.len; ++i) {
        vkDestroyImageView(
            app->device,
            slice_get(app->swapchain_image_views, i),
            NULL
        );
    }

    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
}

static int recreate_swapchain(
    HelloTriangleApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    int width = 0;
    int height = 0;
    glfwGetFramebufferSize(app->window, &width, &height);
    while (width == 0 or height == 0) {
        if (glfwWindowShouldClose(app->window)) {
            return 0;
        }
        glfwGetFramebufferSize(app->window, &width, &height);
        glfwWaitEvents();
    }

    vkDeviceWaitIdle(app->device);

    cleanup_swapchain(app);
    *swapchain_arena = app->base_swapchain_arena;

    int error = create_swapchain(app, swapchain_arena, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_image_views(app, swapchain_arena);
    if (error != 0) {
        return error;
    }

    return 0;
}

static int init_vulkan(
    HelloTriangleApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        return HT_ERROR_INIT_VULKAN_VOLK;
    }

    int error = create_instance(app, temp_arena);
    if (error != 0) {
        return error;
    }

#ifdef ENABLE_VALIDATION_LAYERS
    error = setup_debug_messenger(app);
    if (error != 0) {
        return error;
    }
#endif // ENABLE_VALIDATION_LAYERS
    
    error = create_surface(app);
    if (error != 0) {
        return error;
    }

    error = pick_physical_device(app, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_logical_device(app, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_swapchain(app, swapchain_arena, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_image_views(app, swapchain_arena);
    if (error != 0) {
        return error;
    }

    error = create_graphics_pipeline(app, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_command_pool(app, temp_arena);
    if (error != 0) {
        return error;
    }
    
    error = create_vertex_buffer(app);
    if (error != 0) {
        return error;
    }

    error = create_index_buffer(app);
    if (error != 0) {
        return error;
    }

    error = create_command_buffers(app);
    if (error != 0) {
        return error;
    }

    error = create_sync_objects(app);
    if (error != 0) {
        return error;
    }

    return 0;
}

static int draw_frame(
    HelloTriangleApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    vkWaitForFences(
        app->device,
        1,
        &app->in_flight_fences[app->current_frame],
        VK_TRUE,
        UINT64_MAX
    );

    uint32_t image_index;
    VkResult result = vkAcquireNextImageKHR(
        app->device,
        app->swapchain,
        UINT64_MAX,
        app->image_available_semaphores[app->current_frame],
        VK_NULL_HANDLE,
        &image_index
    );
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        return recreate_swapchain(app, swapchain_arena, temp_arena);
    } else if (result != VK_SUCCESS and result != VK_SUBOPTIMAL_KHR) {
        return HT_ERROR_DRAW_FRAME_SWAPCHAIN;
    }

    vkResetFences(app->device, 1, &app->in_flight_fences[app->current_frame]);

    vkResetCommandBuffer(app->command_buffers[app->current_frame], 0);
    int error = record_command_buffer(
        app,
        app->command_buffers[app->current_frame],
        image_index
    );
    if (error != 0) {
        return error;
    }

    VkSemaphore wait_semaphores[] = {
        app->image_available_semaphores[app->current_frame]
    };
    VkPipelineStageFlags wait_stages[] = {
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT
    };
    assert(countof(wait_semaphores) == countof(wait_stages));

    VkSemaphore signal_semaphores[] = {
        app->render_finished_semaphores[app->current_frame]
    };

    VkSubmitInfo submit_info = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = countof(wait_semaphores),
        .pWaitSemaphores = wait_semaphores,
        .pWaitDstStageMask = wait_stages,
        .commandBufferCount = 1,
        .pCommandBuffers = &app->command_buffers[app->current_frame],
        .signalSemaphoreCount = countof(signal_semaphores),
        .pSignalSemaphores = signal_semaphores,
    };

    result = vkQueueSubmit(
        app->graphics_queue,
        1,
        &submit_info,
        app->in_flight_fences[app->current_frame]
    );
    if (result != VK_SUCCESS) {
        return HT_ERRROR_DRAW_FRAME_SUBMIT;
    }

    VkSwapchainKHR swapchain = app->swapchain;

    VkPresentInfoKHR present_info = {
        .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
        .waitSemaphoreCount = countof(signal_semaphores),
        .pWaitSemaphores = signal_semaphores,
        .swapchainCount = 1,
        .pSwapchains = &swapchain,
        .pImageIndices = &image_index,
        .pResults = NULL,
    };

    app->current_frame = (app->current_frame + 1) % MAX_FRAMES_IN_FLIGHT;

    result = vkQueuePresentKHR(app->present_queue, &present_info);
    if (
        result == VK_ERROR_OUT_OF_DATE_KHR or
        result == VK_SUBOPTIMAL_KHR or
        app->framebuffer_resized
    ) {
        app->framebuffer_resized = false;
        return recreate_swapchain(app, swapchain_arena, temp_arena);
    } else if (result != VK_SUCCESS) {
        return HT_ERROR_DRAW_FRAME_SWAPCHAIN;
    }
 
    return 0;
}

static int main_loop(
    HelloTriangleApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
        draw_frame(app, swapchain_arena, temp_arena);
    }

    vkDeviceWaitIdle(app->device);

    return 0;
}

static void cleanup(HelloTriangleApp *app) {
    cleanup_swapchain(app);

    vkDestroyBuffer(app->device, app->index_buffer, NULL);
    vkFreeMemory(app->device, app->index_buffer_memory, NULL);

    vkDestroyBuffer(app->device, app->vertex_buffer, NULL);
    vkFreeMemory(app->device, app->vertex_buffer_memory, NULL);

    vkDestroyPipeline(app->device, app->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroySemaphore(
            app->device,
            app->image_available_semaphores[i],
            NULL
        );
        vkDestroySemaphore(
            app->device,
            app->render_finished_semaphores[i],
            NULL
        );
        vkDestroyFence(app->device, app->in_flight_fences[i], NULL);
    }

    vkDestroyCommandPool(app->device, app->command_pool, NULL);

    vkDestroyDevice(app->device, NULL);

    vkDestroySurfaceKHR(app->instance, app->surface, NULL);

#ifdef ENABLE_VALIDATION_LAYERS
    vkDestroyDebugUtilsMessengerEXT(
        app->instance,
        app->debug_messenger,
        NULL
    );
#endif // ENABLE_VALIDATION_LAYERS

    vkDestroyInstance(app->instance, NULL);

    glfwDestroyWindow(app->window);
    glfwTerminate();
}

static int run(HelloTriangleApp *app, Arena temp_arena) {
    Arena swapchain_arena = arena_init(
        arena_alloc(&temp_arena, ARENA_SIZE, 1),
        ARENA_SIZE
    );
    assert(swapchain_arena.base != NULL);

    app->base_swapchain_arena = swapchain_arena;

    int error = init_window(app);
    if (error != 0) {
        return error;
    }

    error = init_vulkan(app, &swapchain_arena, temp_arena);
    if (error != 0) {
        return error;
    }

    error = main_loop(app, &swapchain_arena, temp_arena);
    if (error != 0) {
        return error;
    }

    cleanup(app);

    return 0;
}

int main(void) {
    Arena arena = arena_init(
        malloc(2 * ARENA_SIZE),
        2 * ARENA_SIZE
    );
    if (arena.base == NULL) {
        return HT_ERROR_MAIN_MALLOC;
    }

    HelloTriangleApp app = {
        .width = 480,
        .height = 480,
        .physical_device = VK_NULL_HANDLE,
    };

    int error = run(&app, arena);

    free(arena.base);

    return error;
}

