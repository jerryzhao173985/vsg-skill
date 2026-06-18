---
name: vsg
description: Build native C++ VulkanSceneGraph (VSG) applications — the Viewer/Window/Camera render loop, scene-graph nodes (Group, MatrixTransform, StateGroup), procedural geometry (Builder), model IO (vsg::read/Options), and the ref_ptr/Inherit object model. Use when the user asks to build/scaffold/extend a VSG app, set up a Vulkan scene graph, write a vsg::Viewer, load and render a 3D model with VSG, or wire a CommandGraph. Triggers: 'vsg', 'VulkanSceneGraph', 'vsg::Viewer', 'vsgExamples', 'vulkan scene graph', 'scene graph c++'. Scope: component APIs, class descriptions, idioms, and build wiring. Out of scope: GLSL/SPIR-V shader authoring, Vulkan-level synchronisation internals, and tone/marketing copy. IMPORTANT: this file is an orchestrator. Load the references/ files named in the routing table; SKILL.md alone is insufficient.
---

# VulkanSceneGraph (VSG) skill

## Mission

A `vsg` skill is an adapter that teaches an agent how to build high-fidelity native **C++ VulkanSceneGraph** applications. It is not a copy of the VSG documentation. It tells the agent which headers are authoritative, what the public API actually is, which idioms are load-bearing, and how to verify generated code against the real library before claiming it works.

