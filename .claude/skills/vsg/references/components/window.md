---
title: Window / WindowTraits
description: WindowTraits holds the configuration (size, fullscreen, samples, debug layers, swapchain prefs) used to create a platform-specific Vulkan Window via Window::create(traits).
---

## Public include
```cpp
#include <vsg/all.h>             // barrel include, brings in everything
// for reference, the specific headers:
// #include <vsg/app/Window.h>        // Window
// #include <vsg/app/WindowTraits.h>  // WindowTraits
```
Namespace is `vsg::`. `Window.h` itself includes `WindowTraits.h` (`include/vsg/app/Window.h:15`). All symbols live in `namespace vsg` (`include/vsg/app/WindowTraits.h:19`, `include/vsg/app/Window.h:24`).

## When to use
Use `vsg::WindowTraits` to declare *what kind of* window/Vulkan instance/device you want, and `vsg::Window::create(traits)` to actually instantiate the cross-platform window (`include/vsg/app/WindowTraits.h:26`, `include/vsg/app/Window.h:27`). `Window` is an abstract base; `Window::create()` returns the correct platform subclass (Xcb/Win32/MacOS/etc.) for the running OS (`include/vsg/app/Window.h:28-29`). You do not construct platform window classes directly, and you do not hand-build instance/device/swapchain — the traits drive all of that. Pair the created window with `vsg::Viewer` (call `viewer->addWindow(window)`); the traits object is not a viewer and does no rendering itself.

## Key API
WindowTraits:
- `vsg::WindowTraits::create(...)` — factory returning `ref_ptr<WindowTraits>`; overloads accept `CommandLine&`, a title `std::string`, `(width,height,title)`, or `(x,y,width,height,title)` (`include/vsg/app/WindowTraits.h:30-35`).
- `WindowTraits(CommandLine& arguments)` — reads many CLI options into the traits (see below) (`include/vsg/app/WindowTraits.h:31`).
- `width` / `height` — `uint32_t`, default `1280` / `1024` (`include/vsg/app/WindowTraits.h:49-50`).
- `x` / `y` — `int32_t` window position, default `0` / `0` (`include/vsg/app/WindowTraits.h:47-48`).
- `fullscreen` — `bool`, default `false` (`include/vsg/app/WindowTraits.h:52`).
- `windowTitle` — `std::string`, default `"vsg window"` (`include/vsg/app/WindowTraits.h:58`).
- `decoration` — `bool` window border/titlebar, default `true` (`include/vsg/app/WindowTraits.h:60`).
- `samples` — `VkSampleCountFlags` multisample bitmask, default `VK_SAMPLE_COUNT_1_BIT`; device picks max requested value it supports (`include/vsg/app/WindowTraits.h:92-96`).
- `debugLayer` — `bool` enable `VK_LAYER_KHRONOS_validation`, default `false` (`include/vsg/app/WindowTraits.h:77`).
- `apiDumpLayer` / `synchronizationLayer` / `debugUtils` — `bool`, default `false` (`include/vsg/app/WindowTraits.h:78-80`).
- `swapchainPreferences` — `SwapchainPreferences` (presentMode, imageCount, surfaceFormat) (`include/vsg/app/WindowTraits.h:68`).
- `depthFormat` — `VkFormat`, default `VK_FORMAT_D32_SFLOAT` (`include/vsg/app/WindowTraits.h:69`).
- `vulkanVersion` — default `VK_API_VERSION_1_0` (`include/vsg/app/WindowTraits.h:66`).
- `deviceFeatures` / `deviceExtensionNames` / `deviceTypePreferences` — device tuning (`include/vsg/app/WindowTraits.h:88-90`).
- `validate()` — finalizes layer/extension lists from the debug flags (`include/vsg/app/WindowTraits.h:45`).

