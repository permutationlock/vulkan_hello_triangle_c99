#define _POSIX_C_SOURCE 200809L

#if __STDC_VERSION__ < 201112L
    #define AVEN_MAX_ALIGNMENT 16
#endif
#include "aven.h"
#include "aven_glm.h"
#include "aven_time.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <unistd.h>

#define VOLK_IMPLEMENTATION
#include <volk.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define MASTER_ARENA_SIZE 1024 * 1024 * 8
#define SWAPCHAIN_ARENA_SIZE 1024
#define MAX_FRAMES_IN_FLIGHT 2
#define GAME_TIMESTEP_NS (4L * 1000L * 1000L)

#ifdef ENABLE_VALIDATION_LAYERS
const char *VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};
#endif // ENABLE_VALIDATION_LAYERS

const char *DEVICE_EXTENSIONS[] = {
    "VK_KHR_swapchain",
#ifdef ENABLE_VALIDATION_LAYERS
    "VK_KHR_shader_non_semantic_info",
#endif
};

typedef struct {
    Mat2 view;
} UniformBufferObject;

typedef struct { Vec2 pos; Vec4 color; } Vertex;

const Vertex VERTICES[] = {
    { .pos = { { -0.5f, -0.5f } }, .color = { { 1.0f, 0.0f, 0.0f } } },
    { .pos = { {  0.5f, -0.5f } }, .color = { { 0.0f, 1.0f, 0.0f } } },
    { .pos = { {  0.5f,  0.5f } }, .color = { { 0.0f, 0.0f, 1.0f } } },
    { .pos = { { -0.5f,  0.5f } }, .color = { { 1.0f, 1.0f, 1.0f } } },
};

typedef uint16_t Index;

const Index INDICES[] = { 0, 1, 2, 2, 3, 0 };

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
    float rotation_angle;
    bool done;
    pthread_mutex_t mutex;
} GameData;

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

    VkImage color_image;
    VkDeviceMemory color_image_memory;
    VkImageView color_image_view;

    VkDescriptorSetLayout descriptor_set_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkBuffer vertex_buffer;
    VkDeviceMemory vertex_buffer_memory;
    VkBuffer index_buffer;
    VkDeviceMemory index_buffer_memory;

    VkBuffer uniform_buffers[MAX_FRAMES_IN_FLIGHT];
    VkDeviceMemory uniform_buffers_memory[MAX_FRAMES_IN_FLIGHT];
    void *uniform_buffers_mapped[MAX_FRAMES_IN_FLIGHT];

    VkDescriptorPool descriptor_pool;
    VkDescriptorSet descriptor_sets[MAX_FRAMES_IN_FLIGHT];

    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

    VkSampleCountFlagBits msaa_samples;

    Arena base_swapchain_arena;
    uint32_t current_frame;
    bool framebuffer_resized;

    GameData game_data;
} VulkanApp;

