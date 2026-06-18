---
title: Viewer
description: High-level render-loop driver that owns windows, event handlers, record-and-submit tasks and presentations, and drives the per-frame advance/handleEvents/update/recordAndSubmit/present cycle.
---

## Public include
Consumers use the barrel include and the `vsg::` namespace:

```cpp
#include <vsg/all.h>      // barrel; pulls in vsg/app/Viewer.h
// specific header for reference:
#include <vsg/app/Viewer.h>
```

`Viewer` is declared in `include/vsg/app/Viewer.h:32` inside `namespace vsg` (`include/vsg/app/Viewer.h:27`).

## When to use
Use `vsg::Viewer` as the top-level driver of any interactive VSG application: it owns the `Window`(s), dispatches UI events, and runs the record/submit/present frame loop (`include/vsg/app/Viewer.h:30`). It is not the scene graph and not a window — attach a `vsg::Window` to it via `addWindow` (`include/vsg/app/Viewer.h:41`), and build the scene from `vsg::Node`/`vsg::Group` subgraphs wired into a `CommandGraph`. For purely offscreen/headless rendering you still use a `Viewer`, but drive frames manually rather than relying on window events (see `examples/app/vsgheadless/vsgheadless.cpp:696`).

## Key API
- `vsg::Viewer::create()` — factory returning `ref_ptr<Viewer>`; `Viewer` derives from `Inherit<Object, Viewer>` (`include/vsg/app/Viewer.h:32`).
- `addWindow(ref_ptr<Window>)` — attach a window to the viewer (`include/vsg/app/Viewer.h:41`).
- `addEventHandler(ref_ptr<Visitor>)` — register one event handler, e.g. `CloseHandler`, `Trackball` (`include/vsg/app/Viewer.h:71`).
- `addEventHandlers(const EventHandlers&)` — register a batch of handlers (`include/vsg/app/Viewer.h:73`).
- `assignRecordAndSubmitTaskAndPresentation(CommandGraphs)` — create the `RecordAndSubmitTask`/`Presentation` objects for the given command graphs, replacing any prior setup (`include/vsg/app/Viewer.h:122`, `include/vsg/app/Viewer.h:124`).
- `addRecordAndSubmitTaskAndPresentation(CommandGraphs)` — additive variant that keeps existing tasks (`include/vsg/app/Viewer.h:126`).
- `compile(ref_ptr<ResourceHints> hints = {})` — compile all Vulkan objects and upload GPU data; returns `CompileResult` (`include/vsg/app/Viewer.h:107`).
- `advanceToNextFrame(double simulationTime = UseTimeSinceStartPoint)` — checks `active()`, polls events, advances `FrameStamp`; returns `false` when the viewer is no longer active (`include/vsg/app/Viewer.h:99`, `include/vsg/app/Viewer.h:102`).
- `handleEvents()` — pass polled events to registered handlers (`include/vsg/app/Viewer.h:105`).
- `update()` — run queued update operations / animations (`include/vsg/app/Viewer.h:135`).
- `recordAndSubmit()` — record command buffers and submit to the GPU queues (`include/vsg/app/Viewer.h:137`).
- `present()` — present the rendered swapchain images (`include/vsg/app/Viewer.h:139`).
- `active()` — true while the viewer is valid and running (`include/vsg/app/Viewer.h:56`); `close()` schedules shutdown so `active()` returns false (`include/vsg/app/Viewer.h:59`).
- `setupThreading()` / `stopThreading()` — opt into multi-threaded record/submit (`include/vsg/app/Viewer.h:132`, `include/vsg/app/Viewer.h:133`).
- `addUpdateOperation(ref_ptr<Operation>, UpdateOperations::RunBehavior = UpdateOperations::ONE_TIME)` — queue an update op; default runs once (`include/vsg/app/Viewer.h:85`).
- `getFrameStamp()` — current `FrameStamp*` for timing/frame count (`include/vsg/app/Viewer.h:52`).
- `deviceWaitIdle()` — `vkDeviceWaitIdle` on all devices, e.g. before teardown/resource changes (`include/vsg/app/Viewer.h:142`).
- `UseTimeSinceStartPoint` — `static constexpr double` sentinel default for `simulationTime` (`include/vsg/app/Viewer.h:97`).
- Free function `vsg::updateViewer(Viewer&, const CompileResult&)` — update viewer structures after compiling new subgraphs at runtime (`include/vsg/app/Viewer.h:172`).

## Best Practices

### Lifecycle (wire, then compile, then loop)
- Always allocate via `vsg::Viewer::create()` into a `ref_ptr<Viewer>`; never `new Viewer` (factory inherited through `Inherit<Object, Viewer>`, `include/vsg/app/Viewer.h:32`). The copy ctor and assignment are deleted, so pass it by `ref_ptr` (`include/vsg/app/Viewer.h:37`, `include/vsg/app/Viewer.h:38`).
- Order is load-bearing: `addWindow` -> set up camera/event handlers -> `assignRecordAndSubmitTaskAndPresentation({commandGraph})` -> `compile()` -> frame loop. `compile()` MUST run exactly once after the scene and tasks are wired and before the first `advanceToNextFrame()` (`examples/app/vsghelloworld/vsghelloworld.cpp:71`, `examples/app/vsghelloworld/vsghelloworld.cpp:74`, `examples/app/vsghelloworld/vsghelloworld.cpp:77`).
- The canonical loop body is exactly `advanceToNextFrame()` as the `while` condition, then `handleEvents()`, `update()`, `recordAndSubmit()`, `present()` in that order (`examples/app/vsghelloworld/vsghelloworld.cpp:77-87`).
- Add a `CloseHandler` event handler so the window close button / Escape can stop the loop — it flips `active()` to false (`examples/app/vsghelloworld/vsghelloworld.cpp:64`; `close()` semantics at `include/vsg/app/Viewer.h:59`).

