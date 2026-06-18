---
title: Multi-view & multi-window (several Views / Windows)
description: Compose several vsg::View instances in one window (side-by-side, inset/overlay) or drive several vsg::Window instances, controlling per-view screen rectangles, depth clears and trackballs.
---

## Public include

```cpp
#include <vsg/all.h>                       // pulls in everything below
// or, granularly:
#include <vsg/app/View.h>                  // vsg::View
#include <vsg/app/RenderGraph.h>           // vsg::RenderGraph, vsg::createRenderGraphForView
#include <vsg/app/Camera.h>                // vsg::Camera (getViewport / getRenderArea)
#include <vsg/state/ViewportState.h>       // vsg::ViewportState (viewport + scissor)
#include <vsg/commands/ClearAttachments.h> // vsg::ClearAttachments (scissored clear)
#include <vsg/app/Trackball.h>             // vsg::Trackball (one per camera)
```

## When to use

- You need **two or more viewpoints in a single window**: side-by-side split, a picture-in-picture inset, or a HUD-style overlay. Use **multiple `vsg::View` children in one `vsg::RenderGraph`** — one render pass, one window.
- You need **two or more OS windows** driven by one `vsg::Viewer`. Use **one `vsg::CommandGraph` per `vsg::Window`** and submit them together via `assignRecordAndSubmitTaskAndPresentation`.
- The deciding factor: a `vsg::RenderGraph` wraps a single `vkCmdBeginRenderPass`/`vkCmdEndRenderPass` pair bound to one window/framebuffer (`include/vsg/app/RenderGraph.h:25-27`). Multiple views into the **same** surface ⇒ multi-View; multiple surfaces ⇒ multi-Window.

## Key API

- `vsg::View::create(camera, scenegraph, features = RECORD_ALL)` — a `View` is a `Group` pairing one `Camera` with a scene subgraph; ctor at `include/vsg/app/View.h:43`. Each `View` gets an auto-assigned `viewID` (`include/vsg/app/View.h:70`) and carries its own `camera` (`include/vsg/app/View.h:67`).
- `vsg::ViewportState::create(x, y, width, height)` — builds a single viewport+scissor pair at a screen rectangle; ctor at `include/vsg/state/ViewportState.h:33`. **This one object holds BOTH the Vulkan `viewports` and `scissors`** (`include/vsg/state/ViewportState.h:36-37`), so it defines the on-screen rectangle for its view. `set(x,y,w,h)` fills `viewports[0]` plus a matching `scissors[0]` (`src/vsg/state/ViewportState.cpp:58-73`), and `ViewportState::record()` binds both via `vkCmdSetViewport`/`vkCmdSetScissor` (`src/vsg/state/ViewportState.cpp:142-145`).
- `vsg::Camera::create(projection, viewMatrix, viewportState)` — `getViewport()` returns the `VkViewport` and `getRenderArea()` returns the scissor `VkRect2D`, both delegating to the camera's `viewportState` (`include/vsg/app/Camera.h:38-39`). `getRenderArea()` is what you feed into a `VkClearRect`.
- `vsg::RenderGraph::create(window)` (`include/vsg/app/RenderGraph.h:34`) then `renderGraph->addChild(view)` — add several `View` (and `ClearAttachments`) children; record order = child order (`include/vsg/nodes/Group.h:39`).
- `vsg::createRenderGraphForView(window, camera, scenegraph)` — convenience one-shot that builds a RenderGraph + View + resize handler for a single full-window view (`include/vsg/app/RenderGraph.h:88`).
- `vsg::ClearAttachments::create(Attachments{...}, Rects{...})` — scissored `vkCmdClearAttachments`; `Attachments` is `std::vector<VkClearAttachment>`, `Rects` is `std::vector<VkClearRect>` (`include/vsg/commands/ClearAttachments.h:24-25,28`). Placed as a RenderGraph child **between** two views to give the later view a fresh color/depth within a rectangle.
- `vsg::Trackball::create(camera)` — one trackball per camera; it mutates only that camera (`examples/app/vsgmultiviews/vsgmultiviews.cpp:214-215`). For multi-window, restrict a trackball to its window with `trackball->addWindow(window)` (`include/vsg/app/Trackball.h:65`).
- `viewer->assignRecordAndSubmitTaskAndPresentation({cg1, cg2, ...})` — submit one CommandGraph per window (`include/vsg/app/Viewer.h:124`); `viewer->setupThreading()` optionally records windows in parallel (`include/vsg/app/Viewer.h:132`).

## Best Practices

