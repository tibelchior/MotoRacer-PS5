// The PS5 display as a Vulkan surface for SDL_GPU (ps5/sdl/nullvulkan.inc calls this). The title links RADV
// directly; the surface is VK_KHR_display on the console's one display, in the mode named by data/output
// (1080, 1440, 2160) or else the largest one. Swapchain, device and drawing are SDL_GPU's (the repo's own
// Direct3D backend); this file only makes the surface.
#define VK_NO_PROTOTYPES
#include <vulkan/vulkan.h>

#include <cstdarg>
#include <cstdio>
#include <string>

extern "C" VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vk_icdGetInstanceProcAddr(VkInstance instance, const char *name);
extern "C" const char *mh_ps5_data_dir(void);

namespace {
void say(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char *fmt, ...) {
    char text[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(text, sizeof text, fmt, ap);
    va_end(ap);
    std::fprintf(stderr, "[ps5-vulkan] %s\n", text);
    const std::string path = std::string(mh_ps5_data_dir()) + "/motoracer.log";
    if (FILE *f = std::fopen(path.c_str(), "a")) {
        std::fprintf(f, "[ps5-vulkan] %s\n", text);
        std::fclose(f);
    }
}
}  // namespace

extern "C" VkResult mh_ps5_vk_create_surface(VkInstance instance, VkSurfaceKHR *surface) {
    auto get = [&](const char *n) { return vk_icdGetInstanceProcAddr(instance, n); };
    auto enumerate = reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(get("vkEnumeratePhysicalDevices"));
    auto display_props = reinterpret_cast<PFN_vkGetPhysicalDeviceDisplayPropertiesKHR>(get("vkGetPhysicalDeviceDisplayPropertiesKHR"));
    auto mode_props = reinterpret_cast<PFN_vkGetDisplayModePropertiesKHR>(get("vkGetDisplayModePropertiesKHR"));
    auto create = reinterpret_cast<PFN_vkCreateDisplayPlaneSurfaceKHR>(get("vkCreateDisplayPlaneSurfaceKHR"));
    if (!enumerate || !display_props || !mode_props || !create) {
        say("display functions missing");
        return VK_ERROR_INITIALIZATION_FAILED;
    }
    uint32_t count = 1;
    VkPhysicalDevice physical = VK_NULL_HANDLE;
    VkResult r = enumerate(instance, &count, &physical);
    if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || count < 1) { say("no physical device (%d)", int(r)); return VK_ERROR_INITIALIZATION_FAILED; }
    VkDisplayPropertiesKHR display{};
    count = 1;
    r = display_props(physical, &count, &display);
    if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || count < 1) { say("no display (%d)", int(r)); return VK_ERROR_INITIALIZATION_FAILED; }
    VkDisplayModePropertiesKHR modes[16];
    count = 16;
    r = mode_props(physical, display.display, &count, modes);
    if ((r != VK_SUCCESS && r != VK_INCOMPLETE) || count < 1) { say("no display modes (%d)", int(r)); return VK_ERROR_INITIALIZATION_FAILED; }
    unsigned forced = 0;
    if (FILE *f = std::fopen((std::string(mh_ps5_data_dir()) + "/output").c_str(), "r")) {
        if (std::fscanf(f, "%u", &forced) != 1) forced = 0;
        std::fclose(f);
    }
    uint32_t best = 0;
    bool have = false;
    for (uint32_t i = 0; i < count; ++i) {
        const auto &v = modes[i].parameters.visibleRegion;
        say("display mode %u: %ux%u at %.3f Hz", i, v.width, v.height, modes[i].parameters.refreshRate / 1000.0);
        const auto &b = modes[best].parameters.visibleRegion;
        const bool better = forced ? (v.height == forced && (!have || modes[i].parameters.refreshRate > modes[best].parameters.refreshRate))
                                   : (v.width * v.height > b.width * b.height ||
                                      (v.width * v.height == b.width * b.height && modes[i].parameters.refreshRate > modes[best].parameters.refreshRate));
        if (better) { best = i; have = true; }
    }
    VkDisplaySurfaceCreateInfoKHR sci{VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR};
    sci.displayMode = modes[best].displayMode;
    sci.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
    sci.globalAlpha = 1.0f;
    sci.alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
    sci.imageExtent = modes[best].parameters.visibleRegion;
    r = create(instance, &sci, nullptr, surface);
    say("display surface %ux%u (mode %u): %d", sci.imageExtent.width, sci.imageExtent.height, best, int(r));
    return r;
}
