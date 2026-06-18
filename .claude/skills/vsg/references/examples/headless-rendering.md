---
title: Headless / Offscreen Rendering
description: Render a VSG scene with no window or surface — build a Device directly from an Instance, render into your own Framebuffer, and copy the color image into a host-visible buffer to write to disk. NOTE — crashes on macOS/MoltenVK (see Platform note).
---

## Copy-paste starter (do this first)

**A complete, compile+link-verified headless program ships with this skill: [`headless-main.cpp`](./headless-main.cpp).** Copy that whole file as your `main.cpp` and build it with the standard Setup `CMakeLists.txt` — it is the fastest correct path. It already includes the offscreen render pass, the color-capture barrier sequence, the surface-less `Device`/`CommandGraph`, the no-`present()` loop, and the `MappedData` readback, with every step cited inline to `examples/app/vsgheadless/vsgheadless.cpp`. The walkthrough below explains *why* each step exists (read it when you need to adapt the starter, e.g. add MSAA or capture depth). ⚠ It builds everywhere but **crashes at runtime on macOS/MoltenVK** — see the Platform note.

## What to copy

- Headless rendering = no `vsg::Window`/`vsg::Surface`: build a `vsg::Device` straight from a `vsg::Instance`, render into a `vsg::Framebuffer` you allocate yourself (a `RenderPass` + color/depth `ImageView`s), drive it with a `Viewer` whose loop has no `present()`, then copy the color image into a host-visible buffer and `vsg::write` it. The canonical program is `examples/app/vsgheadless/vsgheadless.cpp`.
- The viewer still uses the normal record/submit task; it simply gets NO `Presentation` object because there is no present-capable queue family (see Related → foundations).
- You provide the offscreen target. `createCommandGraphForView` (the windowed convenience) is NOT used here; you wire `CommandGraph` → `RenderGraph` → `View` by hand.
- READ THE PLATFORM NOTE before attempting this on macOS — it crashes there under MoltenVK; capture from a window-backed swapchain instead.

## The program

Canonical source: `examples/app/vsgheadless/vsgheadless.cpp` (845 lines). It loads a model, builds a surface-less device, renders `numFrames` frames into an offscreen framebuffer, captures color (and optionally depth), and writes the result to `screenshot.vsgb` / `depth.vsgb`.

The file also defines the offscreen helpers it uses at file scope:
- `createOffscreenRenderPass` — `examples/app/vsgheadless/vsgheadless.cpp:29` (color attachment `finalLayout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL` at :34 so the color image is ready to copy from).
- `createColorImageView` — `examples/app/vsgheadless/vsgheadless.cpp:198` (image `usage` = `COLOR_ATTACHMENT_BIT | TRANSFER_SRC_BIT` at :208).
- `createDepthImageView` — `examples/app/vsgheadless/vsgheadless.cpp:216`.
- `createColorCapture` — `examples/app/vsgheadless/vsgheadless.cpp:234` (returns `{Commands, Image}`).
- `createDepthCapture` — `examples/app/vsgheadless/vsgheadless.cpp:394` (returns `{Commands, Buffer}`).

## The offscreen path

Step by step. Each step is a cite into the canonical program; the helpers above expand the inline detail.

1. Pick formats/extent. `VkExtent2D extent{2048, 1024}`, `imageFormat = VK_FORMAT_R8G8B8A8_UNORM`, `depthFormat = VK_FORMAT_D32_SFLOAT` — `examples/app/vsgheadless/vsgheadless.cpp:498`.

2. Create the `Instance`. `vsg::Instance::create(instanceExtensions, validatedNames, vulkanVersion)` — `examples/app/vsgheadless/vsgheadless.cpp:561`. NOTE: no surface/window extensions are required for the headless path.

3. Find a graphics `PhysicalDevice` + queue family WITHOUT a surface. `auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT)` — `examples/app/vsgheadless/vsgheadless.cpp:562`. This is the surface-less overload returning `std::pair<ref_ptr<PhysicalDevice>, int>` — `include/vsg/vk/Instance.h:73`. (The presentation overload that also takes a `Surface*` and returns a `std::tuple<…, int, int>` at `include/vsg/vk/Instance.h:76` is deliberately NOT used.)

