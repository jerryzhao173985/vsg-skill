---
title: The application & rendering execution model
description: How a VSG frame happens — Viewer owns RecordAndSubmitTasks + Presentations, drives compile() once then loops advanceToNextFrame/handleEvents/update/recordAndSubmit/present over a CommandGraph→RenderGraph→View→scene record hierarchy.
---

## What this covers
- The render-loop lifecycle: one-time `Viewer::compile()` then a repeated frame loop of `advanceToNextFrame` → `handleEvents` → `update` → `recordAndSubmit` → `present`.
- The recording hierarchy: `CommandGraph` → `RenderGraph` → `View` → scene subgraph, and how each maps to Vulkan command-buffer recording.
- How `Viewer` owns `RecordAndSubmitTask`s (record + `vkQueueSubmit`) and `Presentation`s (`vkQueuePresentKHR`), and how `assignRecordAndSubmitTaskAndPresentation` wires them from `CommandGraph`s.
- What `compile()` does (create Vulkan objects, upload data) vs `recordAndSubmit()` (record command buffers, submit) vs `present()`.
- Optional multi-threading via `setupThreading()` and the per-task/per-CommandGraph thread model.

## Mental model
A VSG application is a `vsg::Viewer` that owns two parallel lists: `recordAndSubmitTasks` (work that records command buffers and submits them to a Vulkan queue) and `presentations` (work that presents finished swapchain images) (`include/vsg/app/Viewer.h:116`, `include/vsg/app/Viewer.h:119-120`). You do not build these by hand; you build a scene-graph recording hierarchy of `CommandGraph`s and hand them to the Viewer, which derives the tasks and presentations from them.

The recording hierarchy is itself a scene graph. A `CommandGraph` is a `Group` that "sits at the top of the scene graph and manages the recording of its subgraph to Vulkan command buffers" (`include/vsg/app/CommandGraph.h:26-27`). Beneath it sits a `RenderGraph`, which "encapsulates the vkCmdBeginRenderPass/vkCmdEndRenderPass functionality" (`include/vsg/app/RenderGraph.h:25`). Beneath that sits a `View`, which "pairs a Camera that defines the view with a subgraph that defines the scene" (`include/vsg/app/View.h:34`). So the canonical chain is CommandGraph → RenderGraph → View → your scene. The convenience helpers `createRenderGraphForView` (`src/vsg/app/RenderGraph.cpp:218-230`) and `createCommandGraphForView` (`src/vsg/app/CommandGraph.cpp:141-148`) assemble exactly this chain for you.

The lifecycle has two phases with a hard ordering between them. First, exactly once after wiring, `Viewer::compile()` walks every task's CommandGraphs collecting resource requirements, allocates descriptor pools, and compiles each task — creating the Vulkan objects (pipelines, descriptor sets, buffers, images) and arranging for vertex/image/uniform data to be uploaded to the GPU (`src/vsg/app/Viewer.cpp:278-418`). Then the frame loop runs repeatedly: `advanceToNextFrame()` (poll events, acquire the next swapchain image, build a new `FrameStamp`, advance per-task fence indices), `handleEvents()` (dispatch events to handlers), `update()` (run update operations and animations), `recordAndSubmit()` (record command buffers and `vkQueueSubmit`), and `present()` (`vkQueuePresentKHR`) (`examples/app/vsgviewer/vsgviewer.cpp:395-404`, `examples/app/vsghelloworld/vsghelloworld.cpp:73-86`).

The key invariant: `compile()` creates GPU state; `recordAndSubmit()` only *records and submits* command buffers referencing that state; `present()` only displays already-rendered images. Recording assumes the Vulkan objects already exist, so `compile()` must run after the scene+CommandGraphs are wired and before the first `recordAndSubmit()`. Adding renderable content after compile requires re-compiling that subgraph (via `Viewer::compileManager` / `CompileManager`) and merging the result back with `updateViewer` — you cannot simply add nodes and record them.

