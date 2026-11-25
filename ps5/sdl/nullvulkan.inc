// PS5: Vulkan through SDL's dummy video driver. There is no window system: the "window" is the console's
// one display, reached with VK_KHR_display. The title links RADV directly (no loader); the surface is
// made by ps5/glue/ps5_vulkan.cpp. Included by SDL_nullvideo.c (ps5/sdl/patch.py).
#ifdef SDL_VIDEO_VULKAN
#include "../SDL_vulkan_internal.h"

extern PFN_vkVoidFunction vk_icdGetInstanceProcAddr(VkInstance instance, const char *name);
extern VkResult mh_ps5_vk_create_surface(VkInstance instance, VkSurfaceKHR *surface);

static bool DUMMY_Vulkan_LoadLibrary(SDL_VideoDevice *_this, const char *path)
{
    _this->vulkan_config.vkGetInstanceProcAddr = (SDL_FunctionPointer)vk_icdGetInstanceProcAddr;
    _this->vulkan_config.vkEnumerateInstanceExtensionProperties =
        (SDL_FunctionPointer)vk_icdGetInstanceProcAddr(VK_NULL_HANDLE, "vkEnumerateInstanceExtensionProperties");
    SDL_strlcpy(_this->vulkan_config.loader_path, "ps5-radv", sizeof(_this->vulkan_config.loader_path));
    return true;
}

static void DUMMY_Vulkan_UnloadLibrary(SDL_VideoDevice *_this)
{
}

static char const *const *DUMMY_Vulkan_GetInstanceExtensions(SDL_VideoDevice *_this, Uint32 *count)
{
    static const char *const extensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_DISPLAY_EXTENSION_NAME };
    if (count) {
        *count = 2;
    }
    return extensions;
}

static bool DUMMY_Vulkan_CreateSurface(SDL_VideoDevice *_this, SDL_Window *window, VkInstance instance,
                                       const struct VkAllocationCallbacks *allocator, VkSurfaceKHR *surface)
{
    const VkResult r = mh_ps5_vk_create_surface(instance, surface);
    if (r != VK_SUCCESS) {
        return SDL_SetError("PS5 display surface failed: %d", (int)r);
    }
    return true;
}

static void DUMMY_Vulkan_DestroySurface(SDL_VideoDevice *_this, VkInstance instance, VkSurfaceKHR surface,
                                        const struct VkAllocationCallbacks *allocator)
{
    PFN_vkDestroySurfaceKHR destroy = (PFN_vkDestroySurfaceKHR)vk_icdGetInstanceProcAddr(instance, "vkDestroySurfaceKHR");
    if (destroy) {
        destroy(instance, surface, allocator);
    }
}

static bool DUMMY_Vulkan_GetPresentationSupport(SDL_VideoDevice *_this, VkInstance instance, VkPhysicalDevice physicalDevice,
                                                Uint32 queueFamilyIndex)
{
    PFN_vkGetPhysicalDeviceQueueFamilyProperties props =
        (PFN_vkGetPhysicalDeviceQueueFamilyProperties)vk_icdGetInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties");
    VkQueueFamilyProperties q[16];
    Uint32 n = 16;
    if (!props) {
        return false;
    }
    props(physicalDevice, &n, q);
    return queueFamilyIndex < n && (q[queueFamilyIndex].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0;
}
#endif // SDL_VIDEO_VULKAN