- **Side-by-side** = two `View` children whose cameras have **disjoint** `ViewportState` rectangles in ONE RenderGraph. One render pass, the RenderGraph's single clear covers the whole window; no per-view depth-clear trick is needed because the rectangles do not overlap.
- **Inset / overlay** = views whose rectangles overlap (e.g. inset in a corner). Insert a **scissored `ClearAttachments` between** the main view and the inset view so the inset gets its own background color and a fresh depth buffer, independent of the main view behind it (`examples/app/vsgmultiviews/vsgmultiviews.cpp:185-201`).
- Build the inset `VkClearRect` from the inset camera's scissor: `VkClearRect rect{secondary_camera->getRenderArea(), 0, 1}` (`examples/app/vsgmultiviews/vsgmultiviews.cpp:196`). Using `getRenderArea()` keeps the clear exactly aligned with the inset's `ViewportState`.
- Give **each view its own headlight** child so lit geometry is lit in every view (`examples/app/vsgmultiviews/vsgmultiviews.cpp:178-180`).
- **One Trackball per camera.** Add them in the order you want events handled; in an inset layout add the inset trackball before the main one so the smaller, topmost rectangle gets first chance at the pointer (`examples/app/vsgmultiviews/vsgmultiviews.cpp:214-215`).
- For **multi-window**, share the device by default — `windowTraits2->device = window1->getOrCreateDevice()` — and only use separate devices when explicitly required (`examples/app/vsgwindows/vsgwindows.cpp:142`). Constrain each window's trackball with `addWindow` (`examples/app/vsgwindows/vsgwindows.cpp:178,182`).
- See **commandgraph.md** for the View/RenderGraph/CommandGraph nesting (CommandGraph → RenderGraph → View → scene).

## Composition examples

Inset / overlay — two Views in one RenderGraph with a scissored depth+color clear between them (distilled from `examples/app/vsgmultiviews/vsgmultiviews.cpp`, which builds):

```cpp
// helper: camera whose ViewportState carves a screen rectangle (x,y,w,h)
vsg::ref_ptr<vsg::Camera> createCameraForScene(vsg::Node* scene,
    int32_t x, int32_t y, uint32_t w, uint32_t h)
{
    vsg::ComputeBounds cb; scene->accept(cb);
    auto centre = (cb.bounds.min + cb.bounds.max) * 0.5;
    double radius = vsg::length(cb.bounds.max - cb.bounds.min) * 0.6;
    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                      centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto persp = vsg::Perspective::create(30.0, double(w) / double(h),
                                          0.001 * radius, radius * 4.5);
    auto vp = vsg::ViewportState::create(x, y, w, h);   // viewport + scissor
    return vsg::Camera::create(persp, lookAt, vp);
}

uint32_t width = window->extent2D().width, height = window->extent2D().height;

// main view: full window
auto main_camera = createCameraForScene(scene, 0, 0, width, height);
auto main_view   = vsg::View::create(main_camera, scene);

// inset view: top-right quarter
auto inset_camera = createCameraForScene(scene2, (width * 3) / 4, 0, width / 4, height / 4);
auto inset_view   = vsg::View::create(inset_camera, scene2);

main_view->addChild(vsg::createHeadlight());
inset_view->addChild(vsg::createHeadlight());

auto renderGraph = vsg::RenderGraph::create(window);
renderGraph->addChild(main_view);                       // (1) main, full window

// (2) clear color + depth ONLY inside the inset rectangle
VkClearValue colorClear{}; colorClear.color = vsg::sRGB_to_linear(0.2f, 0.2f, 0.2f, 1.0f);
VkClearAttachment colorAtt{VK_IMAGE_ASPECT_COLOR_BIT, 0, colorClear};
VkClearValue depthClear{}; depthClear.depthStencil = {0.0f, 0};
VkClearAttachment depthAtt{VK_IMAGE_ASPECT_DEPTH_BIT, 1, depthClear};
VkClearRect rect{inset_camera->getRenderArea(), 0, 1}; // scissor of the inset
renderGraph->addChild(vsg::ClearAttachments::create(
    vsg::ClearAttachments::Attachments{colorAtt, depthAtt},
    vsg::ClearAttachments::Rects{rect, rect}));

renderGraph->addChild(inset_view);                      // (3) inset, on top

auto commandGraph = vsg::CommandGraph::create(window);
commandGraph->addChild(renderGraph);
viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

viewer->addEventHandler(vsg::Trackball::create(inset_camera)); // inset first
viewer->addEventHandler(vsg::Trackball::create(main_camera));
viewer->compile();
```

Multi-window — one CommandGraph per Window, submitted together (distilled from `examples/app/vsgwindows/vsgwindows.cpp`):

```cpp
auto window1 = vsg::Window::create(windowTraits);
windowTraits2->device = window1->getOrCreateDevice();   // share Instance/Device
auto window2 = vsg::Window::create(windowTraits2);
viewer->addWindow(window1);
viewer->addWindow(window2);

auto main_camera = createCameraForScene(scene,  0, 0, window1->extent2D().width, window1->extent2D().height);
auto sec_camera  = createCameraForScene(scene2, 0, 0, window2->extent2D().width, window2->extent2D().height);
auto main_view   = vsg::View::create(main_camera, scene);
auto sec_view    = vsg::View::create(sec_camera,  scene2);
main_view->addChild(vsg::createHeadlight());
sec_view->addChild(vsg::createHeadlight());

// one trackball per window
auto t1 = vsg::Trackball::create(main_camera); t1->addWindow(window1); viewer->addEventHandler(t1);
auto t2 = vsg::Trackball::create(sec_camera);  t2->addWindow(window2); viewer->addEventHandler(t2);

auto rg1 = vsg::RenderGraph::create(window1, main_view); // RenderGraph(window, view) ctor
auto rg2 = vsg::RenderGraph::create(window2, sec_view);
auto cg1 = vsg::CommandGraph::create(window1); cg1->addChild(rg1);
auto cg2 = vsg::CommandGraph::create(window2); cg2->addChild(rg2);
viewer->assignRecordAndSubmitTaskAndPresentation({cg1, cg2}); // one per window
// viewer->setupThreading();  // optional parallel record
viewer->compile();
```