## Key types
- `Viewer` — owns the windows, the `recordAndSubmitTasks` and `presentations` lists, and drives the whole lifecycle (`include/vsg/app/Viewer.h:32`).
- `CommandGraph` — top-of-scene `Group` that records its subgraph into one Vulkan command buffer per frame (`include/vsg/app/CommandGraph.h:27`).
- `RenderGraph` — `Group` that wraps its children in `vkCmdBeginRenderPass`/`vkCmdEndRenderPass` (`include/vsg/app/RenderGraph.h:28`).
- `View` — `Group` pairing a `Camera` with a scene subgraph; pushes view/projection and view-dependent state (lights) (`include/vsg/app/View.h:35`).
- `RecordAndSubmitTask` — records its `CommandGraphs` into command buffers and submits them to a `Queue`, guarded by a per-frame `Fence` (`include/vsg/app/RecordAndSubmitTask.h:27`).
- `Presentation` — presents the swapchains of its windows on a queue, waiting on the task's signal semaphores (`include/vsg/app/Presentation.h:21`).
- `CompileManager` / `CompileResult` — thread-safe compilation of subgraphs and the struct describing what was compiled (`include/vsg/app/CompileManager.h:55`, `include/vsg/app/CompileManager.h:26`).

## How it works (implementation-grounded)

**Wiring — `assignRecordAndSubmitTaskAndPresentation(CommandGraphs)`.** This takes your CommandGraphs, finds the windows inside them with a `FindWindows` visitor, and groups the CommandGraphs by (device, queueFamily, presentFamily) (`src/vsg/app/Viewer.cpp:459-477`). For each group it creates one `RecordAndSubmitTask` assigned the group's CommandGraphs and the main graphics queue; and, when the group has a present family, also a `Presentation` assigned the same windows and the present queue (`src/vsg/app/Viewer.cpp:540-555`). Groups without a present family get a task but no presentation — that is how compute-only / off-screen work is expressed (`src/vsg/app/Viewer.cpp:558-571`). It also assigns the discovered windows back onto the Viewer (`src/vsg/app/Viewer.cpp:480`). Secondary-level CommandGraphs are ordered before primary ones so they are recorded first (`src/vsg/app/Viewer.cpp:485-499`).

**compile() — create Vulkan objects and arrange uploads.** `Viewer::compile()` returns early if there are no tasks (`src/vsg/app/Viewer.cpp:282-285`). It builds a per-device `CollectResourceRequirements` by accepting it over each CommandGraph (`src/vsg/app/Viewer.cpp:299-313`), allocates per-device resources and assigns view IDs/bins (`src/vsg/app/Viewer.cpp:317-347`), creates a `CompileManager` if absent (`src/vsg/app/Viewer.cpp:376-380`), then for each task calls `compileManager->compileTask(task, resourceRequirements)` and hands the dynamic data to the task's `transferTask` for GPU upload (`src/vsg/app/Viewer.cpp:401-402`). It also starts any `DatabasePager`s (`src/vsg/app/Viewer.cpp:406-415`). This is where pipelines, descriptor sets and buffers come into existence — not during recording.

**advanceToNextFrame() — frame setup.** Returns false if the viewer is no longer `active()` (`src/vsg/app/Viewer.cpp:162-165`), polls window events (`src/vsg/app/Viewer.cpp:168`), acquires the next swapchain image via `acquireNextFrame()` (rebuilding the swapchain on out-of-date/suboptimal results) (`src/vsg/app/Viewer.cpp:170`, `src/vsg/app/Viewer.cpp:208-246`), creates a fresh `FrameStamp` with an incremented frame count (`src/vsg/app/Viewer.cpp:172-192`), advances each task's frame/fence index (`src/vsg/app/Viewer.cpp:197-200`), and pushes a `FrameEvent` (`src/vsg/app/Viewer.cpp:203`).

