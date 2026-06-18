---
title: CommandGraph / RenderGraph / View
description: The recording wiring layer — CommandGraph manages Vulkan command-buffer recording, RenderGraph wraps a render pass, and View pairs a Camera with the scene subgraph.
---

## Public include
Prefer the barrel include; the namespace is `vsg::`.

```cpp
#include <vsg/all.h>          // barrel include
// specific headers for reference:
#include <vsg/app/CommandGraph.h>   // include/vsg/app/CommandGraph.h:27
#include <vsg/app/RenderGraph.h>    // include/vsg/app/RenderGraph.h:28
#include <vsg/app/View.h>           // include/vsg/app/View.h:35
```

All three are `Group` subclasses declared in `namespace vsg` (`include/vsg/app/CommandGraph.h:23,27`, `include/vsg/app/RenderGraph.h:22,28`, `include/vsg/app/View.h:19,35`).

## When to use
These three nodes form the top of the renderable scene graph, in the order `CommandGraph -> RenderGraph -> View -> scene`. Use `CommandGraph` as the per-window/per-queue root that records its subgraph into Vulkan command buffers (`include/vsg/app/CommandGraph.h:26`); use `RenderGraph` to wrap a `vkCmdBeginRenderPass`/`vkCmdEndRenderPass` pair (`include/vsg/app/RenderGraph.h:25-27`); use `View` to bind a `Camera` to a scene subgraph (`include/vsg/app/View.h:34`). This is structural recording wiring — not for scene content (use `Group`/`StateGroup`/`Switch`/`MatrixTransform`) and not for the device/window/frame loop (use `Viewer`/`Window`). Multiple `View`s under one `RenderGraph` composite into the same render pass; multiple `RenderGraph`s under one `CommandGraph` chain render passes; multiple `CommandGraph`s drive multiple windows/queues.

## Key API
- `vsg::createCommandGraphForView(window, camera, scenegraph, contents = VK_SUBPASS_CONTENTS_INLINE, assignHeadlight = true)` — one-call convenience building `CommandGraph->RenderGraph->View->scene` (`include/vsg/app/CommandGraph.h:66`).
- `vsg::CommandGraph::create(window, child = {})` — construct a `CommandGraph` for a `Window`, optionally with a child node (`include/vsg/app/CommandGraph.h:32`).
- `vsg::CommandGraph::create(device, family)` — device+queue-family constructor for headless/secondary recording (`include/vsg/app/CommandGraph.h:31`).
- `CommandGraph::window`, `CommandGraph::framebuffer`, `CommandGraph::device` — recording targets; framebuffer takes precedence over window if set (`include/vsg/app/CommandGraph.h:35-38`).
- `CommandGraph::submitOrder` (default `0`) and `CommandGraph::queueFamily` (default `-1`), `CommandGraph::presentFamily` (default `-1`) (`include/vsg/app/CommandGraph.h:40-42`).
- `vsg::createRenderGraphForView(window, camera, scenegraph, contents = VK_SUBPASS_CONTENTS_INLINE, assignHeadlight = true)` — builds a `RenderGraph` + `View` only (`include/vsg/app/RenderGraph.h:88`).
- `vsg::RenderGraph::create(window, view = {})` — construct a `RenderGraph` for a `Window`, optionally seeding the first child `View`; sets up `clearValues` for the window attachments (`include/vsg/app/RenderGraph.h:34`).
- `RenderGraph::clearValues` and `RenderGraph::setClearValues(clearColor = {{0.2,0.2,0.4,1.0}}, clearDepthStencil = {0.0f, 0})` — clear configuration (`include/vsg/app/RenderGraph.h:62,66`).
- `RenderGraph::renderArea` (`VkRect2D`), `RenderGraph::contents` (default `VK_SUBPASS_CONTENTS_INLINE`), `RenderGraph::framebuffer`/`RenderGraph::window` (`include/vsg/app/RenderGraph.h:52,69,42-43`).
- `RenderGraph::getExtent()`, `RenderGraph::getRenderPass()` (`include/vsg/app/RenderGraph.h:49,46`).
- `vsg::View::create(camera, scenegraph = {}, features = RECORD_ALL)` — pair a `Camera` with a scene; `features` is a `ViewFeatures` mask (`include/vsg/app/View.h:43`).
- `vsg::View::create(features = RECORD_ALL)` — empty view; add scene children via `addChild` (`include/vsg/app/View.h:38`).
- `View::camera`, `View::viewID` (auto-assigned, `const`), `View::LODScale` (default `1.0`), `View::mask` (default `MASK_ALL`) (`include/vsg/app/View.h:67,70,81,78`).
- `ViewFeatures`: `INHERIT_VIEWPOINT`, `RECORD_LIGHTS`, `RECORD_SHADOW_MAPS`, `RECORD_ALL` (`include/vsg/app/View.h:26-32`).
- `viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph})` — register the `CommandGraphs` with the `Viewer` (`include/vsg/app/Viewer.h:124`).

