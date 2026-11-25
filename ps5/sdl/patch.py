#!/usr/bin/env python3
"""Patches a copy of the SDL3 source for Vulkan on the PS5 (run by tools/container/sdl.sh)."""
import re, sys
src, inc = sys.argv[1], sys.argv[2]
# 1. dummy video: Vulkan entry points
p = src + '/src/video/dummy/SDL_nullvideo.c'
s = open(p).read()
if 'nullvulkan.inc' not in s:
    body = open(inc).read()
    s = s.replace('static void DUMMY_EVDEV_VideoQuit(SDL_VideoDevice *_this);\n#endif\n',
                  'static void DUMMY_EVDEV_VideoQuit(SDL_VideoDevice *_this);\n#endif\n\n' + body + '\n', 1) \
        if 'static void DUMMY_EVDEV_VideoQuit(SDL_VideoDevice *_this);\n#endif\n' in s else body + '\n' + s
    s = s.replace('    device->free = DUMMY_DeleteDevice;\n',
                  '''    device->free = DUMMY_DeleteDevice;
#ifdef SDL_VIDEO_VULKAN
    device->Vulkan_LoadLibrary = DUMMY_Vulkan_LoadLibrary;
    device->Vulkan_UnloadLibrary = DUMMY_Vulkan_UnloadLibrary;
    device->Vulkan_GetInstanceExtensions = DUMMY_Vulkan_GetInstanceExtensions;
    device->Vulkan_CreateSurface = DUMMY_Vulkan_CreateSurface;
    device->Vulkan_DestroySurface = DUMMY_Vulkan_DestroySurface;
    device->Vulkan_GetPresentationSupport = DUMMY_Vulkan_GetPresentationSupport;
#endif
''', 1)
    open(p, 'w').write(s)
# 2. CMake: Vulkan video + SDL_GPU Vulkan driver on this (console) build
p = src + '/CMakeLists.txt'
s = open(p).read()
if 'PS5 VULKAN' not in s:
    s = s.replace('if(SDL_GPU)\n  if(HAVE_DXGI1_6_H)',
                  '# PS5 VULKAN: no window system, but the dummy video driver reaches the display through VK_KHR_display\nif(SDL_GPU)\n  set(SDL_VIDEO_VULKAN 1)\n  add_definitions(-DSDL_VIDEO_VULKAN=1 -DSDL_GPU_VULKAN=1)\n  if(HAVE_DXGI1_6_H)', 1)
    open(p, 'w').write(s)
# 3. SDL_GPU Vulkan: the console's RADV does not report a conformance version, so SDL would skip it as
#    "non-conformant"; accept it, and say why a device is rejected.
p = src + '/src/gpu/vulkan/SDL_gpu_vulkan.c'
s = open(p).read()
if 'PS5 ACCEPT' not in s:
    old = '    if (!isConformant) {\n        return 0;\n    }'
    assert old in s, 'conformance check not found'
    s = s.replace(old, '    if (!isConformant) { /* PS5 ACCEPT */\n        SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vulkan: driver reports non-conformant, using it anyway");\n    }', 1)
    n = 0
    def rej(m):
        global n
        n += 1
        return m.group(1) + 'SDL_LogWarn(SDL_LOG_CATEGORY_GPU, "Vulkan: device rejected (reason %d)"); ' % n + m.group(2)
    a = s.index('static Uint8 VULKAN_INTERNAL_IsDeviceSuitable(')
    b = s.index('static Uint8 VULKAN_INTERNAL_DeterminePhysicalDevice(')
    seg = re.sub(r'(\n\s+)(return 0;)', rej, s[a:b])
    s = s[:a] + seg + s[b:]
    open(p, 'w').write(s)
print('patched')