Window:
- `vsg::Window::create(ref_ptr<WindowTraits>)` — returns `ref_ptr<Window>`, **may be null** on failure (`include/vsg/app/Window.h:37`).
- `extent2D()` — `const VkExtent2D&`, the actual framebuffer size; use for camera aspect ratio and `ViewportState` (`include/vsg/app/Window.h:64`).
- `traits()` — accessor for the `ref_ptr<WindowTraits>` the window was built from (`include/vsg/app/Window.h:61-62`).
- `clearColor()` — mutable `vec4&` clear color (`include/vsg/app/Window.h:66`).
- `framebufferSamples()` — `VkSampleCountFlagBits` actually chosen (`include/vsg/app/Window.h:73`).
- `getDevice()` / `getOrCreateDevice()`, `getInstance()`, `getSurface()`, `getSwapchain()` — lazy accessors to underlying Vulkan objects (`include/vsg/app/Window.h:75-96`).

CommandLine options read by `WindowTraits(CommandLine&)` include `--debug`/`-d` (debugLayer), `--api`/`-a`, `--sync`, `--window`/`-w W H`, `--fullscreen`/`--fs`, `--no-frame`, `--samples N`, `--screen`, `--display`, `--IMMEDIATE`/`--FIFO`/`--MAILBOX`, `--double-buffer`/`--triple-buffer`, `--prefer-discrete`/`--prefer-integrated` (`src/vsg/app/WindowTraits.cpp:34-66`).

## Best Practices
- Always create traits and windows via `::create()`, never `new`; they are intrusively ref-counted `vsg::Object`s and clean up when the last `ref_ptr` drops (`include/vsg/app/WindowTraits.h:27`, `include/vsg/app/Window.h:30`).
- ALWAYS null-check the result of `Window::create()` — it returns null when window/Vulkan setup fails (`include/vsg/app/Window.h:37`). Real examples bail with an error message and `return 1` (`examples/app/vsghelloworld/vsghelloworld.cpp:31-35`, `examples/app/vsgviewer/vsgviewer.cpp:228-231`).
- Build traits with `WindowTraits::create(arguments)` first, then override individual fields by direct member assignment (e.g. `windowTraits->fullscreen = true;`, `windowTraits->width = 192;`) before passing to `Window::create()` (`examples/app/vsgviewer/vsgviewer.cpp:55`, `examples/app/vsgviewer/vsgviewer.cpp:79-87`).
- Derive camera aspect ratio and viewport from `window->extent2D()` (the real framebuffer size), not from the requested `traits->width/height`, since the windowing system may resize (`examples/app/vsghelloworld/vsghelloworld.cpp:58`, `examples/app/vsghelloworld/vsghelloworld.cpp:61`).
- Set device-level options (e.g. `deviceFeatures`, `deviceExtensionNames`) on the traits *before* `Window::create()`, since the device is built during creation (`examples/app/vsgviewer/vsgviewer.cpp:110-112`, `examples/app/vsgviewer/vsgviewer.cpp:131`).
- Enable `debugLayer = true` (or pass `-d`/`--debug`) during development to turn on Vulkan validation; leave it off in release (`include/vsg/app/WindowTraits.h:77`, `src/vsg/app/WindowTraits.cpp:34`).
- For multisampling set `samples` to a `VkSampleCountFlagBits` such as `VK_SAMPLE_COUNT_8_BIT`; the device clamps to the max it supports (`include/vsg/app/WindowTraits.h:92-96`). Read the chosen value back via `framebufferSamples()` (`include/vsg/app/Window.h:73`).
- For multi-window setups, copy a base traits object via the copy-construct overload `WindowTraits::create(*windowTraits)` rather than sharing one mutable instance (`examples/app/vsgmultigpu/vsgmultigpu.cpp:252`, `include/vsg/app/WindowTraits.h:32`).
- After creating windows, add them to a `vsg::Viewer` and call `viewer->compile()` before the frame loop — the window alone does not compile/record (`examples/app/vsghelloworld/vsghelloworld.cpp:37`, `examples/app/vsghelloworld/vsghelloworld.cpp:74`).