4. Create the logical `Device`. Build `vsg::QueueSettings` from `queueFamily` (`examples/app/vsgheadless/vsgheadless.cpp:570`), then `vsg::Device::create(physicalDevice, queueSettings, validatedNames, deviceExtensions, deviceFeatures)` — `examples/app/vsgheadless/vsgheadless.cpp:576`. The constructor signature is `Device(PhysicalDevice*, const QueueSettings&, Names layers, Names deviceExtensions, const DeviceFeatures* = nullptr, …)` — `include/vsg/vk/Device.h:42`. No swapchain extension is enabled.

5. Set up the camera against the scene bounds — `examples/app/vsgheadless/vsgheadless.cpp:598` (`vsg::ViewportState::create(extent)` so the viewport matches the offscreen extent).

6. Build your OWN offscreen target:
   - Color `ImageView` via `createColorImageView(device, extent, imageFormat, VK_SAMPLE_COUNT_1_BIT)` — `examples/app/vsgheadless/vsgheadless.cpp:611`. Internally `vsg::createImageView(device, colorImage, VK_IMAGE_ASPECT_COLOR_BIT)` — `examples/app/vsgheadless/vsgheadless.cpp:213`; that `Device*` overload is declared at `include/vsg/state/ImageView.h:68`.
   - Depth `ImageView` via `createDepthImageView(...)` — `examples/app/vsgheadless/vsgheadless.cpp:612` (uses `vsg::createImageView(device, depthImage, vsg::computeAspectFlagsForFormat(depthFormat))` — `examples/app/vsgheadless/vsgheadless.cpp:231`).
   - `RenderPass` via `vsg::createOffscreenRenderPass(device, imageFormat, depthFormat, true)` — `examples/app/vsgheadless/vsgheadless.cpp:615`, which ends in `RenderPass::create(device, attachments, subpasses, dependencies)` — `examples/app/vsgheadless/vsgheadless.cpp:80`. The `RenderPass` ctor is `RenderPass(Device*, const Attachments&, const Subpasses&, const Dependencies&, …)` — `include/vsg/vk/RenderPass.h:93`.
   - `Framebuffer` via `vsg::Framebuffer::create(renderPass, vsg::ImageViews{colorImageView, depthImageView}, extent.width, extent.height, 1)` — `examples/app/vsgheadless/vsgheadless.cpp:616`. Ctor: `Framebuffer(ref_ptr<RenderPass>, const ImageViews&, uint32_t width, uint32_t height, uint32_t layers)` — `include/vsg/vk/Framebuffer.h:25`. `ImageViews` is `std::vector<ref_ptr<ImageView>>` — `include/vsg/state/ImageView.h:62`.

7. Build the capture subgraphs (these are how pixels escape):
   - `std::tie(colorBufferCapture, copiedColorBuffer) = createColorCapture(device, extent, colorImageView->image, imageFormat)` — `examples/app/vsgheadless/vsgheadless.cpp:628`.
   - `std::tie(depthBufferCapture, copiedDepthBuffer) = createDepthCapture(device, extent, depthImageView->image, depthFormat)` — `examples/app/vsgheadless/vsgheadless.cpp:629`.

8. Wire the render `RenderGraph` against the framebuffer (NOT a window). `auto renderGraph = vsg::RenderGraph::create()`; `renderGraph->framebuffer = framebuffer`; set `renderArea` to `extent`; `setClearValues(...)` — `examples/app/vsgheadless/vsgheadless.cpp:651`. Then `auto view = vsg::View::create(camera, vsg_scene)` and add a headlight — `examples/app/vsgheadless/vsgheadless.cpp:658`; `renderGraph->addChild(view)` — `examples/app/vsgheadless/vsgheadless.cpp:676`.