typedef enum {
    APP_ERROR_NONE = 0,
    APP_ERROR_MAIN_MALLOC,
    APP_ERROR_INIT_WINDOW,
    APP_ERROR_INIT_VULKAN_VOLK,
    APP_ERROR_CREATE_INSTANCE_ALLOC,
    APP_ERROR_CREATE_INSTANCE_CREATE,
    APP_ERROR_CREATE_SURFACE,
    APP_ERROR_FIND_QUEUE_FAMILIES_ALLOC,
    APP_ERROR_CHECK_DEVICE_EXTENSION_SUPPORT_ALLOC,
    APP_ERROR_QUERY_SWAP_CHAIN_SUPPORT_ALLOC,
    APP_ERROR_PICK_PHYSICAL_DEVICE_ENUMERATE,
    APP_ERROR_PICK_PHYSICAL_DEVICE_ALLOC,
    APP_ERROR_PICK_PHYSICAL_DEVICE_SUITABLE,
    APP_ERROR_CREATE_LOGICAL_DEVICE,
    APP_ERROR_CREATE_SWAP_CHAIN_CREATE,
    APP_ERROR_CREATE_SWAP_CHAIN_ALLOC,
    APP_ERROR_CREATE_IMAGE_VIEWS_ALLOC,
    APP_ERROR_CREATE_IMAGE_VIEWS_CREATE,
    APP_ERROR_READ_FILE_OPEN,
    APP_ERROR_READ_FILE_SEEK,
    APP_ERROR_READ_FILE_TELL,
    APP_ERROR_READ_FILE_ALLOC,
    APP_ERROR_READ_FILE_READ,
    APP_ERROR_CREATE_SHADER_MODULE,
    APP_ERROR_CREATE_RENDER_PASS,
    APP_ERROR_CREATE_GRAPHICS_PIPELINE_LAYOUT,
    APP_ERROR_CREATE_GRAPHICS_PIPELINE_CREATE,
    APP_ERROR_CREATE_FRAMEBUFFER_ALLOC,
    APP_ERROR_CREATE_FRAMEBUFFER_CREATE,
    APP_ERROR_CREATE_COMMAND_POOL,
    APP_ERROR_CREATE_COMMAND_BUFFER,
    APP_ERROR_RECORD_COMMAND_BUFFER_BEGIN,
    APP_ERROR_RECORD_COMMAND_BUFFER_END,
    APP_ERROR_CREATE_SYNC_OBJECTS_AVAILABLE,
    APP_ERROR_CREATE_SYNC_OBJECTS_FINISHED,
    APP_ERROR_CREATE_SYNC_OBJECTS_FENCE,
    APP_ERROR_DRAW_FRAME_SWAPCHAIN,
    APP_ERROR_DRAW_FRAME_SUBMIT,
    APP_ERROR_FIND_MEMORY_TYPE,
    APP_ERROR_CREATE_BUFFER_CREATE,
    APP_ERROR_CREATE_BUFFER_MEMORY,
    APP_ERROR_COPY_BUFFER_ALLOCATE,
    APP_ERROR_COPY_BUFFER_COMMAND,
    APP_ERROR_COPY_BUFFER_SUBMIT,
    APP_ERROR_CREATE_DESCRIPTOR_SET_LAYOUT,
    APP_ERROR_CREATE_DESCRIPTOR_POOL,
    APP_ERROR_CREATE_DESCRIPTOR_SETS,
    APP_ERROR_CREATE_COLOR_RESOURCES_CREATE,
    APP_ERROR_CREATE_COLOR_RESOURCES_ALLOC,
    APP_ERROR_MAIN_LOOP_THREAD,
    APP_ERROR_MAIN_LOOP_JOIN,
    APP_ERROR_RUN_TIME,
#ifdef ENABLE_VALIDATION_LAYERS
    APP_ERROR_CHECK_VALIDATION_LAYER_SUPPORT_ALLOC,
    APP_ERROR_SETUP_DEBUG_MESSENGER,
    APP_ERROR_CREATE_INSTANCE_VALIDATION,
#endif // ENABLE_VALIDATION_LAYERS
} VulkanAppError;

void framebuffer_resize_callback(GLFWwindow *window, int width, int height) {
    (void)width;
    (void)height;

    VulkanApp *app = glfwGetWindowUserPointer(window);
    app->framebuffer_resized = true;
}

static int init_window(VulkanApp *app) {
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
        return APP_ERROR_INIT_WINDOW;
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
            .error = APP_ERROR_CHECK_VALIDATION_LAYER_SUPPORT_ALLOC
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
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT,
        .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
            VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
        .pfnUserCallback = debug_callback,
        .pUserData = NULL,
    };
}

static int setup_debug_messenger(VulkanApp *app) {
    VkDebugUtilsMessengerCreateInfoEXT create_info =
        debug_messenger_create_info();

    VkResult result = vkCreateDebugUtilsMessengerEXT(
        app->instance,
        &create_info,
        NULL,
        &app->debug_messenger
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_SETUP_DEBUG_MESSENGER;
    }
    
    return 0;
}
#endif // ENABLE_VALIDATION_LAYERS

