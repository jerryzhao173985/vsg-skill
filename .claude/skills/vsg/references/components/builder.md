---
title: Builder (procedural geometry) — Builder, GeometryInfo, StateInfo
description: Creates renderable scene-graph subgraphs for primitive shapes (box, sphere, quad, cylinder, cone, capsule, disk, height field) from GeometryInfo + StateInfo, without loading files.
---

## Public include
Use the barrel include and the `vsg::` namespace:
```cpp
#include <vsg/all.h>      // barrel header
// specific header for reference: include/vsg/utils/Builder.h
```
`Builder`, `GeometryInfo`, and `StateInfo` are declared in `include/vsg/utils/Builder.h:109`, `include/vsg/utils/Builder.h:53`, `include/vsg/utils/Builder.h:25`.

## When to use
Use `Builder` to generate primitive geometry subgraphs procedurally at runtime (prototyping, placeholders, instanced/billboarded markers, intersection feedback) instead of authoring meshes or loading model files. Not for loading 3rd-party model files — use `vsg::read_cast<vsg::Node>(filename, options)` for that. Not for arbitrary custom meshes — build a `vsg::VertexIndexDraw` / `vsg::Geometry` yourself. For one-off transform placement of the result, wrap it in a `vsg::MatrixTransform`.

## Key API
- `vsg::Builder::create()` — static factory returning `vsg::ref_ptr<Builder>`; `Builder` derives via `Inherit<Object, Builder>` (`include/vsg/utils/Builder.h:109`).
- `builder->createBox(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:123`).
- `builder->createCapsule(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:124`).
- `builder->createCone(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:125`).
- `builder->createCylinder(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:126`).
- `builder->createDisk(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:127`).
- `builder->createQuad(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:128`).
- `builder->createSphere(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:129`).
- `builder->createHeightField(info, stateInfo)` -> `ref_ptr<Node>` (`include/vsg/utils/Builder.h:130`).
- `builder->createStateGroup(stateInfo)` -> `ref_ptr<StateGroup>` (`include/vsg/utils/Builder.h:132`).
- `builder->assignCompileTraversal(ct)` — assign a `CompileTraversal` so shapes are GPU-compiled on creation for runtime addition (`include/vsg/utils/Builder.h:135`).
- `builder->options` (`ref_ptr<Options>`), `builder->sharedObjects` (`ref_ptr<SharedObjects>`), `builder->shaderSet` (`ref_ptr<ShaderSet>`), `builder->verbose` (`bool`, default `false`), `builder->compileTraversal` (`ref_ptr<CompileTraversal>`) (`include/vsg/utils/Builder.h:118`, `:119`, `:120`, `:121`, `:137`).

### GeometryInfo members (all `vec3`/`vec4`/`mat4` = float precision)
- `position` = `{0,0,0}`; `dx` = `{1,0,0}`; `dy` = `{0,1,0}`; `dz` = `{0,0,1}` — extents along each axis (`include/vsg/utils/Builder.h:63-66`).
- `color` = `{1,1,1,1}` (`include/vsg/utils/Builder.h:67`); `transform` = `mat4` identity (`include/vsg/utils/Builder.h:68`).
- `cullNode` = `false` — decorate the subgraph with a `CullNode` when `true` (`include/vsg/utils/Builder.h:71`).
- `positions` / `colors` (`ref_ptr<Data>`) — per-instance arrays; `vec3Array` for instancing, `vec4Array` for billboards (`include/vsg/utils/Builder.h:91-93`).
- Convenience constructors/`set()` from `t_box<T>` or `t_sphere<T>` to derive position+extents (`include/vsg/utils/Builder.h:57-89`).

### StateInfo members (all `bool` unless noted)
- `lighting` = `true`; `two_sided` = `false`; `blending` = `false`; `greyscale` = `false`; `wireframe` = `false` (`include/vsg/utils/Builder.h:27-31`).
- `instance_colors_vec4` = `true`; `instance_positions_vec3` = `false`; `billboard` = `false` (overrides `instance_positions_vec3`) (`include/vsg/utils/Builder.h:32-34`).
- `image`, `displacementMap` (`ref_ptr<Data>`), `viewDescriptorSetLayout` (`ref_ptr<DescriptorSetLayout>`) (`include/vsg/utils/Builder.h:36-38`).