9. Create the `CommandGraph` from `(device, queueFamily)` — the surface-less overload. `auto commandGraph = vsg::CommandGraph::create(device, queueFamily)` — `examples/app/vsgheadless/vsgheadless.cpp:678`. The constructor is `CommandGraph(ref_ptr<Device> in_device, int family)` — `include/vsg/app/CommandGraph.h:31` (distinct from the `Window`-based ctor at `include/vsg/app/CommandGraph.h:32`). Add the render graph and the capture commands as children: `commandGraph->addChild(renderGraph)` (:679), `commandGraph->addChild(colorBufferCapture)` (:681), `commandGraph->addChild(depthBufferCapture)` (:682). The captures run after the render in the same submission.

10. Hand the command graphs to the viewer. `viewer->assignRecordAndSubmitTaskAndPresentation(commandGraphs)` — `examples/app/vsgheadless/vsgheadless.cpp:694`. Because no command graph carries a present-capable family (`presentFamily` stays `-1`, default at `include/vsg/app/CommandGraph.h:41`), the viewer creates a `RecordAndSubmitTask` but NO `Presentation` (see Related → foundations). Then `viewer->compile()` — `examples/app/vsgheadless/vsgheadless.cpp:696`.

11. Run the loop with NO `present()`. `while (viewer->advanceToNextFrame() && (numFrames--) > 0)` — `examples/app/vsgheadless/vsgheadless.cpp:701`; inside: `viewer->handleEvents()` (:780), `viewer->update()` (:782), `viewer->recordAndSubmit()` (:784). There is no `viewer->present()` call anywhere in the loop.

12. Read pixels back. After recording, `viewer->waitForFences(0, waitTimeout)` — `examples/app/vsgheadless/vsgheadless.cpp:789`. Query the device row pitch with `vkGetImageSubresourceLayout` — `examples/app/vsgheadless/vsgheadless.cpp:795`. Map the host-visible memory as a `vsg::ubvec4Array2D` via `vsg::MappedData<vsg::ubvec4Array2D>::create(deviceMemory, offset, 0, Properties{imageFormat}, width, height)` (auto-unmaps on destruction) — `examples/app/vsgheadless/vsgheadless.cpp:804`; if `rowPitch` doesn't match the tight row width, it copies row-by-row through a flat `vsg::ubyteArray` instead — `examples/app/vsgheadless/vsgheadless.cpp:810`. Finally `vsg::write(imageData, colorFilename)` — `examples/app/vsgheadless/vsgheadless.cpp:818` (depth path: `examples/app/vsgheadless/vsgheadless.cpp:829`/`:837`).

### The capture-command shape (inside `createColorCapture`)

The capture `Commands` subgraph is a sequence of barriers + a copy (`examples/app/vsgheadless/vsgheadless.cpp:234`):
- Barrier: destination image `UNDEFINED → TRANSFER_DST_OPTIMAL` and source color image to `TRANSFER_SRC_OPTIMAL` via two `vsg::ImageMemoryBarrier::create(...)` wrapped in a `vsg::PipelineBarrier::create(...)` — `examples/app/vsgheadless/vsgheadless.cpp:291` / :303 / :314.
- Copy: if blit is supported, `vsg::BlitImage::create()` (which can also convert format to `R8G8B8A8_UNORM`) — `examples/app/vsgheadless/vsgheadless.cpp:337`; otherwise `vsg::CopyImage::create()` — `examples/app/vsgheadless/vsgheadless.cpp:360`.
- Barrier: destination image `TRANSFER_DST_OPTIMAL → GENERAL` so host memory can be mapped — `examples/app/vsgheadless/vsgheadless.cpp:371`.
- Depth capture uses `vsg::CopyImageToBuffer::create()` into a host-visible buffer instead — `examples/app/vsgheadless/vsgheadless.cpp:451`.

## Distilled snippet — Device from Instance (surface-less)