static int create_instance(VulkanApp *app, Arena temp_arena) {
#ifdef ENABLE_VALIDATION_LAYERS
    {
        BoolResult layer_support_result = check_validation_layer_support(
            temp_arena
        );
        if (layer_support_result.error != 0) {
            return layer_support_result.error;
        }

        if (!layer_support_result.payload) {
            return APP_ERROR_CREATE_INSTANCE_VALIDATION;
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
        return APP_ERROR_CREATE_INSTANCE_ALLOC;
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
        return APP_ERROR_CREATE_INSTANCE_CREATE;
    }

    volkLoadInstanceOnly(app->instance);

    return 0;
}

static int create_surface(VulkanApp *app) {
    VkResult result = glfwCreateWindowSurface(
        app->instance,
        app->window,
        NULL,
        &app->surface
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_SURFACE;
    }

    return 0;
}

typedef struct {
    Optional(uint32_t) graphics_family;
    Optional(uint32_t) present_family;
} QueueFamilyIndices;

typedef Result(QueueFamilyIndices) QueueFamilyIndicesResult;

static QueueFamilyIndicesResult find_queue_families(
    VulkanApp *app,
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
            .error = APP_ERROR_FIND_QUEUE_FAMILIES_ALLOC
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
            .error = APP_ERROR_CHECK_DEVICE_EXTENSION_SUPPORT_ALLOC
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
    VulkanApp *app,
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
                .error = APP_ERROR_QUERY_SWAP_CHAIN_SUPPORT_ALLOC
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
                .error = APP_ERROR_QUERY_SWAP_CHAIN_SUPPORT_ALLOC
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
    VulkanApp *app,
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

static VkSampleCountFlagBits get_max_sample_count(VulkanApp *app) {
    VkPhysicalDeviceProperties physical_device_properties;
    vkGetPhysicalDeviceProperties(
        app->physical_device,
        &physical_device_properties
    );

    VkSampleCountFlags counts =
        physical_device_properties.limits.framebufferColorSampleCounts &
        physical_device_properties.limits.framebufferDepthSampleCounts;

    if (counts & VK_SAMPLE_COUNT_64_BIT) { return VK_SAMPLE_COUNT_64_BIT; }
    if (counts & VK_SAMPLE_COUNT_32_BIT) { return VK_SAMPLE_COUNT_32_BIT; }
    if (counts & VK_SAMPLE_COUNT_16_BIT) { return VK_SAMPLE_COUNT_16_BIT; }
    if (counts & VK_SAMPLE_COUNT_8_BIT) { return VK_SAMPLE_COUNT_8_BIT; }
    if (counts & VK_SAMPLE_COUNT_4_BIT) { return VK_SAMPLE_COUNT_4_BIT; }
    if (counts & VK_SAMPLE_COUNT_2_BIT) { return VK_SAMPLE_COUNT_2_BIT; }

    return VK_SAMPLE_COUNT_1_BIT;
}

static int pick_physical_device(VulkanApp *app, Arena temp_arena) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);
    if (device_count == 0) {
        return APP_ERROR_PICK_PHYSICAL_DEVICE_ENUMERATE;
    }

    VkPhysicalDevice *devices = arena_create_array(
        VkPhysicalDevice,
        &temp_arena,
        device_count
    );
    if (devices == NULL) {
        return APP_ERROR_PICK_PHYSICAL_DEVICE_ALLOC;
    }

    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    for (uint32_t i = 0; i < device_count; ++i) {
        BoolResult result = is_device_suitable(app, devices[i], temp_arena);
        if (result.error != 0) {
            return result.error;
        }

        if (result.payload) {
            app->physical_device = devices[i];
            app->msaa_samples = get_max_sample_count(app);
            break;
        }
    }

    if (app->physical_device == VK_NULL_HANDLE) {
        return APP_ERROR_PICK_PHYSICAL_DEVICE_SUITABLE;
    }

    return 0;
}