**update() — apply changes before recording.** Merges any `DatabasePager` scene-graph updates (calling `updateViewer` when a recompile changed Viewer-level structures), runs queued update operations, then runs animations (`src/vsg/app/Viewer.cpp:798-812`).

**recordAndSubmit() — record then submit.** First resets every CommandGraph (`src/vsg/app/Viewer.cpp:820-826`). In the single-threaded path it calls `task->submit(_frameStamp)` for each task (`src/vsg/app/Viewer.cpp:842-845`). `RecordAndSubmitTask::submit` is `start()` → early transfer → `record()` → `finish()` (`src/vsg/app/RecordAndSubmitTask.cpp:85-114`). `start()` waits on the current frame's `Fence` if it still has dependencies, giving CPU/GPU frame throttling (`src/vsg/app/RecordAndSubmitTask.cpp:116-141`). `record()` calls `commandGraph->record(...)` per CommandGraph (`src/vsg/app/RecordAndSubmitTask.cpp:143-153`). `finish()` collects the recorded command buffers, assembles wait/signal semaphores (including the windows' `imageAvailableSemaphore`/`renderFinishedSemaphore`), and calls `queue->submit(submitInfo, current_fence)` — the actual `vkQueueSubmit`, guarded by the frame fence (`src/vsg/app/RecordAndSubmitTask.cpp:155-273`).

**CommandGraph::record() — actually fill the command buffer.** Skips invisible windows (`src/vsg/app/CommandGraph.cpp:80-84`), gets/creates a `RecordTraversal`, reuses or allocates a command buffer (`src/vsg/app/CommandGraph.cpp:87-115`), `vkBeginCommandBuffer`, traverses the subgraph (`traverse(*recordTraversal)`), `vkEndCommandBuffer`, and adds the buffer to the recorded set ordered by `submitOrder` (`src/vsg/app/CommandGraph.cpp:124-138`). During that traversal `RenderGraph::accept` issues `vkCmdBeginRenderPass`, pushes the viewport state, traverses children (the `View` and scene), then `vkCmdEndRenderPass` (`src/vsg/app/RenderGraph.cpp:150-170`). It also auto-detects window resizes and re-runs the `WindowResizeHandler` (`src/vsg/app/RenderGraph.cpp:109-118`).

**present() — display.** `Viewer::present()` calls `presentation->present()` for each Presentation (`src/vsg/app/Viewer.cpp:849-857`). `Presentation::present` builds the wait semaphores from the windows' `renderFinishedSemaphore`s and calls `queue->present(presentInfo)` — `vkQueuePresentKHR` (`src/vsg/app/Presentation.cpp:18-73`).

**Threading (optional).** `setupThreading()` spawns one worker thread per single-CommandGraph task (each calls `task->submit`), or, for multi-CommandGraph tasks, a primary/secondary/transfer thread set coordinated by barriers (`src/vsg/app/Viewer.cpp:598-770`). When threading is on, `recordAndSubmit()` releases the `FrameBlock` and waits on a completion `Barrier` instead of submitting inline (`src/vsg/app/Viewer.cpp:828-846`). The public loop is unchanged; only `recordAndSubmit()` internals differ.