## Best Practices
- All three are heap-allocated via `T::create(...)` returning `vsg::ref_ptr<T>`; never `new`. They derive via `Inherit<Group, T>` so they are `Group` nodes and use `addChild` to attach children (`include/vsg/app/CommandGraph.h:27`, `include/vsg/app/RenderGraph.h:28`, `include/vsg/app/View.h:35`).
- Build the hierarchy strictly `CommandGraph -> RenderGraph -> View -> scene`: attach the `View` to the `RenderGraph` (constructor arg or `addChild`), the `RenderGraph` to the `CommandGraph` (constructor arg or `addChild`), then hand the `CommandGraph` to the viewer (`examples/app/vsgviewer/vsgviewer.cpp:323-330`).
- Call `viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph})` and then `viewer->compile()` BEFORE entering the frame loop; recording wiring must be assigned and compiled first (`examples/app/vsgoverlay/vsgoverlay.cpp:120-122`).
- `RenderGraph::accept(RecordTraversal&)` issues the begin/end render pass and traverses children during the record traversal — this runs on the record thread, so do not mutate the graph mid-record (`include/vsg/app/RenderGraph.h:38-39`).
- `RenderGraph::create(window, view)` already configures `clearValues` for the window; only call `setClearValues(...)` to override colour/depth (`include/vsg/app/RenderGraph.h:34,64-66`).
- For lighting, add a headlight (or light) into the `View` subgraph; the convenience functions do this automatically via `assignHeadlight = true` (`include/vsg/app/CommandGraph.h:66`, `examples/app/vsgviewer/vsgviewer.cpp:325`).
- Each `View` gets an auto-assigned `const uint32_t viewID`; do not attempt to set it. Use `View::mask` to control subgraph traversal per view (`include/vsg/app/View.h:69-78`).
- Window resize handling is wired by default on `RenderGraph` (`windowResizeHandler`); the convenience `createRenderGraphForView` assigns it for you (`include/vsg/app/RenderGraph.h:73-75,87`).
- Multiple windows: build one `CommandGraph` per window and pass them all in the braced list, e.g. `assignRecordAndSubmitTaskAndPresentation({commandGraph1, commandGraph2})` (`examples/app/vsgwindows/vsgwindows.cpp:195`).

## Composition examples

Convenience one-liner (most common):

```cpp
#include <vsg/all.h>
// camera + scene already created; builds CommandGraph->RenderGraph->View->scene
auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene); // CommandGraph.h:66
viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});             // Viewer.h:124
viewer->compile();                                                            // before frame loop
// distilled from examples/app/vsgortho/vsgortho.cpp:91-94
```

Manual composition (when you need control over View/RenderGraph settings):

```cpp
#include <vsg/all.h>
auto view = vsg::View::create(camera);          // View.h:43 — pair camera with scene
view->LODScale = 1.0;                            // View.h:81
view->addChild(vsg::createHeadlight());          // lighting into the View subgraph
view->addChild(vsg_scene);                       // attach scene under the View

auto renderGraph = vsg::RenderGraph::create(window, view);   // RenderGraph.h:34 — wraps render pass, seeds View
auto commandGraph = vsg::CommandGraph::create(window, renderGraph); // CommandGraph.h:32 — records the subgraph

viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});  // Viewer.h:124
// distilled from examples/app/vsgviewer/vsgviewer.cpp:323-330
```

