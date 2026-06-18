---
title: Procedural geometry (vsgbuilder)
description: Generate primitive shapes at runtime with Builder instead of loading model files.
---

## What to copy
- The `Builder` setup: a `SharedObjects` on `Options`, `builder->options = options`, then `createBox`/`createSphere`/`createCylinder`/… into a `Group`.
- Reusing one `GeometryInfo` + `StateInfo` across shapes, shifting `geomInfo.position` between calls.
- Toggling `StateInfo` flags (`lighting`, `wireframe`, `two_sided`) to change rendering without touching shaders.

## The program
`examples/utils/vsgbuilder/vsgbuilder.cpp` — exercises every `Builder` shape plus instancing, billboards, textures, and flat/wireframe/two-sided state. The reference for building geometry without asset files.

## Key lines
- Shared objects on options: `examples/utils/vsgbuilder/vsgbuilder.cpp:18`
- `Builder::create()` + assign options: `examples/utils/vsgbuilder/vsgbuilder.cpp:20`, `:21`
- `GeometryInfo` / `StateInfo`: `examples/utils/vsgbuilder/vsgbuilder.cpp:23`, `:29`
- `createBox` / `createSphere` / `createCylinder`: `examples/utils/vsgbuilder/vsgbuilder.cpp:194`, `:211`, `:232`

## Snippet (distilled, compiles)

```cpp
auto options = vsg::Options::create();
options->sharedObjects = vsg::SharedObjects::create();   // share pipelines/state across shapes

auto builder = vsg::Builder::create();
builder->options = options;

auto scene = vsg::Group::create();
vsg::GeometryInfo geomInfo;   // float position/extents/color
vsg::StateInfo stateInfo;     // lighting/wireframe/two_sided/...

scene->addChild(builder->createBox(geomInfo, stateInfo));
geomInfo.position += geomInfo.dx * 1.5f;
scene->addChild(builder->createSphere(geomInfo, stateInfo));
// then wire `scene` into a Camera + CommandGraph and viewer->compile() (see model-viewer)
```

## Related
- Full recipe: `references/patterns.md` (Recipe 2). API detail: `references/components/builder.md`.
