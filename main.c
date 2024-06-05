#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <vulkan/vulkan_core.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#define AVEN_MAX_ALIGNMENT 16
#include "aven.h"

#define ARENA_SIZE 1024 * 1024 * 8
#define MAX_FRAMES_IN_FLIGHT 2

const char *VALIDATION_LAYERS[] = {
    "VK_LAYER_KHRONOS_validation"
};

const char *DEVICE_EXTENSIONS[] = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
};

typedef slice(VkImage) VkImage_slice_t;
typedef slice(VkImageView) VkImageView_slice_t;
typedef slice(VkFramebuffer) VkFramebuffer_slice_t;

typedef struct {
    uint32_t width;
    uint32_t height;

    GLFWwindow *window;

    VkInstance instance;

#ifdef ENABLE_VALIDATION_LAYERS
    VkDebugUtilsMessengerEXT debug_messenger;
#endif

    VkSurfaceKHR surface;
    VkPhysicalDevice physical_device;
    VkDevice device;

    VkQueue graphics_queue;
    VkQueue present_queue;

    VkSwapchainKHR swapchain;
    VkImage_slice_t swapchain_images;
    VkImageView_slice_t swapchain_image_views;
    VkFramebuffer_slice_t swapchain_framebuffers;
    VkFormat swapchain_image_format;
    VkExtent2D swapchain_extent;

    VkRenderPass render_pass;
    VkPipelineLayout pipeline_layout;
    VkPipeline graphics_pipeline;

    VkCommandPool command_pool;
    VkCommandBuffer command_buffers[MAX_FRAMES_IN_FLIGHT];

    VkSemaphore image_available_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkSemaphore render_finished_semaphores[MAX_FRAMES_IN_FLIGHT];
    VkFence in_flight_fences[MAX_FRAMES_IN_FLIGHT];

    byte_slice_t base_swapchain_arena;
    uint32_t current_frame;
    bool framebuffer_resized;
} hello_triangle_app_t;

typedef enum {
    HT_ERROR_NONE = 0,
    HT_ERROR_MAIN_MALLOC,
    HT_ERROR_INIT_WINDOW,
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
#ifdef ENABLE_VALIDATION_LAYERS
    HT_ERROR_CHECK_VALIDATION_LAYER_SUPPORT_ALLOC,
    HT_ERROR_SETUP_DEBUG_MESSENGER,
    HT_ERROR_CREATE_INSTANCE_VALIDATION,
#endif
} hello_triangle_error_t;

void framebuffer_resize_callback(GLFWwindow *window, int width, int height) {
    hello_triangle_app_t *app = glfwGetWindowUserPointer(window);
    app->framebuffer_resized = true;
}