```cpp
// 1) Instance — no window/surface extensions needed for headless.
auto instance = vsg::Instance::create(instanceExtensions, validatedLayers, vulkanVersion);

// 2) Graphics-capable PhysicalDevice + queue family, WITHOUT a Surface.
//    Surface-less overload returns std::pair<ref_ptr<PhysicalDevice>, int>.
auto [physicalDevice, queueFamily] = instance->getPhysicalDeviceAndQueueFamily(VK_QUEUE_GRAPHICS_BIT);
if (!physicalDevice || queueFamily < 0) return 0;

// 3) Logical Device — no swapchain extension enabled.
vsg::QueueSettings queueSettings{vsg::QueueSetting{queueFamily, {1.0}}};
auto deviceFeatures = vsg::DeviceFeatures::create();
auto device = vsg::Device::create(physicalDevice, queueSettings, validatedLayers, deviceExtensions, deviceFeatures);
```
(Mirrors `examples/app/vsgheadless/vsgheadless.cpp:561`-`:576`.)

## Key types

- `vsg::Instance` — `include/vsg/vk/Instance.h:48`. `Instance::create(...)` (via `Inherit`); surface-less queue lookup `getPhysicalDeviceAndQueueFamily(VkQueueFlags, …)` → `std::pair<ref_ptr<PhysicalDevice>, int>` at `include/vsg/vk/Instance.h:73`.
- `vsg::Device` — `include/vsg/vk/Device.h:39`; ctor at `include/vsg/vk/Device.h:42`; `deviceID` member used for per-device buffers at `include/vsg/vk/Device.h:48`. `vsg::QueueSetting` / `QueueSettings` at `include/vsg/vk/Device.h:30`-`:36`.
- `vsg::RenderPass` — `include/vsg/vk/RenderPass.h:85`; ctor `include/vsg/vk/RenderPass.h:93`; `Attachments`/`Subpasses`/`Dependencies` typedefs at `include/vsg/vk/RenderPass.h:88`-`:90`; free helpers `createRenderPass`/`createMultisampledRenderPass` at `include/vsg/vk/RenderPass.h:121`-`:130` (the offscreen variants are defined locally in the example, not in this header).
- `vsg::ImageView` — `include/vsg/state/ImageView.h:24`; `vk(deviceID)` accessor at `include/vsg/state/ImageView.h:39`; `using ImageViews = std::vector<ref_ptr<ImageView>>` at `include/vsg/state/ImageView.h:62`; `createImageView(Device*, ref_ptr<Image>, VkImageAspectFlags)` at `include/vsg/state/ImageView.h:68`.
- `vsg::Framebuffer` — `include/vsg/vk/Framebuffer.h:22`; ctor `include/vsg/vk/Framebuffer.h:25`; `extent2D()` / `getRenderPass()` at `include/vsg/vk/Framebuffer.h:43` / `:33`.
- `vsg::CommandGraph` — `include/vsg/app/CommandGraph.h:27`; surface-less ctor `CommandGraph(ref_ptr<Device>, int family)` at `include/vsg/app/CommandGraph.h:31`; `framebuffer` member at `include/vsg/app/CommandGraph.h:36`; `queueFamily`/`presentFamily` at `include/vsg/app/CommandGraph.h:40`-`:41`.

## Platform note

On macOS via MoltenVK, surface-less offscreen pipeline compilation can fail at record time: `vsg::BindGraphicsPipeline::record` dereferences a null per-view pipeline-implementation array, causing `EXC_BAD_ACCESS` / a segfault on the first frame. This was confirmed environmental, not a wiring bug — the official `examples/app/vsgheadless/vsgheadless.cpp` program reproduces the identical crash (same instruction offset) on the same host. Native-Vulkan hosts (Linux/Windows) are unaffected. If you must render headless on macOS, render to a window-backed swapchain and capture from it instead (see `examples/app/vsgscreenshot/vsgscreenshot.cpp`). This is an observed-on-macOS/MoltenVK runtime limitation, not a header-documented behavior.

## Related

- `../foundations/application-and-rendering.md` — why a command-graph group with no present family gets a `RecordAndSubmitTask` but no `Presentation`.
- `examples/app/vsgscreenshot/vsgscreenshot.cpp` — windowed capture; the recommended path on macOS/MoltenVK and the source of the same image-copy barrier pattern.
- `examples/app/vsgheadless/vsgheadless.cpp` — the full canonical headless program cited throughout.