static int create_logical_device(VulkanApp *app, Arena temp_arena) {
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

    VkPhysicalDeviceScalarBlockLayoutFeatures scalar_layout_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SCALAR_BLOCK_LAYOUT_FEATURES,
        .scalarBlockLayout = VK_TRUE,
    };

    VkPhysicalDeviceDynamicRenderingFeatures dynamic_rendering_features = {
        .sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_DYNAMIC_RENDERING_FEATURES,
        .pNext = &scalar_layout_features,
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
        return APP_ERROR_CREATE_LOGICAL_DEVICE;
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
        if (present_mode == VK_PRESENT_MODE_FIFO_KHR) {
            return present_mode;
        }
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

static VkExtent2D choose_swap_extent(
    VulkanApp *app,
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
    VulkanApp *app,
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
        .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_TRANSFER_DST_BIT,
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
        return APP_ERROR_CREATE_SWAP_CHAIN_CREATE;
    }

    vkGetSwapchainImagesKHR(app->device, app->swapchain, &image_count, NULL);

    app->swapchain_images.ptr = arena_create_array(
        VkImage,
        swapchain_arena,
        image_count
    );
    if (app->swapchain_images.ptr == NULL) {
        return APP_ERROR_CREATE_SWAP_CHAIN_ALLOC;
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

static int create_image_views(VulkanApp *app, Arena *swapchain_arena) {
    app->swapchain_image_views.ptr = arena_create_array(
        VkImageView,
        swapchain_arena,
        app->swapchain_images.len
    );
    app->swapchain_image_views.len = app->swapchain_images.len;
    if (app->swapchain_image_views.ptr == NULL) {
        return APP_ERROR_CREATE_IMAGE_VIEWS_ALLOC;
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
            return APP_ERROR_CREATE_IMAGE_VIEWS_CREATE;
        }
    }

    return 0;
}

typedef Result(ByteSlice) ByteSliceResult;

static ByteSliceResult read_file(
    const char *filename,
    Arena *perm_arena,
    size_t alignment
) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return (ByteSliceResult){ .error = APP_ERROR_READ_FILE_OPEN };
    }

    int error = fseek(file, 0L, SEEK_END);
    if (error != 0) {
        fclose(file);
        return (ByteSliceResult){ .error = APP_ERROR_READ_FILE_SEEK };
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return (ByteSliceResult){ .error = APP_ERROR_READ_FILE_TELL };
    }

    rewind(file);

    ByteSlice bytes = {
        .ptr = arena_alloc(perm_arena, (size_t)len, alignment),
        .len = (size_t)len,
    };
    if (bytes.ptr == NULL) {
        fclose(file);
        return (ByteSliceResult){ .error = APP_ERROR_READ_FILE_ALLOC };
    }
    
    size_t bytes_read = fread(bytes.ptr, 1, bytes.len, file);

    fclose(file);

    if (bytes_read != bytes.len) {
        return (ByteSliceResult){ .error = APP_ERROR_READ_FILE_READ };
    }

    return (ByteSliceResult){ .payload = bytes };
}

typedef Result(VkShaderModule) VkShaderModuleResult;

static VkShaderModuleResult create_shader_module(
    VulkanApp *app,
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
            .error = APP_ERROR_CREATE_SHADER_MODULE
        };
    }

    return (VkShaderModuleResult){ .payload = shader_module };
}

static int create_descriptor_set_layout(VulkanApp *app) {
    VkDescriptorSetLayoutBinding ubo_layout_binding = {
        .binding = 0,
        .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = 1,
        .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
    };
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 1,
        .pBindings = &ubo_layout_binding,
    };

    VkResult result = vkCreateDescriptorSetLayout(
        app->device,
        &layout_info,
        NULL,
        &app->descriptor_set_layout
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_DESCRIPTOR_SET_LAYOUT;
    }

    return 0;
}