VulkanSceneGraph (VSG, v1.1.14) is a modern C++17 Vulkan scene-graph library. The **authoritative source is the header tree** at `include/vsg/` in the [VulkanSceneGraph repo](https://github.com/vsg-dev/VulkanSceneGraph); the **idiomatic usage** is the [vsgExamples repo](https://github.com/vsg-dev/vsgExamples) under `examples/`. Every rule in this skill is cited to a `path:line` in one of those two repos. Code wins over prose on every conflict.

## Scope

This skill follows the design-system extraction charter — In scope: tokens, assets, component descriptions, component APIs. Out of scope: tone of voice, marketing copy, product copywriting. For VulkanSceneGraph specifically, the applicable pillars are **component descriptions and component APIs** (the VSG classes and their public surface); VSG ships no design tokens and no visual assets, so those two pillars are empty here. GLSL/SPIR-V shader authoring and Vulkan-level synchronisation internals are also out of scope — route those to a dedicated shader/Vulkan skill.

## Setup

VSG is consumed via CMake `find_package`. A standalone consumer project links `vsg::vsg` and, optionally, `vsgXchange::vsgXchange` for reading third-party model/image formats.

**`CMakeLists.txt`** (grounded in `vsgExamples/CMakeLists.txt:24` + `examples/app/vsgviewer/CMakeLists.txt:7,9-12`):

```cmake
cmake_minimum_required(VERSION 3.14)
project(myvsgapp LANGUAGES CXX)

find_package(vsg REQUIRED)                 # VulkanSceneGraph core
find_package(vsgXchange QUIET)             # optional: 3D model + image loaders

add_executable(myvsgapp main.cpp)
target_link_libraries(myvsgapp vsg::vsg)

if (vsgXchange_FOUND)                       # only when vsgXchange was found
    target_compile_definitions(myvsgapp PRIVATE vsgXchange_FOUND)
    target_link_libraries(myvsgapp vsgXchange::vsgXchange)
endif()
```

**Build** (grounded in `vsgExamples/INSTALL.md:11-25`):

```sh
cmake -B build -S .
cmake --build build -j 8
```

**`main.cpp`** — load-bearing order: scene + viewer + camera → wire a `CommandGraph` → **`compile()` once** → frame loop. Minimal skeleton:

```cpp
#include <vsg/all.h>                 // + <vsgXchange/all.h> when vsgXchange_FOUND (glTF/OBJ/images)
int main(int argc, char** argv)
{
    auto options = vsg::Options::create();   // options->add(vsgXchange::all::create()) for non-native formats
    auto scene = vsg::read_cast<vsg::Node>("model.vsgt", options);
    if (!scene) return 1;                                           // read_cast is nullable

    auto viewer = vsg::Viewer::create();                           // never `new`
    auto window = vsg::Window::create(vsg::WindowTraits::create());
    if (!window) return 1;                                         // Window::create is nullable
    viewer->addWindow(window);

    auto lookAt = vsg::LookAt::create(vsg::dvec3(0.0, -10.0, 0.0), vsg::dvec3(0.0, 0.0, 0.0), vsg::dvec3(0.0, 0.0, 1.0));
    auto persp  = vsg::Perspective::create(30.0, static_cast<double>(window->extent2D().width) / window->extent2D().height, 0.1, 1000.0);
    auto camera = vsg::Camera::create(persp, lookAt, vsg::ViewportState::create(window->extent2D()));

    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));
    auto cg = vsg::createCommandGraphForView(window, camera, scene);  // adds a headlight by default
    viewer->assignRecordAndSubmitTaskAndPresentation({cg});
    viewer->compile();                                             // ONCE, after wiring, before the loop
    while (viewer->advanceToNextFrame()) {                         // handleEvents → update → recordAndSubmit → present
        viewer->handleEvents(); viewer->update(); viewer->recordAndSubmit(); viewer->present();
    }
    return 0;                                                      // ref_ptr cleans up
}
```

Complete, copy-paste programs live in `references/examples/`: the bounds-framed viewer (`model-viewer.md`, from `examples/app/vsghelloworld/vsghelloworld.cpp:6-91`) and the offscreen renderer (`headless-main.cpp`).

## Include rules

- **Barrel include is canonical:** `#include <vsg/all.h>` (`vsg/all.h`) pulls in the whole public surface; this is what every example uses (`examples/app/vsghelloworld/vsghelloworld.cpp:1`). Per-header includes (`#include <vsg/app/Viewer.h>`) are public and fine for narrow translation units, but the barrel is the default.
- **Namespace is `vsg::`.** Maths types, nodes, app classes, and free functions all live in `namespace vsg`.
- **vsgXchange is a separate, optional package** (`#include <vsgXchange/all.h>`, namespace `vsgXchange::`). Guard its use behind `vsgXchange_FOUND` (`examples/app/vsgviewer/CMakeLists.txt:9-12`). Native `.vsgt`/`.vsgb` IO needs no vsgXchange.
- Do **not** include from build/internal paths or copy `src/` implementation files; consume only the public `include/vsg/` surface.

## Source-of-truth rules

| Source | Role | Authority |
|---|---|---|
| `include/vsg/**.h` (VulkanSceneGraph @ v1.1.14) | the public API | **highest** — class/method/member names, signatures, defaults |
| `examples/**/*.cpp` (vsgExamples @ master) | idiomatic usage / wiring | canonical for composition and call order |
| `src/vsg/**.cpp` (VulkanSceneGraph) | implementation / behaviour | **authoritative for runtime behaviour** — the `references/foundations/` files cite it for *how* compile/record/traversal actually work; never copy `src/` as the consumer API |

When a header and an example disagree on a signature, the **header wins** — the example may lag. Every extracted rule in `references/` cites a `path:line`; an uncited claim is a `[VERIFY]` or a bug.

## When to Load References

| Trigger | Files to load | Notes |
|---|---|---|
| user needs the architecture / mental model (how VSG fits together) | references/foundations/index.md | start here for any non-trivial app; routes to the 4 foundation files |
| user reasons about object lifetime, memory, or custom node types | references/foundations/object-model.md | ref_ptr/Object/Inherit/Allocator/Visitor model, grounded in `src/vsg/core/` |
| user reasons about the scene graph, traversal, or state binding | references/foundations/scene-graph.md | Node DAG, RecordTraversal, StateGroup, grounded in `src/vsg/` |
| user reasons about the frame lifecycle, compile/record/present, or threading | references/foundations/application-and-rendering.md | the rendering execution model, grounded in `src/vsg/app/Viewer.cpp` |
| user works with data, IO, serialization, or maths precision | references/foundations/io-and-data.md | Data/Array/Value, read/write/Options, `.vsgt`/`.vsgb`, double precision |
| user works with `ref_ptr`/`create()`/`Inherit`, object lifetime, or defines a new node type | references/components/core-objects.md | the foundational idiom every other file assumes |
| user sets up the viewer or the render loop | references/components/viewer.md | wire → compile → frame loop ordering |
| user creates a window or sets window options | references/components/window.md | `WindowTraits`, nullable `Window::create` |
| user sets up a camera, projection, or view matrix | references/components/camera.md | `LookAt` + `Perspective`/`Orthographic` + `ViewportState` |
| user builds a scene-graph hierarchy / container node | references/components/group.md | `Group`/`Node`, child ownership, traversal |
| user positions, rotates, or scales a subgraph | references/components/matrixtransform.md | `dmat4` matrix, `Transform`/`AbsoluteTransform` |
| user binds pipeline/descriptor state to a subgraph | references/components/stategroup.md | `StateGroup` + state commands |
| user lights a scene, or geometry renders black | references/components/lighting.md | `createHeadlight` + Ambient/Directional/Point/Spot; the lit-but-no-light trap |
| user casts real-time shadows from a light | references/components/shadows.md | `ShadowSettings`/`HardShadows`/`SoftShadows`/`PCSS` on `light->shadowSettings`; tune `view->viewDependentState` |
| user shows multiple views or multiple windows | references/components/multiview.md | several `View`s in one `RenderGraph` (side-by-side / inset via `ClearAttachments`) or one `CommandGraph` per `Window` |
| user implements mouse picking / ray or box selection | references/components/intersection.md | `LineSegmentIntersector`/`PolytopeIntersector`; shader-free highlight via `vsg::Switch` |
| user adds camera manipulation or event handling | references/components/trackball.md | `Trackball`, `CloseHandler`, the `Visitor` handler model |
| user wires rendering (command graph / render graph / view) | references/components/commandgraph.md | `createCommandGraphForView`, manual composition |
| user generates procedural geometry (box/sphere/…) | references/components/builder.md | `Builder` + `GeometryInfo` + `StateInfo` |
| user loads or saves models, or sets IO options | references/components/io.md | `vsg::read`/`read_cast`/`write`, `Options`, vsgXchange |
| user wants a worked example program to copy | references/examples/index.md | annotated real vsgExamples programs with exact cites |
| user builds a model viewer | references/examples/model-viewer.md | the canonical load→frame→orbit→render skeleton (`vsghelloworld`) |
| user wants a 2D / orthographic view | references/examples/orthographic-view.md | swap Perspective→Orthographic (`vsgortho`) |
| user wants procedurally-generated geometry | references/examples/procedural-geometry.md | `Builder` shapes, no asset files (`vsgbuilder`) |
| user renders offscreen / headless (no window) | references/examples/headless-rendering.md | Device-from-Instance, capture-to-file; ⚠ crashes on macOS/MoltenVK |
| user composes a full app or asks for a common recipe | references/patterns.md | end-to-end recipes (view a model, procedural scene, headless) |
| (maintenance) common pitfalls / Bad-Good-Why registry | references/anti-patterns.md | mirror of the Hard rules; not required during generation |

## Component slate

Each bullet's identifier is the contract-section file basename under `references/components/`; the classes it covers follow. (Several files group sibling classes — e.g. `window` covers `Window` + `WindowTraits`.)

- `core-objects` — the intrusive object model: `T::create()` factory, strong `ref_ptr<T>`, weak `observer_ptr<T>`, `Inherit<Base,Derived>` for new types.
- `viewer` — `vsg::Viewer`, the render-loop driver: owns windows/handlers/tasks; `compile()` + advance/handleEvents/update/recordAndSubmit/present.
- `window` — `vsg::Window` + `vsg::WindowTraits`: Vulkan surface + window configuration (size, fullscreen, samples, debug layers).
- `camera` — `vsg::Camera` + `vsg::LookAt` + `vsg::Perspective`: view + projection + viewport for a `View`.
- `group` — `vsg::Group`: the basic scene-graph container of child `Node`s.
- `matrixtransform` — `vsg::MatrixTransform`: positions a subgraph via a 4×4 `dmat4` multiplied into the modelview.
- `stategroup` — `vsg::StateGroup`: binds Vulkan state (pipeline, descriptors) to its subgraph.
- `lighting` — `createHeadlight` + `AmbientLight`/`DirectionalLight`/`PointLight`/`SpotLight`: lights for lit pipelines (lit geometry renders black without one).
- `shadows` — `ShadowSettings`/`HardShadows`/`SoftShadows`/`PercentageCloserSoftShadows` on `light->shadowSettings`: real-time shadow maps from a directional/spot light, tuned on `view->viewDependentState`.
- `multiview` — several `vsg::View`s in one window (side-by-side, or inset via a scissored `ClearAttachments`) or several `vsg::Window`s (one `CommandGraph` each).
- `intersection` — `LineSegmentIntersector`/`PolytopeIntersector` for mouse picking / box-select; shader-free highlight via `vsg::Switch`.
- `trackball` — `vsg::Trackball` + event handlers (`CloseHandler`), attached to the `Viewer`.
- `commandgraph` — `vsg::CommandGraph` + `RenderGraph` + `View`: the recording wiring; `createCommandGraphForView` builds it.
- `builder` — `vsg::Builder`: procedural geometry (box/sphere/quad/cylinder/cone/capsule/disk/height-field) from `GeometryInfo` + `StateInfo`.
- `io` — `vsg::read` / `write` / `Options`: load/save scene graphs (native `.vsgt`/`.vsgb` + vsgXchange formats).

## Hard rules

- **Allocate with `T::create(...)`, never `new`/`delete`.** Every `vsg::Object`-derived type is heap-allocated through its static factory and intrusively reference-counted; the destructor is `protected`, so stack allocation and manual `delete` will not compile. Cleanup is automatic when the last `ref_ptr` drops. See `references/components/core-objects.md`.
- **Derive new types via `vsg::Inherit<ParentClass, NewClass>`, never directly from the base.** `Inherit` wires `create()`, ref-counting, RTTI (`type_info`/`is_compatible`), and `accept()` visitor dispatch. See `references/components/core-objects.md`.
- **Wire then compile then loop.** Build `Viewer → Window → Camera → CommandGraph` (via `vsg::createCommandGraphForView`), call `viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph})`, then **`viewer->compile()` exactly once before the frame loop**. The loop body is `handleEvents → update → recordAndSubmit → present`, with `while (viewer->advanceToNextFrame())` as the condition. See `references/components/viewer.md` and `references/components/commandgraph.md`.
- **Check nullable returns.** `vsg::Window::create(...)` and `vsg::read_cast<vsg::Node>(...)` can return an empty `ref_ptr`; test before use. See `references/components/window.md` and `references/components/io.md`.
- **Mind precision.** Scene-graph transforms are **double precision** (`vsg::dmat4`/`dvec3`); `Builder` `GeometryInfo` is **float** (`vsg::vec3`/`mat4`). Do not pass `dvec3` into `GeometryInfo`; offset world-scale placement with a `MatrixTransform`. See `references/components/matrixtransform.md` and `references/components/builder.md`.
- **Do not mutate the scene graph on the record thread.** `RecordTraversal` reads the graph during `recordAndSubmit()`; structural edits must happen outside recording (or via the viewer's update operations). See `references/components/group.md` and `references/components/commandgraph.md`.
- **Lit geometry needs a light.** `vsg::Builder` sets `StateInfo.lighting=true` by default (and any lit `ShaderSet` is lit), so surfaces render **black** unless a `vsg::Light` is in the View/scene. `vsg::createCommandGraphForView(...)` adds a headlight automatically (its `assignHeadlight` defaults true); a hand-wired `View` must `addChild(vsg::createHeadlight())` or add explicit lights. See `references/components/lighting.md`.
- **Grounding discipline.** Any class, method, member, or enum the agent cannot ground in an `include/vsg/**.h` line gets a literal `[VERIFY]` marker inline. Report blockers instead of guessing — if the header does not declare it, it does not exist.

## Final checks

After generating VSG code, the agent confirms, in one closing summary:

1. **Compiles and links.** The code builds against the public API — `target_link_libraries(<target> vsg::vsg)` is present and every type used is reachable from `#include <vsg/all.h>`. When a local VSG install exists, the strongest proof is a `find_package(vsg)` CMake target that compiles **and links** the snippet (catches signature/ABI mismatches a `-fsyntax-only` parse misses); syntax-only against the headers is the cheaper fallback.
2. **Idioms hold.** Every object is created via `T::create(...)` (no `new`), new types use `Inherit<>`, `viewer->compile()` runs once before the loop, and nullable `create`/`read_cast` results are checked.
3. **Cited.** Each VSG class used is traceable to its header (`include/vsg/...`); any rule that could not be grounded is flagged `[VERIFY]`.
4. **Names the artifact.** State the app/screen built (e.g. "a model viewer", "a procedural-geometry scene") and list any remaining `[VERIFY]` markers.
