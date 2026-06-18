---
title: Lighting (createHeadlight / AmbientLight / DirectionalLight / PointLight / SpotLight)
description: VSG lighting — why lit surfaces render black without a Light node, the createHeadlight() one-liner fix, and explicit Ambient/Directional/Point/Spot lights.
---

## Public include

```cpp
#include <vsg/lighting/Light.h>            // base Light + createHeadlight()
#include <vsg/lighting/AmbientLight.h>
#include <vsg/lighting/DirectionalLight.h>
#include <vsg/lighting/PointLight.h>
#include <vsg/lighting/SpotLight.h>
```

`vsg::createHeadlight()` is declared in `Light.h:49`.

## When to use

Use a `vsg::Light` whenever your scene is rendered with a **lit** pipeline. This is the default for geometry built by `vsg::Builder` (its `StateInfo.lighting` defaults to `true`) and for any lit `ShaderSet`. A lit shader samples a light source from the `LightData` uniform that the `RecordTraversal` fills from `Light` nodes in the view (`Light.h:23`). If no `Light` node is present, that uniform contributes no illumination and lit surfaces render **unlit / black**.

- Quick app via `vsg::createCommandGraphForView(...)` or `vsg::createRenderGraphForView(...)`: a headlight is added **for you** (their `assignHeadlight` parameter defaults to `true`, `CommandGraph.h:66`, `RenderGraph.cpp:222`). No extra code needed.
- Hand-wired `View`/`RenderGraph`: **you** must add a light, or the scene is black.
- Want artistic control (sun direction, colored point/spot lights, shadows): add explicit `Light` subclass nodes.

## Key API

Base class `vsg::Light : public Inherit<Node, Light>` (`Light.h:25`). Shared **public data members** (no setters — assign the members directly):

- `std::string name;` (`Light.h:31`)
- `vec3 color = vec3(1.0f, 1.0f, 1.0f);` (`Light.h:32`)
- `float intensity = 1.0f;` (`Light.h:33`)
- `ref_ptr<ShadowSettings> shadowSettings;` (`Light.h:34`)

Subclasses (each `Inherit<Light, X>`) add their own public members:

- `vsg::AmbientLight` — no extra members; uniform fill light (`AmbientLight.h:23`).
- `vsg::DirectionalLight` — `dvec3 direction = dvec3(0.0, 0.0, -1.0);` (`DirectionalLight.h:29`); also `float angleSubtended` (`DirectionalLight.h:30`). Treated as infinitely distant (sun/moon).
- `vsg::PointLight` — `dvec3 position = dvec3(0.0, 0.0, 0.0);` (`PointLight.h:29`); `double radius` (`PointLight.h:30`).
- `vsg::SpotLight` — `dvec3 position` (`SpotLight.h:29`), `dvec3 direction` (`SpotLight.h:30`), `double innerAngle = radians(30.0)` (`SpotLight.h:31`), `double outerAngle = radians(45.0)` (`SpotLight.h:32`), `double radius` (`SpotLight.h:33`).

Convenience helper:

- `vsg::ref_ptr<vsg::Node> vsg::createHeadlight();` (`Light.h:49`). Returns an `AbsoluteTransform` bundling one white `AmbientLight` (intensity `0.0044f`) and one white `DirectionalLight` (intensity `0.9956f`, direction `(0,0,-1)`) (`Light.cpp:72-89`). Because it is built on an `AbsoluteTransform` and added under a `View`, the headlight **tracks the camera**. Takes no arguments.

Shadows: set a shadow-casting light's `shadowSettings` to a `vsg::ShadowSettings` subclass — `vsg::HardShadows::create(numShadowMaps)` (`HardShadows.h:23`), `vsg::SoftShadows::create(numShadowMaps, penumbraRadius)` (`SoftShadows.h:23`), or `vsg::PercentageCloserSoftShadows::create(numShadowMaps)` (`PercentageCloserSoftShadows.h:23`) — and tune cascades on `view->viewDependentState`. Only `DirectionalLight`/`SpotLight` cast shadows. `View::create()` defaults to `RECORD_ALL` (`View.h:31,38`), so shadow-map recording is on and `compile()` auto-sizes the maps; tune cascades via `maxShadowDistance`/`shadowMapBias`/`lambda` on `view->viewDependentState` (`state/ViewDependentState.h:154-156`). Full grounded component: `references/components/shadows.md`.

## Best Practices

- Add a light to the **`View`** (sibling of the scene under the view) so it is camera-relative; this is the headlight idiom (`examples/app/vsgviewer/vsgviewer.cpp:323-326`).
- For a positioned point/spot light, place it under a `Transform` (or with explicit `position`) so it lives at a world location; `vsglights` decorates point/spot lights with a `vsg::CullGroup` so they can be culled (`examples/lighting/vsglights/vsglights.cpp:147-154`).
- Assign members directly: `light->color.set(r,g,b);` and `light->intensity = ...;` — there are no `setColor`/`setIntensity` methods.
- Keep ambient intensity low and let a directional light carry most illumination — mirrors `createHeadlight`'s 0.0044 / 0.9956 split (`Light.cpp:77,82`).
- If you build a `View`/`RenderGraph` by hand and want the free headlight, just call `view->addChild(vsg::createHeadlight());`.