static int create_graphics_pipeline(VulkanApp *app, Arena temp_arena) {
    ByteSlice vert_shader_code;
    {
        ByteSliceResult result = read_file(
            "shaders/vert.spv",
            &temp_arena,
            alignof(uint32_t)
        );
        if (result.error != 0) {
            return result.error;
        }

        vert_shader_code = result.payload;
    }

    ByteSlice frag_shader_code;
    {
        ByteSliceResult result = read_file(
            "shaders/frag.spv",
            &temp_arena,
            alignof(uint32_t)
        );
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
        .rasterizationSamples = app->msaa_samples,
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
        .setLayoutCount = 1,
        .pSetLayouts = &app->descriptor_set_layout,
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
        return APP_ERROR_CREATE_GRAPHICS_PIPELINE_LAYOUT;
    }

    VkFormat attachment_formats[] = {
        app->swapchain_image_format
    };

    VkPipelineRenderingCreateInfo pipeline_rendering_create_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO,
        .colorAttachmentCount = countof(attachment_formats),
        .pColorAttachmentFormats = attachment_formats,
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
        return APP_ERROR_CREATE_GRAPHICS_PIPELINE_CREATE;
    }

    vkDestroyShaderModule(app->device, frag_shader_module, NULL);
    vkDestroyShaderModule(app->device, vert_shader_module, NULL);

    return 0;
}

static int create_command_pool(VulkanApp *app, Arena temp_arena) {
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
        return APP_ERROR_CREATE_COMMAND_POOL;
    }

    return 0;
}

typedef Result(uint32_t) MemoryTypeIndexResult;

static MemoryTypeIndexResult find_memory_type(
    VulkanApp *app,
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

    return (MemoryTypeIndexResult){ .error = APP_ERROR_FIND_MEMORY_TYPE };
}

static int create_color_resources(VulkanApp *app) {
    VkImageCreateInfo image_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
        .imageType = VK_IMAGE_TYPE_2D,
        .extent.width = app->swapchain_extent.width,
        .extent.height = app->swapchain_extent.height,
        .extent.depth = 1,
        .mipLevels = 1,
        .arrayLayers = 1,
        .format = app->swapchain_image_format,
        .tiling = VK_IMAGE_TILING_OPTIMAL,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .usage = VK_IMAGE_USAGE_TRANSIENT_ATTACHMENT_BIT |
            VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        .samples = app->msaa_samples,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
    };

    VkResult result = vkCreateImage(
        app->device,
        &image_info,
        NULL,
        &app->color_image
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_COLOR_RESOURCES_CREATE;
    }

    VkMemoryRequirements mem_requirements;
    vkGetImageMemoryRequirements(
        app->device,
        app->color_image,
        &mem_requirements
    );

    uint32_t memory_type_index;
    {
        MemoryTypeIndexResult memory_type_index_result = find_memory_type(
            app,
            mem_requirements.memoryTypeBits,
            VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT
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
        &app->color_image_memory
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_COLOR_RESOURCES_ALLOC;
    }

    vkBindImageMemory(
        app->device,
        app->color_image,
        app->color_image_memory,
        0
    );

    VkImageViewCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
        .image = app->color_image,
        .viewType = VK_IMAGE_VIEW_TYPE_2D,
        .format = app->swapchain_image_format,
        .subresourceRange = {
            .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
            .baseMipLevel = 0,
            .levelCount = 1,
            .baseArrayLayer = 0,
            .layerCount = 1,
        },
    };

    result = vkCreateImageView(
        app->device,
        &create_info,
        NULL,
        &app->color_image_view
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_IMAGE_VIEWS_CREATE;
    }

    return 0;
}

static int create_buffer(
    VulkanApp *app,
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
        return APP_ERROR_CREATE_BUFFER_CREATE;
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
        return APP_ERROR_CREATE_BUFFER_MEMORY;
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
    VulkanApp *app,
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
        return APP_ERROR_COPY_BUFFER_ALLOCATE;
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
        return APP_ERROR_COPY_BUFFER_COMMAND;
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
        return APP_ERROR_COPY_BUFFER_SUBMIT;
    }

    vkQueueWaitIdle(app->graphics_queue);

    vkFreeCommandBuffers(app->device, app->command_pool, 1, &command_buffer);

    return 0;
}