## Rules that follow
- Build the recording hierarchy as `CommandGraph` → `RenderGraph` → `View` → scene, then call `assignRecordAndSubmitTaskAndPresentation({commandGraph})` before compiling; tasks/presentations are derived, never hand-built (`src/vsg/app/Viewer.cpp:420-575`, `examples/app/vsgviewer/vsgviewer.cpp:328-330`).
- Call `viewer->compile()` exactly once, after wiring and before the first `recordAndSubmit()`; it creates the Vulkan objects and uploads data, which recording assumes already exist (`src/vsg/app/Viewer.cpp:278-418`).
- Drive the frame loop in this order: `advanceToNextFrame()` (as the `while` condition) → `handleEvents()` → `update()` → `recordAndSubmit()` → `present()` (`examples/app/vsgviewer/vsgviewer.cpp:395-404`).
- Add event handlers (e.g. `CloseHandler`, `Trackball`) before the loop; `handleEvents()` only dispatches to registered handlers (`src/vsg/app/Viewer.cpp:265-276`, `examples/app/vsgviewer/vsgviewer.cpp:294-308`).
- To add renderable content after `compile()`, compile that subgraph through `Viewer::compileManager` and merge with `updateViewer`; do not append nodes and expect recording to work (`include/vsg/app/Viewer.h:94`, `src/vsg/app/Viewer.cpp:882-887`).
- If multi-threading, call `setupThreading()` after `assignRecordAndSubmitTaskAndPresentation` (it is also auto-restarted by re-wiring); the loop body stays identical (`src/vsg/app/Viewer.cpp:598-619`, `examples/app/vsgviewer/vsgviewer.cpp:334-336`).
- Read `DatabasePager`/PagedLOD-derived state (e.g. `task->databasePager`) only after `compile()`, since compile() is what creates and assigns the pager (`src/vsg/app/Viewer.cpp:349-353`, `examples/app/vsgviewer/vsgviewer.cpp:373-379`).

## Common mistakes
- Recording before compiling → call `compile()` once after wiring; recording references Vulkan objects that only `compile()` creates (`src/vsg/app/Viewer.cpp:278-418`).
- Manually constructing `RecordAndSubmitTask`/`Presentation` → hand `CommandGraph`s to `assignRecordAndSubmitTaskAndPresentation`, which derives and wires both with correct queues and semaphores (`src/vsg/app/Viewer.cpp:483-571`).
- Putting scene nodes directly under a `CommandGraph` → a `RenderGraph` (begin/end render pass) and a `View` (camera + lights) must sit between the CommandGraph and the scene (`src/vsg/app/RenderGraph.cpp:105-171`, `include/vsg/app/View.h:34`).
- Expecting `present()` to render → `present()` only calls `vkQueuePresentKHR`; rendering work is recorded and submitted in `recordAndSubmit()` (`src/vsg/app/Presentation.cpp:73`, `src/vsg/app/RecordAndSubmitTask.cpp:273`).
- Adding nodes after compile and expecting them to draw → recompile the subgraph via `CompileManager` and apply `updateViewer` (`src/vsg/app/Viewer.cpp:882-887`).
- Forgetting to register a `CloseHandler` → the loop may never exit because `advanceToNextFrame()` stays active (`src/vsg/app/Viewer.cpp:155-206`, `examples/app/vsgviewer/vsgviewer.cpp:294`).

## Source references
- `include/vsg/app/Viewer.h` — Viewer lifecycle API, task/presentation lists, threading hooks
- `include/vsg/app/CommandGraph.h` — CommandGraph role, `createCommandGraphForView`
- `include/vsg/app/RenderGraph.h` — RenderGraph render-pass wrapping, `createRenderGraphForView`
- `include/vsg/app/View.h` — View = camera + scene subgraph
- `include/vsg/app/RecordAndSubmitTask.h` — record/submit/fence API
- `include/vsg/app/Presentation.h` — presentation/queue API
- `include/vsg/app/CompileManager.h` — CompileManager, CompileResult
- `src/vsg/app/Viewer.cpp` — compile(), advanceToNextFrame(), update(), recordAndSubmit(), present(), assignRecordAndSubmitTaskAndPresentation(), setupThreading()
- `src/vsg/app/CommandGraph.cpp` — record() command-buffer begin/traverse/end
- `src/vsg/app/RenderGraph.cpp` — accept() begin/end render pass, resize handling
- `src/vsg/app/RecordAndSubmitTask.cpp` — submit()/start()/record()/finish(), vkQueueSubmit
- `src/vsg/app/Presentation.cpp` — present(), vkQueuePresentKHR
- `examples/app/vsgviewer/vsgviewer.cpp` — full wiring + mainloop + threading
- `examples/app/vsghelloworld/vsghelloworld.cpp` — minimal compile + mainloop
