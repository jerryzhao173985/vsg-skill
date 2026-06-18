# Patterns — vsg

Cross-cutting, screen-/app-level recipes that compose multiple VSG classes. Each is distilled from a real `vsgExamples` program (cited). For single-class API detail, load the per-component file instead.

## Recipe 1 — View a loaded 3D model

The canonical interactive VSG app: load a model, frame it with a camera, add mouse + close handlers, render. Distilled faithfully from `examples/app/vsghelloworld/vsghelloworld.cpp:6-91`.

```cpp
#include <vsg/all.h>
#include <vsgXchange/all.h>   // when vsgXchange_FOUND — for glTF/OBJ/etc.

int main(int argc, char** argv)
{
    vsg::CommandLine arguments(&argc, argv);                       // parse CLI overrides
    auto windowTraits = vsg::WindowTraits::create(arguments);

    auto options = vsg::Options::create(vsgXchange::all::create()); // register loaders
    options->paths = vsg::getEnvPaths("VSG_FILE_PATH");

    vsg::Path filename = (argc > 1) ? arguments[1] : "models/teapot.vsgt";
    auto vsg_scene = vsg::read_cast<vsg::Node>(filename, options);
    if (!vsg_scene) return 1;                                       // null on missing file / no loader

    auto viewer = vsg::Viewer::create();
    auto window = vsg::Window::create(windowTraits);
    if (!window) return 1;
    viewer->addWindow(window);

    // frame the camera from the scene bounds
    vsg::ComputeBounds computeBounds;
    vsg_scene->accept(computeBounds);
    vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5;
    double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;

    auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0), centre, vsg::dvec3(0.0, 0.0, 1.0));
    auto perspective = vsg::Perspective::create(30.0,
        static_cast<double>(window->extent2D().width) / static_cast<double>(window->extent2D().height),
        radius * 0.001, radius * 4.5);
    auto camera = vsg::Camera::create(perspective, lookAt, vsg::ViewportState::create(window->extent2D()));

    viewer->addEventHandler(vsg::CloseHandler::create(viewer));
    viewer->addEventHandler(vsg::Trackball::create(camera));

    auto commandGraph = vsg::createCommandGraphForView(window, camera, vsg_scene);
    viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});
    viewer->compile();

    while (viewer->advanceToNextFrame())
    {
        viewer->handleEvents();
        viewer->update();
        viewer->recordAndSubmit();
        viewer->present();
    }
    return 0;
}
```

Key components: `references/components/io.md` (read/Options), `window.md`, `camera.md`, `trackball.md`, `commandgraph.md`, `viewer.md`.

## Recipe 2 — Build a procedural-geometry scene (no model files)

Generate primitives at runtime with `Builder` instead of loading assets. Distilled from `examples/utils/vsgbuilder/vsgbuilder.cpp:20-211,308`.

```cpp
#include <vsg/all.h>

auto options = vsg::Options::create();
options->sharedObjects = vsg::SharedObjects::create();   // share pipelines/state across shapes

auto builder = vsg::Builder::create();
builder->options = options;

auto scene = vsg::Group::create();

vsg::GeometryInfo geomInfo;                  // float position/extents/color
vsg::StateInfo stateInfo;                    // lighting/wireframe/two_sided/texture flags
stateInfo.lighting = true;

scene->addChild(builder->createBox(geomInfo, stateInfo));
geomInfo.position += geomInfo.dx * 1.5f;     // shift the next shape along +x
scene->addChild(builder->createSphere(geomInfo, stateInfo));
geomInfo.position += geomInfo.dx * 1.5f;
scene->addChild(builder->createCylinder(geomInfo, stateInfo));

// then wire `scene` into a Camera + CommandGraph and viewer->compile() exactly as Recipe 1.
```

The available shapes are exactly `createBox`, `createSphere`, `createQuad`, `createCylinder`, `createCone`, `createCapsule`, `createDisk`, `createHeightField` (`include/vsg/utils/Builder.h:123-130`) — there is no `createTorus`/`createPlane`. For shapes created *after* the viewer starts, call `builder->assignCompileTraversal(vsg::CompileTraversal::create(*viewer))` first. See `references/components/builder.md`.

**Lighting (do not skip):** `Builder` geometry is lit by default, so it renders **black** with no light. The `createCommandGraphForView` wiring (Recipe 1) adds a headlight automatically; if you hand-wire a `View` for explicit control, add `view->addChild(vsg::createHeadlight())`. See `references/components/lighting.md`.

## Recipe 3 — Offscreen / headless rendering

Render with no window/surface: build a `vsg::Device` straight from a `vsg::Instance`, render into your own `vsg::Framebuffer`, run the loop with **no `present()`**, then copy the color image into a host-visible buffer and `vsg::write` it. The full step-by-step path — grounded line-by-line in `examples/app/vsgheadless/vsgheadless.cpp` — is in **`references/examples/headless-rendering.md`**.

⚠ **macOS/MoltenVK:** surface-less offscreen rendering crashes at record time on macOS (the official `vsgheadless` reproduces it identically); render to a window-backed swapchain and capture from it instead (`examples/app/vsgscreenshot`). See the Platform note in `references/examples/headless-rendering.md`.
