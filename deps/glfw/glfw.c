#if defined(_WIN32)
    #define _GLFW_WIN32
#elif defined(__linux__)
    #define _GLFW_WAYLAND
    #define _GLFW_X11
#endif

#define _GNU_SOURCE

#include "src/init.c"
#include "src/platform.c"
#include "src/context.c"
#include "src/monitor.c"
#include "src/window.c"
#include "src/input.c"
#include "src/vulkan.c"

#if defined(_WIN32)
    #include "src/win32_init.c"
    #include "src/win32_module.c"
    #include "src/win32_monitor.c"
    #include "src/win32_window.c"
    #include "src/win32_joystick.c"
    #include "src/win32_time.c"
    #include "src/win32_thread.c"
    #include "src/wgl_context.c"

    #include "src/egl_context.c"
    #include "src/osmesa_context.c"
#elif defined(__linux__) 
    #include "src/posix_poll.c"
    #include "src/posix_module.c"
    #include "src/posix_thread.c"
    #include "src/posix_time.c"
    #include "src/linux_joystick.c"
    #include "src/xkb_unicode.c"

    #include "src/egl_context.c"
    #include "src/osmesa_context.c"

    #include "src/wl_init.c"
    #include "src/wl_monitor.c"
    #include "src/wl_window.c"

    #include "src/x11_init.c"
    #include "src/x11_monitor.c"
    #include "src/x11_window.c"
    #include "src/glx_context.c"
#endif