## Composition examples

One-liner headlight added to a hand-wired `View` (camera-tracking; fixes black geometry):

```cpp
auto view = vsg::View::create(camera);
view->addChild(vsg::createHeadlight());   // ambient + directional, tracks camera
view->addChild(vsg_scene);                // Builder/lit scene now lit

auto renderGraph = vsg::RenderGraph::create(window, view);
auto commandGraph = vsg::CommandGraph::create(window, renderGraph);
```
(idiom: `examples/app/vsgviewer/vsgviewer.cpp:323-329`)

Explicit directional light (sun) with control over direction, color, intensity:

```cpp
auto directionalLight = vsg::DirectionalLight::create();
directionalLight->name = "sun";
directionalLight->color.set(1.0f, 1.0f, 1.0f);   // vec3, public member
directionalLight->intensity = 0.95f;             // float, public member
directionalLight->direction.set(0.0f, -1.0f, -1.0f);  // dvec3
scene->addChild(directionalLight);
```
(idiom: `examples/lighting/vsglights/vsglights.cpp:130-134`, `examples/lighting/vsgshadow/vsgshadow.cpp:532-536`)

## Source references

- `include/vsg/lighting/Light.h:25` — `class Light : public Inherit<Node, Light>`; members `name:31`, `color:32`, `intensity:33`, `shadowSettings:34`.
- `include/vsg/lighting/Light.h:49` — `extern ... ref_ptr<vsg::Node> createHeadlight();`
- `src/vsg/lighting/Light.cpp:72-89` — `createHeadlight()` builds `AmbientLight` (0.0044) + `DirectionalLight` (0.9956) under an `AbsoluteTransform`.
- `src/vsg/app/RenderGraph.cpp:222` — `if (assignHeadlight) view->addChild(createHeadlight());`
- `include/vsg/app/CommandGraph.h:66` — `createCommandGraphForView(..., bool assignHeadlight = true)`.
- `include/vsg/lighting/AmbientLight.h:23`, `DirectionalLight.h:23,29`, `PointLight.h:23,29`, `SpotLight.h:23,29-33`.
- `examples/app/vsgviewer/vsgviewer.cpp:323-326` — `view->addChild(vsg::createHeadlight());`
- `examples/app/vsgoverlay/vsgoverlay.cpp:107-109` — one headlight shared across two views.
- `examples/lighting/vsglights/vsglights.cpp:120-190` — ambient/directional/point/spot construction.
- `examples/lighting/vsgshadow/vsgshadow.cpp:294-314,532-536` — `shadowSettings` assignment.

## Common mistakes

- **Black / unlit geometry trap.** Building a scene with `vsg::Builder` (whose `StateInfo.lighting` defaults to `true`) or any lit `ShaderSet`, then hand-wiring a `View` with no `Light` node. The lit shader reads an empty `LightData` uniform (`Light.h:23`) and every lit surface renders black. **Fix:** add a light to the view — `view->addChild(vsg::createHeadlight());` (`vsgviewer.cpp:325`) — or, if you used `createCommandGraphForView`/`createRenderGraphForView`, rely on their default headlight (`RenderGraph.cpp:222`).
- Disabling the auto headlight by passing `assignHeadlight = false` to `createCommandGraphForView` / `createRenderGraphForView` and then forgetting to add your own light — same black result.
- Expecting `AmbientLight` alone to shade form: its `createHeadlight` intensity is only `0.0044f` (`Light.cpp:77`); without a directional/point/spot light surfaces look flat and nearly black.
- Calling non-existent setters like `light->setColor(...)`. `color`/`intensity`/`direction`/`position` are **public data members**, assigned directly.

## Things to never invent

- No `setColor` / `setIntensity` / `setDirection` / `setPosition` / `setName` methods — these are public data members (`Light.h:31-33`, `DirectionalLight.h:29`, `PointLight.h:29`). Assign them directly.
- `createHeadlight()` takes **no arguments** (`Light.h:49`) — no color/intensity/direction parameters.
- Do not invent a `Light::create(...)` overload taking color/intensity; construct with `X::create()` then set members (idiom across `vsglights.cpp`).
- Do not assume a `Light` auto-illuminates a scene by mere construction; it must be **added to the View/scene graph** as a node to be recorded.
- `DirectionalLight.h` has no `position` member and `PointLight.h`/`AmbientLight.h` have no `direction` member — do not cross them.
- Do not invent shadow APIs beyond assigning `shadowSettings` to a real subclass (`HardShadows.h:23`, `SoftShadows.h:23`) and tuning `view->viewDependentState` (`state/ViewDependentState.h`); see `examples/lighting/vsgshadow/vsgshadow.cpp` for full wiring. In particular there is no `View::maxNumShadowMaps` and no `Light::setShadowSettings()`.