### Behavior
- `advanceToNextFrame()` already polls events internally and returns false when inactive, so use it as the loop condition rather than calling `pollEvents()` yourself (`include/vsg/app/Viewer.h:99-102`).
- If you compile new subgraphs at runtime (via `compileManager`, `include/vsg/app/Viewer.h:94`), call the free function `vsg::updateViewer(viewer, compileResult)` to fold the new resources into the viewer (`include/vsg/app/Viewer.h:171`, `include/vsg/app/Viewer.h:172`).

### Lifecycle & threading
- Cleanup is automatic: when the last `ref_ptr<Viewer>` drops, the protected destructor runs (`include/vsg/app/Viewer.h:152`). Do not `delete` it.
- For multi-threaded recording call `setupThreading()` after `compile()` and before/within the loop; examples gate it behind a flag (`examples/app/vsgviewer/vsgviewer.cpp:336`, `include/vsg/app/Viewer.h:132`).
- Call `deviceWaitIdle()` before tearing down GPU resources to ensure no in-flight frames reference them (`include/vsg/app/Viewer.h:142`).

## Composition examples

Distilled faithfully from `examples/app/vsghelloworld/vsghelloworld.cpp:29-90`:

```cpp
#include <vsg/all.h>

int main(int argc, char** argv)
{
    auto windowTraits = vsg::WindowTraits::create();

    // create the viewer and attach a window
    auto viewer = vsg::Viewer::create();          // ref_ptr<Viewer>, never `new`
    auto window = vsg::Window::create(windowTraits);
    if (!window) return 1;
    viewer->addWindow(window);

    // ... build vsg_scene (a ref_ptr<vsg::Node>) and a camera ...

    // event handlers: CloseHandler lets the window close / Esc stop the loop
    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    // wire the command graph as a record-and-submit task + presentation
    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

    // compile MUST happen once, after wiring, before the loop
    viewer->compile();

    // canonical frame loop
    while (viewer->advanceToNextFrame())   // false when no longer active
    {
        viewer->handleEvents();   // dispatch events to handlers
        viewer->update();         // run queued update operations
        viewer->recordAndSubmit();// record + submit to GPU
        viewer->present();        // present swapchain image
    }
    return 0; // ref_ptr cleans everything up
}
```

## Source references
- `include/vsg/app/Viewer.h` — `Viewer` class declaration, frame-loop methods, `updateViewer` free function.
- `examples/app/vsghelloworld/vsghelloworld.cpp` — canonical wire/compile/loop sequence drawn from lines 29-90.
- `examples/app/vsgviewer/vsgviewer.cpp` — `setupThreading()` usage (line 336).
- `examples/app/vsgheadless/vsgheadless.cpp` — `compile()` in an offscreen/headless flow (line 696).

## Common mistakes
- Calling `viewer->compile()` before `assignRecordAndSubmitTaskAndPresentation(...)` or before `addWindow(...)` -> wire the window and tasks first, then compile once (`examples/app/vsghelloworld/vsghelloworld.cpp:71-74`).
- Forgetting `compile()` entirely and jumping into the loop -> always compile once after wiring and before `advanceToNextFrame()` (`include/vsg/app/Viewer.h:107`).
- Hand-rolling `while (viewer->active()) { pollEvents(); ... }` -> use `while (viewer->advanceToNextFrame())`, which polls and reports inactivity for you (`include/vsg/app/Viewer.h:99-102`).
- Omitting a `CloseHandler` so the window cannot stop the loop -> add `vsg::CloseHandler::create(viewer)` as an event handler (`examples/app/vsghelloworld/vsghelloworld.cpp:64`).
- Constructing with `new vsg::Viewer` or copying the viewer -> use `vsg::Viewer::create()` and pass `ref_ptr<Viewer>` (copy/assign are deleted, `include/vsg/app/Viewer.h:37-38`).

## Things to never invent
- There is no `viewer->run()` / `viewer->renderLoop()` / `viewer->frame()` all-in-one method — you drive `advanceToNextFrame`/`handleEvents`/`update`/`recordAndSubmit`/`present` yourself (`include/vsg/app/Viewer.h:99-139`).
- There is no `setScene()` / `setSceneData()` / `addChild()` on `Viewer` — the scene is wired through a `CommandGraph` and `assignRecordAndSubmitTaskAndPresentation` (`include/vsg/app/Viewer.h:124`).
- There is no `setCamera()` on `Viewer` — the camera lives in the `CommandGraph`/`View`, not the viewer.
- `updateViewer` is a free function `vsg::updateViewer(Viewer&, const CompileResult&)`, not a member `viewer->updateViewer(...)` (`include/vsg/app/Viewer.h:172`).
- Do not assume `compile()` returns `void` — it returns `CompileResult` (`include/vsg/app/Viewer.h:107`).