## Best Practices
- Always allocate via `vsg::Builder::create()` into a `vsg::ref_ptr<vsg::Builder>`; never `new Builder` — it is intrusively ref-counted through `vsg::Object` (`include/vsg/utils/Builder.h:109`).
- Set `builder->options = options;` before creating shapes so texture/displacement reads and shared objects resolve correctly (`examples/utils/vsgbuilder/vsgbuilder.cpp:20-21`).
- For shapes built before the frame loop and added to the scene graph up front, you do NOT call `assignCompileTraversal`; the normal `viewer->compile()` compiles the whole scene (`examples/utils/vsgbuilder/vsgbuilder.cpp:308`).
- For shapes created and added to the scene AFTER the viewer is running (e.g. interactively), call `builder->assignCompileTraversal(vsg::CompileTraversal::create(*viewer));` once after the viewer is configured, so each `create*` result is GPU-compiled before it is recorded (`include/vsg/utils/Builder.h:135`, `examples/utils/vsgintersection/vsgintersection.cpp:383`).
- `position`, `dx/dy/dz`, `color`, and `transform` are float (`vec3`/`vec4`/`mat4`), NOT double — do not pass `dvec3`/`dmat4`; place results in a double-precision `MatrixTransform` if you need world-scale offsets (`include/vsg/utils/Builder.h:63-68`).
- For instancing set `stateInfo.instance_positions_vec3 = true` and assign `geomInfo.positions` a `vsg::vec3Array`; for billboards set `stateInfo.billboard = true` and assign a `vsg::vec4Array` of `{x,y,z,scaleDistance}` (`include/vsg/utils/Builder.h:33-34`, `:91`, `examples/utils/vsgbuilder/vsgbuilder.cpp:133-162`).
- `billboard` overrides `instance_positions_vec3`; reset `state.billboard = false` and clear `geom.positions` before building non-billboard shapes from the same `GeometryInfo` (`include/vsg/utils/Builder.h:34`, `examples/utils/vsgintersection/vsgintersection.cpp:92-97`).
- A `create*` result may be a `StateGroup` (when state decoration is needed); detect with `node.cast<vsg::StateGroup>()` rather than assuming a plain `Group` (`examples/utils/vsgbuilder/vsgbuilder.cpp:198`).
- Reuse a single `Builder` across many shapes; it caches generated subgraphs internally per shape+state (`_boxes`, `_spheres`, ... maps keyed by `GeometryInfo`+`StateInfo`), so identical requests are shared (`include/vsg/utils/Builder.h:150-158`).
- `Builder` is non-copyable (copy ctor and assignment are deleted); pass it by `ref_ptr` (`include/vsg/utils/Builder.h:113-114`).
- Builder geometry uses a **lit** pipeline by default (`StateInfo.lighting=true`, `include/vsg/utils/Builder.h:27`), so it renders **black** unless a `vsg::Light` is in the view/scene. The windowed `createCommandGraphForView` adds a headlight for you; a hand-wired `View` needs `view->addChild(vsg::createHeadlight())`. See `references/components/lighting.md`.

## Composition examples
Static scene built before the frame loop (compiled by `viewer->compile()`):
```cpp
#include <vsg/all.h>

auto options = vsg::Options::create();
options->sharedObjects = vsg::SharedObjects::create();

auto builder = vsg::Builder::create();
builder->options = options;                 // resolve textures / shared state

vsg::GeometryInfo geomInfo;                  // float position/extents/color
geomInfo.dx.set(1.0f, 0.0f, 0.0f);
geomInfo.dy.set(0.0f, 1.0f, 0.0f);
geomInfo.dz.set(0.0f, 0.0f, 1.0f);

vsg::StateInfo stateInfo;
stateInfo.lighting = true;                   // default; set false for flat shading
// stateInfo.wireframe = true;               // optional wireframe rendering

auto scene = vsg::Group::create();
scene->addChild(builder->createBox(geomInfo, stateInfo));
geomInfo.position += geomInfo.dx * 1.5f;     // shift next shape over
scene->addChild(builder->createSphere(geomInfo, stateInfo));
// ... assign scene to a CommandGraph, then viewer->compile();
```
(distilled from `examples/utils/vsgbuilder/vsgbuilder.cpp:20-211`, `:308`)