Multiple Views composited into one RenderGraph (overlay):

```cpp
#include <vsg/all.h>
auto renderGraph = vsg::RenderGraph::create(window);  // RenderGraph.h:31
renderGraph->addChild(view1);                          // first view
renderGraph->addChild(view2);                          // second view, same render pass
auto commandGraph = vsg::CommandGraph::create(window); // CommandGraph.h:32
commandGraph->addChild(renderGraph);                   // attach the render graph
viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
// distilled from examples/app/vsgoverlay/vsgoverlay.cpp:86-120
```

## Source references
- `include/vsg/app/CommandGraph.h` — `CommandGraph` declaration and `createCommandGraphForView` free function.
- `include/vsg/app/RenderGraph.h` — `RenderGraph` declaration and `createRenderGraphForView` free function.
- `include/vsg/app/View.h` — `View` declaration and `ViewFeatures` enum.
- `include/vsg/app/Viewer.h` — `assignRecordAndSubmitTaskAndPresentation`.
- `examples/app/vsgviewer/vsgviewer.cpp` — manual View/RenderGraph/CommandGraph wiring.
- `examples/app/vsgortho/vsgortho.cpp` — `createCommandGraphForView` convenience usage.
- `examples/app/vsgoverlay/vsgoverlay.cpp` — multiple Views under one RenderGraph.
- `examples/app/vsgwindows/vsgwindows.cpp` — multiple CommandGraphs for multiple windows.

## Common mistakes
- Allocating with `new CommandGraph(...)` -> use `vsg::CommandGraph::create(...)` which returns a `ref_ptr` (`include/vsg/app/CommandGraph.h:32`).
- Wiring in the wrong order (View at top) -> the order is `CommandGraph -> RenderGraph -> View -> scene` (`examples/app/vsgviewer/vsgviewer.cpp:323-329`).
- Entering the frame loop before `viewer->compile()` -> assign tasks then compile before looping (`examples/app/vsgoverlay/vsgoverlay.cpp:120-122`).
- Forgetting lighting -> add a headlight to the `View`, or pass `assignHeadlight = true` (the default) to the convenience functions (`include/vsg/app/CommandGraph.h:66`, `examples/app/vsgviewer/vsgviewer.cpp:325`).
- Passing a bare `commandGraph` -> `assignRecordAndSubmitTaskAndPresentation` takes a `CommandGraphs` (vector), so use brace-init `{commandGraph}` (`include/vsg/app/Viewer.h:124`).
- Hand-setting clear colours when unnecessary -> `RenderGraph::create(window, view)` already populates `clearValues`; only call `setClearValues(...)` to override (`include/vsg/app/RenderGraph.h:34,64-66`).

## Things to never invent
- `View` has no `setCamera`/`getCamera` accessors — `camera` is a public `ref_ptr<Camera>` member (`include/vsg/app/View.h:67`).
- Do not assign `View::viewID`; it is `const` and auto-assigned in the constructor (`include/vsg/app/View.h:69-70`).
- `CommandGraph` has no `addRenderGraph`/`setRenderGraph` — attach via the constructor child arg or `Group::addChild` (`include/vsg/app/CommandGraph.h:32`).
- `RenderGraph` has no `setCamera`/`addView` method — add the `View` via the constructor `view` arg or `addChild` (`include/vsg/app/RenderGraph.h:34`).
- There is no `createCommandGraph()` without arguments matching the View convenience — the convenience free functions are `createCommandGraphForView` and `createRenderGraphForView` (`include/vsg/app/CommandGraph.h:66`, `include/vsg/app/RenderGraph.h:88`).
- `RenderGraph::setRenderPass(...)` does not exist; `renderPass` is a public `ref_ptr<RenderPass>` member and `getRenderPass()` is the only accessor method (`include/vsg/app/RenderGraph.h:46,58`).