static int create_vertex_buffer(VulkanApp *app) {
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

static int create_index_buffer(VulkanApp *app) {
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

static int create_command_buffers(VulkanApp *app) {
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
        return APP_ERROR_CREATE_COMMAND_BUFFER;
    }

    return 0;
}

static int create_sync_objects(VulkanApp *app) {
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
            return APP_ERROR_CREATE_SYNC_OBJECTS_AVAILABLE;
        }
        
        result = vkCreateSemaphore(
            app->device,
            &semaphore_info,
            NULL,
            &app->render_finished_semaphores[i]
        );
        if (result != VK_SUCCESS) {
            return APP_ERROR_CREATE_SYNC_OBJECTS_FINISHED;
        }

        result = vkCreateFence(
            app->device,
            &fence_info,
            NULL,
            &app->in_flight_fences[i]
        );
        if (result != VK_SUCCESS) {
            return APP_ERROR_CREATE_SYNC_OBJECTS_FENCE;
        }
    }

    return 0;
}

static int create_uniform_buffers(VulkanApp *app) {
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        int error = create_buffer(
            app,
            sizeof(UniformBufferObject),
            VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
                VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
            &app->uniform_buffers[i],
            &app->uniform_buffers_memory[i]
        );
        if (error != 0) {
            return error;
        }

        vkMapMemory(
            app->device,
            app->uniform_buffers_memory[i],
            0,
            sizeof(UniformBufferObject),
            0,
            &app->uniform_buffers_mapped[i]
        );
    }

    return 0;
}

static int create_descriptor_pool(VulkanApp *app) {
    VkDescriptorPoolSize pool_size = {
        .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
        .descriptorCount = MAX_FRAMES_IN_FLIGHT,
    };
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size,
        .maxSets = MAX_FRAMES_IN_FLIGHT,
    };

    VkResult result = vkCreateDescriptorPool(
        app->device,
        &pool_info,
        NULL,
        &app->descriptor_pool
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_DESCRIPTOR_POOL;
    }

    return 0;
}

static int create_descriptor_sets(VulkanApp *app) {
    VkDescriptorSetLayout layouts[MAX_FRAMES_IN_FLIGHT];
    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        layouts[i] = app->descriptor_set_layout;
    }

    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = app->descriptor_pool,
        .descriptorSetCount = MAX_FRAMES_IN_FLIGHT,
        .pSetLayouts = layouts,
    };

    VkResult result = vkAllocateDescriptorSets(
        app->device,
        &alloc_info,
        app->descriptor_sets
    );
    if (result != VK_SUCCESS) {
        return APP_ERROR_CREATE_DESCRIPTOR_SETS;
    }

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        VkDescriptorBufferInfo buffer_info = {
            .buffer = app->uniform_buffers[i],
            .offset = 0,
            .range = sizeof(UniformBufferObject),
        };
        
        VkWriteDescriptorSet descriptor_write = {
            .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
            .dstSet = app->descriptor_sets[i],
            .dstBinding = 0,
            .dstArrayElement = 0,
            .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
            .descriptorCount = 1,
            .pBufferInfo = &buffer_info,
        };

        vkUpdateDescriptorSets(app->device, 1, &descriptor_write, 0, NULL);
    }

    return 0;
}