static int init_window(hello_triangle_app_t *app) {
    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    app->window = glfwCreateWindow(
        app->width,
        app->height,
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

typedef result(bool) bool_result_t;

#ifdef ENABLE_VALIDATION_LAYERS
VKAPI_ATTR VkBool32 VKAPI_CALL debug_callback(
    VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
    VkDebugUtilsMessageTypeFlagsEXT message_type,
    const VkDebugUtilsMessengerCallbackDataEXT* callback_data,
    void* user_data
) {
    fprintf(stderr, "validation layer: %s\n", callback_data->pMessage);

    return VK_FALSE;
}

static bool_result_t check_validation_layer_support(
    hello_triangle_app_t *app,
    byte_slice_t temp_arena
) {
    uint32_t layer_count;
    vkEnumerateInstanceLayerProperties(&layer_count, NULL);

    VkLayerProperties *available_layers = aven_alloc(
        &temp_arena,
        layer_count * sizeof(*available_layers),
        alignof(*available_layers)
    );
    if (available_layers == NULL) {
        return (bool_result_t){
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
            return (bool_result_t){ .payload = false };
        }
    }

    return (bool_result_t){ .payload = true };
}

static VkResult create_debug_utils_messenger_ext(
    VkInstance instance,
    const VkDebugUtilsMessengerCreateInfoEXT *create_info,
    const VkAllocationCallbacks *allocator,
    VkDebugUtilsMessengerEXT *debug_messenger
) {
    PFN_vkCreateDebugUtilsMessengerEXT func =
        (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance,
            "vkCreateDebugUtilsMessengerEXT"
        );
    if (func == NULL) {
        return VK_ERROR_EXTENSION_NOT_PRESENT;
    }

    return func(instance, create_info, allocator, debug_messenger);
}

static void destroy_debug_utils_messenger_ext(
    VkInstance instance,
    VkDebugUtilsMessengerEXT debug_messenger,
    const VkAllocationCallbacks *allocator
) {
    PFN_vkDestroyDebugUtilsMessengerEXT func = 
        (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
            instance,
            "vkDestroyDebugUtilsMessengerEXT"
        );
    assert(func != NULL);

    func(instance, debug_messenger, allocator);
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

static int setup_debug_messenger(hello_triangle_app_t *app) {
    VkDebugUtilsMessengerCreateInfoEXT create_info =
        debug_messenger_create_info();

    VkResult result = create_debug_utils_messenger_ext(
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
#endif

static int create_instance(
    hello_triangle_app_t *app,
    byte_slice_t temp_arena
) {
#ifdef ENABLE_VALIDATION_LAYERS
    {
        bool_result_t layer_support_result = check_validation_layer_support(
            app,
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
#endif

    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Hello Triangle",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "No Engine",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    uint32_t glfw_extension_count = 0;
    const char **glfw_extensions = glfwGetRequiredInstanceExtensions(
        &glfw_extension_count
    );

    uint32_t extension_count = glfw_extension_count + 1;
    const char **extensions = aven_alloc(
        &temp_arena, 
        extension_count * sizeof(*extensions),
        alignof(*extensions)
    );
    if (extensions == NULL) {
        return HT_ERROR_CREATE_INSTANCE_ALLOC;
    }

    memcpy(
        extensions,
        glfw_extensions,
        sizeof(*glfw_extensions) * glfw_extension_count
    );
    extensions[glfw_extension_count] = VK_EXT_DEBUG_UTILS_EXTENSION_NAME;

    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info,
        .enabledExtensionCount = extension_count,
        .ppEnabledExtensionNames = extensions,
#ifdef ENABLE_VALIDATION_LAYERS
        .enabledLayerCount = countof(VALIDATION_LAYERS),
        .ppEnabledLayerNames = VALIDATION_LAYERS,
        .pNext = &debug_create_info,
#endif
    };

    VkResult result = vkCreateInstance(&create_info, NULL, &app->instance);
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_INSTANCE_CREATE;
    }
    return 0;
}

static int create_surface(hello_triangle_app_t *app) {
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
    optional(uint32_t) graphics_family;
    optional(uint32_t) present_family;
} queue_family_indices_t;

typedef result(queue_family_indices_t) queue_family_indices_result_t;

static queue_family_indices_result_t find_queue_families(
    hello_triangle_app_t *app,
    VkPhysicalDevice device,
    byte_slice_t temp_arena
) {
    queue_family_indices_t indices = { 0 };

    uint32_t queue_family_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, NULL);
    VkQueueFamilyProperties *queue_families = aven_alloc(
        &temp_arena,
        queue_family_count * sizeof(*queue_families),
        alignof(*queue_families)
    );
    if (queue_families == NULL) {
        return (queue_family_indices_result_t){
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

    return (queue_family_indices_result_t){ .payload = indices };
}

static bool_result_t check_device_extension_support(
    VkPhysicalDevice device,
    byte_slice_t temp_arena
) {
    uint32_t extension_count;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extension_count, NULL);

    VkExtensionProperties *available_extensions = aven_alloc(
        &temp_arena,
        extension_count * sizeof(*available_extensions),
        alignof(*available_extensions)
    );
    if (available_extensions == NULL) {
        return (bool_result_t){
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
            return (bool_result_t){ .payload = false };
        }
    }

    return (bool_result_t){ .payload = true };
}

typedef slice(VkSurfaceFormatKHR) VkSurfaceFormatKHR_slice_t;
typedef slice(VkPresentModeKHR) VkPresentModeKHR_slice_t;

typedef struct {
    VkSurfaceCapabilitiesKHR capabilities;
    VkSurfaceFormatKHR_slice_t formats;
    VkPresentModeKHR_slice_t present_modes;
} swapchain_support_details_t;

typedef result(
    swapchain_support_details_t
) swapchain_support_details_result_t;

static swapchain_support_details_result_t query_swapchain_support(
    hello_triangle_app_t *app,
    VkPhysicalDevice device,
    byte_slice_t *perm_arena
) {
    swapchain_support_details_t details = { 0 };
    
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
        details.formats.ptr = aven_alloc(
            perm_arena,
            format_count * sizeof(*details.formats.ptr),
            alignof(*details.formats.ptr)
        );
        if (details.formats.ptr == NULL) {
            return (swapchain_support_details_result_t){
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
        details.present_modes.ptr = aven_alloc(
            perm_arena,
            present_mode_count * sizeof(*details.present_modes.ptr),
            alignof(*details.present_modes.ptr)
        );
        if (details.present_modes.ptr == NULL) {
            return (swapchain_support_details_result_t){
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

    return (swapchain_support_details_result_t){ .payload = details };
}

static bool_result_t is_device_suitable(
    hello_triangle_app_t *app,
    VkPhysicalDevice device,
    byte_slice_t temp_arena
) {
    queue_family_indices_t indices;
    {
        queue_family_indices_result_t result = find_queue_families(
            app,
            device,
            temp_arena
        );
        if (result.error != 0) {
            return (bool_result_t){ .error = result.error };
        }
        
        indices = result.payload;
    }

    bool queue_family_complete = indices.graphics_family.valid and
        indices.present_family.valid;

    bool extensions_supported = false;
    {
        bool_result_t result = check_device_extension_support(
            device,
            temp_arena
        );
        if (result.error != 0) {
            return result;
        }

        extensions_supported = result.payload;
    }

    bool swapchain_adequate = false;
    if (extensions_supported) {
        swapchain_support_details_result_t result = query_swapchain_support(
            app,
            device,
            &temp_arena
        );
        if (result.error != 0) {
            return (bool_result_t){ .error = result.error };
        }

        swapchain_adequate = result.payload.formats.len > 0 and
            result.payload.present_modes.len > 0;
    }

    return (bool_result_t){
        .payload = queue_family_complete and
            extensions_supported and
            swapchain_adequate
    };
}

static int pick_physical_device(
    hello_triangle_app_t *app,
    byte_slice_t temp_arena
) {
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(app->instance, &device_count, NULL);
    if (device_count == 0) {
        return HT_ERROR_PICK_PHYSICAL_DEVICE_ENUMERATE;
    }

    VkPhysicalDevice *devices = aven_alloc(
        &temp_arena,
        device_count * sizeof(*devices),
        alignof(*devices)
    );
    if (devices == NULL) {
        return HT_ERROR_PICK_PHYSICAL_DEVICE_ALLOC;
    }

    vkEnumeratePhysicalDevices(app->instance, &device_count, devices);

    for (uint32_t i = 0; i < device_count; ++i) {
        bool_result_t result = is_device_suitable(app, devices[i], temp_arena);
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

static int create_logical_device(
    hello_triangle_app_t *app,
    byte_slice_t temp_arena
) {
    queue_family_indices_result_t indices_result = find_queue_families(
        app,
        app->physical_device,
        temp_arena
    );
    if (indices_result.error != 0) {
        return indices_result.error;
    }

    queue_family_indices_t indices = indices_result.payload;
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

    VkDeviceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
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
    VkSurfaceFormatKHR_slice_t available_formats
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
    VkPresentModeKHR_slice_t available_present_modes
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
    hello_triangle_app_t *app,
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
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena,
    byte_slice_t temp_arena
) {
    swapchain_support_details_t swapchain_support;
    {
        swapchain_support_details_result_t result = query_swapchain_support(
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

    queue_family_indices_t indices;
    {
        queue_family_indices_result_t result = find_queue_families(
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

    app->swapchain_images.ptr = aven_alloc(
        swapchain_arena,
        image_count * sizeof(*app->swapchain_images.ptr),
        alignof(*app->swapchain_images.ptr)
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

static int create_image_views(
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena,
    byte_slice_t temp_arena
) {
    app->swapchain_image_views.ptr = aven_alloc(
        swapchain_arena,
        app->swapchain_images.len * sizeof(*app->swapchain_image_views.ptr),
        alignof(*app->swapchain_image_views.ptr)
    );
    if (app->swapchain_image_views.ptr == NULL) {
        return HT_ERROR_CREATE_IMAGE_VIEWS_ALLOC;
    }
    app->swapchain_image_views.len = app->swapchain_images.len;

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

typedef result(byte_slice_t) byte_slice_result_t;

static byte_slice_result_t read_file(
    const char *filename,
    byte_slice_t *perm_arena
) {
    FILE *file = fopen(filename, "rb");
    if (file == NULL) {
        return (byte_slice_result_t){ .error = HT_ERROR_READ_FILE_OPEN };
    }

    int error = fseek(file, 0L, SEEK_END);
    if (error != 0) {
        fclose(file);
        return (byte_slice_result_t){ .error = HT_ERROR_READ_FILE_SEEK };
    }

    long len = ftell(file);
    if (len < 0) {
        fclose(file);
        return (byte_slice_result_t){ .error = HT_ERROR_READ_FILE_TELL };
    }

    rewind(file);

    byte_slice_t bytes = { .ptr = NULL, .len = (size_t)len };
    bytes.ptr = aven_alloc(perm_arena, bytes.len, alignof(bytes.ptr));
    if (bytes.ptr == NULL) {
        fclose(file);
        return (byte_slice_result_t){ .error = HT_ERROR_READ_FILE_ALLOC };
    }
    
    size_t bytes_read = fread(bytes.ptr, 1, bytes.len, file);

    fclose(file);

    if (bytes_read != bytes.len) {
        return (byte_slice_result_t){ .error = HT_ERROR_READ_FILE_READ };
    }

    return (byte_slice_result_t){ .payload = bytes };
}

typedef result(VkShaderModule) VkShaderModule_result_t;

static VkShaderModule_result_t create_shader_module(
    hello_triangle_app_t *app,
    byte_slice_t code
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
        return (VkShaderModule_result_t){
            .error = HT_ERROR_CREATE_SHADER_MODULE
        };
    }

    return (VkShaderModule_result_t){ .payload = shader_module };
}

static int create_render_pass(hello_triangle_app_t *app) {
    VkAttachmentDescription color_attachment = {
        .format = app->swapchain_image_format,
        .samples = VK_SAMPLE_COUNT_1_BIT,
        .loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .storeOp = VK_ATTACHMENT_STORE_OP_STORE,
        .stencilLoadOp = VK_ATTACHMENT_LOAD_OP_CLEAR,
        .stencilStoreOp = VK_ATTACHMENT_STORE_OP_STORE,
        .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
    };

    VkAttachmentReference color_attachment_ref = {
        .attachment = 0,
        .layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    };

    VkSubpassDescription subpass = {
        .pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS,
        .colorAttachmentCount = 1,
        .pColorAttachments = &color_attachment_ref,
    };

    VkSubpassDependency dependency = {
        .srcSubpass = VK_SUBPASS_EXTERNAL,
        .dstSubpass = 0,
        .srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .srcAccessMask = 0,
        .dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
        .dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT,
    };

    VkRenderPassCreateInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
        .attachmentCount = 1,
        .pAttachments = &color_attachment,
        .subpassCount = 1,
        .pSubpasses = &subpass,
        .dependencyCount = 1,
        .pDependencies = &dependency,
    };

    VkResult result = vkCreateRenderPass(
        app->device,
        &render_pass_info,
        NULL,
        &app->render_pass
    );
    if (result != VK_SUCCESS) {
        return HT_ERROR_CREATE_RENDER_PASS;
    }

    return 0;
}

static int create_graphics_pipeline(
    hello_triangle_app_t *app,
    byte_slice_t temp_arena
) {
    byte_slice_t vert_shader_code;
    {
        byte_slice_result_t result = read_file("shaders/vert.spv", &temp_arena);
        if (result.error != 0) {
            return result.error;
        }

        vert_shader_code = result.payload;
    }

    byte_slice_t frag_shader_code;
    {
        byte_slice_result_t result = read_file("shaders/frag.spv", &temp_arena);
        if (result.error != 0) {
            return result.error;
        }

        frag_shader_code = result.payload;
    }

    VkShaderModule vert_shader_module;
    {
        VkShaderModule_result_t result = create_shader_module(
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
        VkShaderModule_result_t result = create_shader_module(
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
        .vertexBindingDescriptionCount = 0,
        .pVertexBindingDescriptions = NULL,
        .vertexAttributeDescriptionCount = 0,
        .pVertexAttributeDescriptions = NULL,
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

    VkGraphicsPipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
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
        .renderPass = app->render_pass,
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

static int create_framebuffers(
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena
) {
    app->swapchain_framebuffers.ptr = aven_alloc(
        swapchain_arena,
        app->swapchain_image_views.len * sizeof(
            *app->swapchain_framebuffers.ptr
        ),
        alignof(*app->swapchain_framebuffers.ptr)
    );
    if (app->swapchain_framebuffers.ptr == NULL) {
        return HT_ERROR_CREATE_FRAMEBUFFER_ALLOC;
    }
    app->swapchain_framebuffers.len = app->swapchain_image_views.len;

    for (size_t i = 0; i < app->swapchain_image_views.len; ++i) {
        VkImageView attachments[] = {
            slice_get(app->swapchain_image_views, i)
        };

        VkFramebufferCreateInfo framebuffer_info = {
            .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
            .renderPass = app->render_pass,
            .attachmentCount = countof(attachments),
            .pAttachments = attachments,
            .width = app->swapchain_extent.width,
            .height = app->swapchain_extent.height,
            .layers = 1,
        };

        VkResult result = vkCreateFramebuffer(
            app->device,
            &framebuffer_info,
            NULL,
            &slice_get(app->swapchain_framebuffers, i)
        );
        if (result != VK_SUCCESS) {
            return HT_ERROR_CREATE_FRAMEBUFFER_CREATE;
        }
    }

    return 0;
}

static int create_command_pool(
    hello_triangle_app_t *app,
    byte_slice_t temp_arena
) {
    queue_family_indices_t queue_family_indices;
    {
        queue_family_indices_result_t result = find_queue_families(
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

static int create_command_buffers(hello_triangle_app_t *app) {
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

static int create_sync_objects(hello_triangle_app_t *app) {
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
    hello_triangle_app_t *app,
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

    VkClearValue clear_color = {
        .color = {
            .float32 = { 0.0f, 0.0f, 0.0f, 1.0f },
        },
    };

    VkRenderPassBeginInfo render_pass_info = {
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = app->render_pass,
        .framebuffer = slice_get(app->swapchain_framebuffers, image_index),
        .renderArea = {
            .offset = { 0, 0 },
            .extent = app->swapchain_extent,
        },
        .clearValueCount = 1,
        .pClearValues = &clear_color,
    };

    vkCmdBeginRenderPass(
        command_buffer,
        &render_pass_info,
        VK_SUBPASS_CONTENTS_INLINE
    );

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

    vkCmdDraw(command_buffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(command_buffer);

    result = vkEndCommandBuffer(command_buffer);
    if (result != VK_SUCCESS) {
        return HT_ERROR_RECORD_COMMAND_BUFFER_END;
    }

    return 0;
}

static void cleanup_swapchain(hello_triangle_app_t *app) {
    for (size_t i = 0; i < app->swapchain_framebuffers.len; ++i) {
        vkDestroyFramebuffer(
            app->device,
            slice_get(app->swapchain_framebuffers, i),
            NULL
        );
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

static int recreate_swapchain(
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena,
    byte_slice_t temp_arena
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

    error = create_image_views(app, swapchain_arena, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_framebuffers(app, swapchain_arena);
    if (error != 0) {
        return error;
    }

    return 0;
}

static int init_vulkan(
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena,
    byte_slice_t temp_arena
) {
    int error = create_instance(app, temp_arena);
    if (error != 0) {
        return error;
    }

#ifdef ENABLE_VALIDATION_LAYERS
    error = setup_debug_messenger(app);
    if (error != 0) {
        return error;
    }
#endif
    
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

    error = create_image_views(app, swapchain_arena, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_render_pass(app);
    if (error != 0) {
        return error;
    }

    error = create_graphics_pipeline(app, temp_arena);
    if (error != 0) {
        return error;
    }

    error = create_framebuffers(app, swapchain_arena);
    if (error != 0) {
        return error;
    }

    error = create_command_pool(app, temp_arena);
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
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena,
    byte_slice_t temp_arena
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
    record_command_buffer(
        app,
        app->command_buffers[app->current_frame],
        image_index
    );

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
    hello_triangle_app_t *app,
    byte_slice_t *swapchain_arena,
    byte_slice_t temp_arena
) {
    while (!glfwWindowShouldClose(app->window)) {
        glfwPollEvents();
        draw_frame(app, swapchain_arena, temp_arena);
    }
    
    vkDeviceWaitIdle(app->device);

    return 0;
}

static void cleanup(hello_triangle_app_t *app) {
    cleanup_swapchain(app);

    vkDestroyPipeline(app->device, app->graphics_pipeline, NULL);
    vkDestroyPipelineLayout(app->device, app->pipeline_layout, NULL);

    vkDestroyRenderPass(app->device, app->render_pass, NULL);

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
    destroy_debug_utils_messenger_ext(
        app->instance,
        app->debug_messenger,
        NULL
    );
#endif

    vkDestroyInstance(app->instance, NULL);

    glfwDestroyWindow(app->window);
    glfwTerminate();
}

static int run(hello_triangle_app_t *app, byte_slice_t temp_arena) {
    byte_slice_t swapchain_arena = {
        .ptr = aven_alloc(&temp_arena, ARENA_SIZE, 1),
        .len = ARENA_SIZE,
    };
    assert(swapchain_arena.ptr != NULL);

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
    byte_slice_t arena = {
        .ptr = malloc(2 * ARENA_SIZE),
        .len = 2 * ARENA_SIZE
    };
    if (arena.ptr == NULL) {
        return HT_ERROR_MAIN_MALLOC;
    }

    hello_triangle_app_t app = {
        .width = 640,
        .height = 480,
        .physical_device = VK_NULL_HANDLE,
    };

    int error = run(&app, arena);

    free(arena.ptr);

    return error;
}