## Composition examples
Distilled from `examples/app/vsghelloworld/vsghelloworld.cpp:6-74` and `examples/app/vsgviewer/vsgviewer.cpp:55-291`:
```cpp
#include <vsg/all.h>
#include <iostream>

int main(int argc, char** argv)
{
    // read CLI args (--window W H, --fullscreen, --samples N, -d for debug, ...)
    vsg::CommandLine arguments(&argc, argv);

    // traits read CLI options into their fields
    auto windowTraits = vsg::WindowTraits::create(arguments);
    windowTraits->windowTitle = "my vsg app";   // override a field directly

    // create the platform-specific window; MAY be null -> always check
    auto window = vsg::Window::create(windowTraits);
    if (!window)
    {
        std::cout << "Could not create window." << std::endl;
        return 1;
    }

    auto viewer = vsg::Viewer::create();
    viewer->addWindow(window);                    // window belongs to the viewer

    // use the ACTUAL framebuffer extent for aspect ratio + viewport
    auto perspective = vsg::Perspective::create(
        30.0,
        static_cast<double>(window->extent2D().width) /
        static_cast<double>(window->extent2D().height),
        0.1, 1000.0);
    auto lookAt = vsg::LookAt::create(/* eye, centre, up */);
    auto camera = vsg::Camera::create(perspective, lookAt,
        vsg::ViewportState::create(window->extent2D()));
    // ... build command graph, viewer->compile(), then frame loop ...
}
```

## Source references
- `include/vsg/app/WindowTraits.h` — `WindowTraits` declaration, members and defaults
- `include/vsg/app/Window.h` — `Window` base class, `create()`, `extent2D()`, accessors
- `src/vsg/app/WindowTraits.cpp` — which CommandLine flags map to which traits fields
- `examples/app/vsghelloworld/vsghelloworld.cpp` — minimal traits + window + null-check + viewer wiring
- `examples/app/vsgviewer/vsgviewer.cpp` — overriding traits fields, deviceFeatures, debug, extent2D usage
- `examples/app/vsgmultigpu/vsgmultigpu.cpp` — copy-constructing traits for multiple windows

## Common mistakes
- Using the requested `windowTraits->width/height` for the camera aspect ratio -> use `window->extent2D().width/height`, the real framebuffer size (`examples/app/vsghelloworld/vsghelloworld.cpp:58`).
- Ignoring the return of `Window::create()` -> it can be null; check and bail before dereferencing (`include/vsg/app/Window.h:37`, `examples/app/vsghelloworld/vsghelloworld.cpp:31`).
- Allocating with `new vsg::WindowTraits` / `new vsg::Window` -> use `vsg::WindowTraits::create(...)` and `vsg::Window::create(...)` which return `ref_ptr` (`include/vsg/app/WindowTraits.h:30`, `include/vsg/app/Window.h:37`).
- Setting `deviceFeatures`/extensions on traits *after* `Window::create()` -> set them before creation; the device is built during `create()` (`examples/app/vsgviewer/vsgviewer.cpp:110-112`).
- Sharing one mutable traits object across several windows you want to differ -> copy it with `WindowTraits::create(*base)` (`examples/app/vsgmultigpu/vsgmultigpu.cpp:252`).
- Trying to `new` a `MacOS_Window`/`Xcb_Window` directly -> always go through `Window::create(traits)` (`include/vsg/app/Window.h:28-37`).

## Things to never invent
- There is no `Window::create(width, height)` taking raw ints — `Window::create` takes only `ref_ptr<WindowTraits>` (`include/vsg/app/Window.h:37`). The size overloads belong to `WindowTraits`, not `Window`.
- There is no `setExtent2D()`/`resize(w,h)` setter on `Window`; `extent2D()` is read-only and `resize()` takes no args (`include/vsg/app/Window.h:59`, `include/vsg/app/Window.h:64`).
- There is no `traits->setWidth()` / getter-style accessors — fields like `width`, `height`, `fullscreen`, `samples` are plain public members assigned directly (`include/vsg/app/WindowTraits.h:47-96`).
- `samples` is a `VkSampleCountFlags` field, not a method; do not call `windowTraits->samples(...)` (`include/vsg/app/WindowTraits.h:96`).
- Do not assume a `vsync`/`presentMode` field on `WindowTraits` directly — present mode lives under `swapchainPreferences.presentMode` (`include/vsg/app/WindowTraits.h:68`, `src/vsg/app/WindowTraits.cpp:40`).
- There is no `Window::isValid()`; the accessor is `valid()` (and `visible()`) (`include/vsg/app/Window.h:41-43`).