static int record_command_buffer(
    VulkanApp *app,
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
        return APP_ERROR_RECORD_COMMAND_BUFFER_BEGIN;
    }

    {
        VkImageMemoryBarrier image_memory_barriers[] = {
            {
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
            }
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
            countof(image_memory_barriers),
            image_memory_barriers
        );
    }

    VkClearValue clear_color = {
        .color = {
            .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };

    VkRenderingAttachmentInfo attachment = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_ATTACHMENT_INFO,
        .imageLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .clearValue = clear_color,
    };
    if (app->msaa_samples == VK_SAMPLE_COUNT_1_BIT) {
        attachment.imageView = slice_get(
            app->swapchain_image_views,
            image_index
        );
    } else {
        attachment.imageView = app->color_image_view;
        attachment.resolveMode = VK_RESOLVE_MODE_AVERAGE_BIT;
        attachment.resolveImageView = slice_get(
            app->swapchain_image_views,
            image_index
        );
        attachment.resolveImageLayout =
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    }

    VkRenderingInfo render_info = {
        .sType = VK_STRUCTURE_TYPE_RENDERING_INFO,
        .renderArea = {
            .offset = { 0, 0 },
            .extent = app->swapchain_extent,
        },
        .layerCount = 1,
        .colorAttachmentCount = 1,
        .pColorAttachments = &attachment,
    };

    vkCmdBeginRendering(command_buffer, &render_info);

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

    assert(sizeof(*INDICES) == sizeof(uint16_t));
    vkCmdBindIndexBuffer(
        command_buffer,
        app->index_buffer,
        0,
        VK_INDEX_TYPE_UINT16
    );

    vkCmdBindDescriptorSets(
        command_buffer,
        VK_PIPELINE_BIND_POINT_GRAPHICS,
        app->pipeline_layout,
        0,
        1,
        &app->descriptor_sets[app->current_frame],
        0,
        NULL
    );

    vkCmdDrawIndexed(command_buffer, countof(INDICES), 1, 0, 0, 0);

    vkCmdEndRendering(command_buffer);

    {
        VkImageMemoryBarrier image_memory_barriers[] = {
            {
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
            }
        };

        vkCmdPipelineBarrier(
            command_buffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
            0,
            0,
            NULL,
            0,
            NULL,
            countof(image_memory_barriers),
            image_memory_barriers
        );
    }
 
    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return APP_ERROR_RECORD_COMMAND_BUFFER_END;
    }

    return 0;
}

static void cleanup_swapchain(VulkanApp *app) {
    if (app->msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        vkDestroyImageView(app->device, app->color_image_view, NULL);
        vkDestroyImage(app->device, app->color_image, NULL);
        vkFreeMemory(app->device, app->color_image_memory, NULL);
    }

    for (size_t i = 0; i < app->swapchain_image_views.len; ++i) {
        vkDestroyImageView(
            app->device,
            slice_get(app->swapchain_image_views, i),
            NULL
        );
    }

    vkDestroySwapchainKHR(app->device, app->swapchain, NULL);
}

static void update_uniform_buffer(
    VulkanApp *app,
    uint32_t current_image
) {
    float width = (float)app->swapchain_extent.width;
    float height = (float)app->swapchain_extent.height;
    float side = min(width, height);
    Mat2 view_matrix = { {
        { { side / width, 0.0f } },
        { { 0.0f, side / height } }
    } };

    int error = pthread_mutex_lock(&app->game_data.mutex);
    assert(error == 0);

    float sin_rangle = sinf(app->game_data.rotation_angle);
    float cos_rangle = cosf(app->game_data.rotation_angle);

    error = pthread_mutex_unlock(&app->game_data.mutex);
    assert(error == 0);

    Mat2 rotation_matrix = { {
        { { cos_rangle, -sin_rangle } },
        { { sin_rangle, cos_rangle } }
    } };

    UniformBufferObject ubo = {
        .view = mat2_mul_mat2(view_matrix, rotation_matrix),
    };
    memcpy(app->uniform_buffers_mapped[current_image], &ubo, sizeof(ubo));
}

static int recreate_swapchain(
    VulkanApp *app,
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

    app->width = (uint32_t)width;
    app->height = (uint32_t)height;

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

    if (app->msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        error = create_color_resources(app);
        if (error != 0) {
            return error;
        }
    }

    return 0;
}

static int init_vulkan(
    VulkanApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    VkResult result = volkInitialize();
    if (result != VK_SUCCESS) {
        return APP_ERROR_INIT_VULKAN_VOLK;
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

    error = create_descriptor_set_layout(app);
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

    if (app->msaa_samples != VK_SAMPLE_COUNT_1_BIT) {
        error = create_color_resources(app);
        if (error != 0) {
            return error;
        }
    }
    
    error = create_vertex_buffer(app);
    if (error != 0) {
        return error;
    }

    error = create_index_buffer(app);
    if (error != 0) {
        return error;
    }

    error = create_uniform_buffers(app);
    if (error != 0) {
        return error;
    }

    error = create_descriptor_pool(app);
    if (error != 0) {
        return error;
    }

    error = create_descriptor_sets(app);
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
    VulkanApp *app,
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
        return APP_ERROR_DRAW_FRAME_SWAPCHAIN;
    }

    update_uniform_buffer(app, app->current_frame);

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
        return APP_ERROR_DRAW_FRAME_SUBMIT;
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
        return APP_ERROR_DRAW_FRAME_SWAPCHAIN;
    }
 
    return 0;
}