## Source references

- `include/vsg/app/View.h:35,43,67,70` — `View` is a `Group`; ctor `View(camera, scenegraph, features)`; `camera`; auto `viewID`.
- `include/vsg/app/RenderGraph.h:25-27,34,88` — RenderGraph = one render-pass scope; `RenderGraph(window, view)` ctor; `createRenderGraphForView`.
- `include/vsg/state/ViewportState.h:33,36-37,39` — `ViewportState(x,y,w,h)`; holds `viewports` AND `scissors`; `set()`.
- `src/vsg/state/ViewportState.cpp:58-73,142-145` — `set(x,y,w,h)` fills one viewport + scissor; `record()` binds them via `vkCmdSetViewport`/`vkCmdSetScissor`.
- `include/vsg/app/Camera.h:31,34,36,38-39` — `Camera(projection, view, viewportState)`; `getViewport()`; `getRenderArea()` (= scissor).
- `include/vsg/commands/ClearAttachments.h:24-25,28` — `Attachments`/`Rects` typedefs; `ClearAttachments(attachments, rects)` ctor.
- `include/vsg/app/Trackball.h:65` — `Trackball::addWindow(window, offset = {})`.
- `include/vsg/app/Viewer.h:124,132` — `assignRecordAndSubmitTaskAndPresentation(CommandGraphs)`; `setupThreading()`.
- `examples/app/vsgmultiviews/vsgmultiviews.cpp:10-29,170-201,214-215` — `createCameraForScene`; inset wiring with scissored `ClearAttachments`; per-camera trackballs.
- `examples/app/vsgoverlay/vsgoverlay.cpp:86-104` — two Views in one RenderGraph with a depth-only `ClearAttachments` between them.
- `examples/app/vsgwindows/vsgwindows.cpp:142,156-195` — shared device; per-window RenderGraph/CommandGraph; `addWindow` on trackballs; multi-CommandGraph submit.

## Common mistakes

- **Sharing one camera between two views.** Each `View` must own its `Camera` (and thus its `ViewportState`); the trackball mutates the camera, so a shared camera makes both views move together. Build a separate camera per view (`examples/app/vsgmultiviews/vsgmultiviews.cpp:170-175`).
- **Forgetting the depth clear for overlapping views.** Without a `ClearAttachments` between an overlay and the view behind it, the overlay's depth tests against stale depth and z-fights / disappears. The `vsgoverlay` example clears depth between the two full-window views (`examples/app/vsgoverlay/vsgoverlay.cpp:93-99`).
- **Mismatched clear rect.** Build the `VkClearRect` from the overlay camera's `getRenderArea()`, not a hand-typed rectangle, so the clear matches the view's scissor exactly (`examples/app/vsgmultiviews/vsgmultiviews.cpp:196`).
- **Putting multiple windows under one CommandGraph.** Each `Window` needs its own `CommandGraph`; submit them as a vector to `assignRecordAndSubmitTaskAndPresentation` (`examples/app/vsgwindows/vsgwindows.cpp:189-195`).
- **Trackball grabbing events on the wrong window.** In multi-window apps call `trackball->addWindow(window)` so each trackball only reacts to its own window (`examples/app/vsgwindows/vsgwindows.cpp:178,182`).

## Things to never invent

- There is **no** `View::setViewport(...)` / `View::viewport` — the screen rectangle lives on the camera's `ViewportState`, reached via `camera->getViewport()` / `camera->getRenderArea()` (`include/vsg/app/Camera.h:36-39`). `View` exposes `camera`, `viewID`, `features`, `mask`, `LODScale`, `bins`, `viewDependentState`, `overridePipelineStates` and nothing else view-positional (`include/vsg/app/View.h:67-90`).
- `ViewportState` only declares `viewports`, `scissors`, `set(x,y,w,h)`, `getViewport()`, `getScissor()` — do not assume helpers like `setExtent`/`resize` beyond these (`include/vsg/state/ViewportState.h:36-46`).
- `ClearAttachments` takes exactly `(Attachments, Rects)` (vectors of `VkClearAttachment` / `VkClearRect`); there is no single-rect convenience overload in the header (`include/vsg/commands/ClearAttachments.h:27-28`).
- Do not call `renderGraph->setViewportState(...)`; the header field is `viewportState` and it is auto-synced with `renderArea` — there is no such setter declared (`include/vsg/app/RenderGraph.h:55`).
- `Trackball::addWindow` is the only window-scoping entry point shown; there is no `removeWindow`/`setWindow` claimed here — cite the header before using anything else (`include/vsg/app/Trackball.h:65`).