Runtime/interactive creation — compile each new shape via the builder's compile traversal:
```cpp
#include <vsg/all.h>

// after viewer is fully configured (windows, tasks assigned):
builder->assignCompileTraversal(vsg::CompileTraversal::create(*viewer));

// later, e.g. inside an event handler, add a shape that is compiled on creation:
vsg::GeometryInfo geom;
vsg::StateInfo state;
geom.position = vsg::vec3(worldIntersection);          // float position
scenegraph->addChild(builder->createSphere(geom, state));
```
(distilled from `examples/utils/vsgintersection/vsgintersection.cpp:383`, `:55-84`)

## Source references
- `include/vsg/utils/Builder.h` — `Builder`, `GeometryInfo`, `StateInfo` declarations and all `create*` methods.
- `examples/utils/vsgbuilder/vsgbuilder.cpp` — full static-scene usage of every shape, instancing, billboards, textures, flat/wireframe/two-sided state.
- `examples/utils/vsgintersection/vsgintersection.cpp` — runtime/interactive creation with `assignCompileTraversal` and a live viewer.

## Common mistakes
- Allocating with `new vsg::Builder` -> use `vsg::Builder::create()` returning a `ref_ptr` (`include/vsg/utils/Builder.h:109`).
- Passing `dvec3`/double values into `GeometryInfo::position`/`transform` -> these are float `vec3`/`mat4`; convert and offset world-scale via a `MatrixTransform` (`include/vsg/utils/Builder.h:63-68`).
- Creating shapes at runtime and recording them without compilation -> call `assignCompileTraversal` first so each `create*` is GPU-ready (`include/vsg/utils/Builder.h:135`, `examples/utils/vsgintersection/vsgintersection.cpp:383`).
- Setting `instance_positions_vec3` with a `vec4Array` (or billboard with a `vec3Array`) -> instancing needs `vec3Array`, billboards need `vec4Array{x,y,z,scaleDistance}` (`include/vsg/utils/Builder.h:33-34`, `:91`).
- Assuming the returned node is always a `Group` -> it may be a `StateGroup`; use `node.cast<vsg::StateGroup>()` when you need that type (`examples/utils/vsgbuilder/vsgbuilder.cpp:198`).
- Leaving `billboard` true when building subsequent non-billboard shapes -> reset `state.billboard = false` and clear `positions` (`examples/utils/vsgintersection/vsgintersection.cpp:92-97`).

## Things to never invent
- There is NO `createTorus`, `createPlane`, `createPyramid`, `createTeapot`, `createGrid`, or `createTriangle` — the only shape methods are `createBox`, `createCapsule`, `createCone`, `createCylinder`, `createDisk`, `createQuad`, `createSphere`, `createHeightField` (`include/vsg/utils/Builder.h:123-130`).
- There is no `Builder::build()`, `Builder::compile()`, or `Builder::setup()` member; compilation is wired only through `assignCompileTraversal()` / `compileTraversal` (`include/vsg/utils/Builder.h:135-137`).
- `GeometryInfo` has no `radius`, `width`, `height`, `dimensions`, or `size` member — size is expressed via `dx`/`dy`/`dz` extents (`include/vsg/utils/Builder.h:63-66`).
- `GeometryInfo` has no `dvec3 position` — it is `vec3` (float) (`include/vsg/utils/Builder.h:63`).
- `StateInfo` has no `texture`, `material`, `doubleSided`, or `transparency` member — use `image`, `two_sided`, and `blending` (`include/vsg/utils/Builder.h:28`, `:29`, `:36`).