void game_step_update(GameData *game_data) {
    float fdt = (float)(GAME_TIMESTEP_NS) /
        (1000.0f * 1000.0f * 1000.0f);
    game_data->rotation_angle += fdt * AVEN_GLM_PI_F / 8.0f;
}

void *game_loop(void *arg) {
    GameData *game_data = arg;

    TimeSpec last_update;
    int error = clock_gettime(CLOCK_MONOTONIC, &last_update);
    assert(error == 0);

    int64_t delta_time = 0;

    while (true) {
        TimeSpec current_time;
        error = clock_gettime(CLOCK_MONOTONIC, &current_time);
        assert(error == 0);

        delta_time += timespec_diff(&current_time, &last_update);
        if (delta_time >= GAME_TIMESTEP_NS) {
            error = pthread_mutex_lock(&game_data->mutex);
            assert(error == 0);
            
            if (game_data->done) {
                return NULL;
            }

            do {
                last_update = current_time;
                delta_time -= GAME_TIMESTEP_NS;
                game_step_update(game_data);
            } while (delta_time >= GAME_TIMESTEP_NS);

            error = pthread_mutex_unlock(&game_data->mutex);
            assert(error == 0);
        }

        current_time.tv_nsec += GAME_TIMESTEP_NS - delta_time;
        if (current_time.tv_nsec > 1000L * 1000L * 1000L) {
            current_time.tv_sec += 1;
            current_time.tv_nsec -= 1000L * 1000L * 1000L;
        }

        error = clock_nanosleep(
            CLOCK_MONOTONIC,
            TIMER_ABSTIME,
            &current_time,
            NULL
        );
        assert(error == 0 || error == EINTR);
    }

    return NULL;
}

static int main_loop(
    VulkanApp *app,
    Arena *swapchain_arena,
    Arena temp_arena
) {
    pthread_mutex_init(&app->game_data.mutex, NULL);

    pthread_t game_thread;
    int error = pthread_create(
        &game_thread,
        NULL,
        game_loop,
        &app->game_data
    );
    if (error != 0) {
        return APP_ERROR_MAIN_LOOP_THREAD;
    }

    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
        draw_frame(app, swapchain_arena, temp_arena);
    }

    error = pthread_mutex_lock(&app->game_data.mutex);
    assert(error == 0);

    app->game_data.done = true;

    error = pthread_mutex_unlock(&app->game_data.mutex);
    assert(error == 0);

    vkDeviceWaitIdle(app->device);

    void *rvalue;
    error = pthread_join(game_thread, &rvalue);
    if (error != 0) {
        return APP_ERROR_MAIN_LOOP_JOIN;
    }

    return 0;
}

static void cleanup(VulkanApp *app) {
    cleanup_swapchain(app);

    for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i) {
        vkDestroyBuffer(app->device, app->uniform_buffers[i], NULL);
        vkFreeMemory(app->device, app->uniform_buffers_memory[i], NULL);
    }

    vkDestroyDescriptorPool(app->device, app->descriptor_pool, NULL);

    vkDestroyDescriptorSetLayout(app->device, app->descriptor_set_layout, NULL);

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

static int run(VulkanApp *app, Arena temp_arena) {
    Arena swapchain_arena = arena_init(
        arena_alloc(&temp_arena, SWAPCHAIN_ARENA_SIZE, 1),
        SWAPCHAIN_ARENA_SIZE
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
        malloc(MASTER_ARENA_SIZE),
        MASTER_ARENA_SIZE
    );
    if (arena.base == NULL) {
        return APP_ERROR_MAIN_MALLOC;
    }

    VulkanApp app = {
        .width = 480,
        .height = 480,
    };

    int error = run(&app, arena);

    free(arena.base);

    return error;
}

