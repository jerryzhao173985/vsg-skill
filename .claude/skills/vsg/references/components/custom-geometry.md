---
title: Custom geometry (hand-built mesh — data + GraphicsPipelineConfigurator + VertexIndexDraw)
description: Build an arbitrary renderable mesh from scratch — author vertex/normal/colour/index arrays, wire a pipeline from a stock ShaderSet via GraphicsPipelineConfigurator, and draw with VertexIndexDraw. There is no "mesh" class; a mesh is three independent pieces.
---

## Public include
Use the barrel include and the `vsg::` namespace:
```cpp
#include <vsg/all.h>      // barrel header
// reference headers:
//   include/vsg/nodes/VertexIndexDraw.h
//   include/vsg/utils/GraphicsPipelineConfigurator.h
//   include/vsg/utils/ShaderSet.h
//   include/vsg/state/material.h
//   include/vsg/core/Array.h
```

## When to use
Use this when you need an arbitrary mesh that `Builder` does not produce (custom topology, imported raw vertex data, generated surfaces) but still want stock Phong/PBR/flat shading without authoring GLSL. There is NO `vsg::Mesh` class to fill in — a custom mesh is THREE independent pieces you assemble yourself:
1. **DATA** — `vsg::Array<T>` buffers for positions/normals/colours/indices.
2. **PIPELINE** — a `ShaderSet` driven through a `GraphicsPipelineConfigurator`, copied onto a `StateGroup`.
3. **DRAW** — a `VertexIndexDraw` command holding the same arrays + indices.
For primitive shapes use `Builder` instead (see builder.md). For custom shading authored in GLSL, build a `ShaderSet` by hand — out of scope here. For the viewer/window/camera/frame-loop boilerplate see examples/model-viewer.md.

## Key API
DATA (`include/vsg/core/Array.h`) — the SAME `vsg::Array<T>` is both CPU authoring buffer and GPU buffer:
- `vsg::vec3Array::create({...})` — positions / normals (`include/vsg/core/Array.h:402`).
- `vsg::vec4Array::create({...})` — per-vertex colours (`include/vsg/core/Array.h:403`); `vsg::vec2Array` for tex coords (`:401`).
- `vsg::ushortArray::create({...})` — 16-bit indices (`include/vsg/core/Array.h:392`); `vsg::uintArray` for 32-bit (`:394`).

PIPELINE — `ShaderSet` factories (`include/vsg/utils/ShaderSet.h:207-213`):
- `vsg::createFlatShadedShaderSet(options)` / `vsg::createPhongShaderSet(options)` / `vsg::createPhysicsBasedRenderingShaderSet(options)`.
- `vsg::GraphicsPipelineConfigurator::create(shaderSet)` (`include/vsg/utils/GraphicsPipelineConfigurator.h:100`).
- `config->assignTexture(name, data, sampler=, dstArrayElement=)` -> `bool` — e.g. `"diffuseMap"` (`:121`).
- `config->assignDescriptor(name, data)` -> `bool` — e.g. `"material"` with a `PhongMaterialValue` (`:119`).
- `config->assignArray(DataList& arrays, name, vertexInputRate, array)` -> `bool` — appends `array` to `arrays` AND records the binding; names `"vsg_Vertex"`/`"vsg_Normal"`/`"vsg_TexCoord0"`/`"vsg_Color"` (`:118`).
- `config->enableArray(name, vertexInputRate, stride, format=)` -> `bool` — binding-only variant (no data yet), used by `Builder` (`:114`).
- `config->init()` — builds `layout`/`graphicsPipeline`/`bindGraphicsPipeline`; call once after all assign/enable (`:139`).
- `config->getSuitableArrayState()` -> `ref_ptr<ArrayState>` — for `StateGroup::prototypeArrayState` (`:142`).
- `config->copyTo(stateGroup, sharedObjects=)` -> `bool` — copies the state commands onto a `StateGroup` (`:148`); `copyTo(StateCommands&, ...)` overload also exists (`:145`).
- `config->baseAttributeBinding` — first binding index (`:109`).
- Material payloads: `vsg::PhongMaterialValue::create()` (`include/vsg/state/material.h:90`), `vsg::PbrMaterialValue::create()` (`:134`).

DRAW (`include/vsg/nodes/VertexIndexDraw.h`):
- `vsg::VertexIndexDraw::create()` — derives `Inherit<Command, VertexIndexDraw>` (`:24`).
- `vid->assignArrays(const DataList& arrays)` — vertex attribute arrays, ORDER-SENSITIVE (`:42`).
- `vid->assignIndices(ref_ptr<Data> indices)` (`:43`).
- `vid->indexCount` — set to `indices->size()` (`:32`); `vid->instanceCount` — `1` for a non-instanced mesh (`:33`); `vid->firstIndex`/`vid->vertexOffset`/`vid->firstInstance`/`vid->firstBinding` default `0` (`:34-38`).

## Best Practices
- **Attribute order is a contract.** The `config->assignArray(...)` (or `enableArray`) call order MUST mirror the `VertexIndexDraw::assignArrays(DataList)` order. They are bound by index, not by name — if you push normals into the `DataList` before positions, positions/normals silently swap and the mesh shades wrong with no error. `Builder` keeps them in lockstep: it `enableArray`s `vsg_Vertex, vsg_Normal, vsg_TexCoord0, vsg_Color` (`src/vsg/utils/Builder.cpp:95-107`) then pushes `vertices, normals, texcoords, colors` in the same order into `arrays` (`src/vsg/utils/Builder.cpp:388-394`).
- **`vsg_Color` is INSTANCE-rate in the stock ShaderSets.** `Builder` and `tile.cpp` both bind it `VK_VERTEX_INPUT_RATE_INSTANCE` (`src/vsg/utils/Builder.cpp:102,107`; `src/vsg/io/tile.cpp:489`), and the canonical example passes a single `vsg::vec4Value` (one colour for the whole instance), not a per-vertex array (`examples/utils/vsggraphicspipelineconfigurator/vsggraphicspipelineconfigurator.cpp:131,144`). For genuine **per-vertex** colour, assign a `vsg::vec4Array` sized to the vertex count with `VK_VERTEX_INPUT_RATE_VERTEX`.
- **Drive the material through `assignDescriptor("material", ...)`.** `Builder` looks up the `"material"` descriptor binding and defaults it to `PhongMaterialValue` when none supplied (`src/vsg/utils/Builder.cpp:88-93`).
- **Set `prototypeArrayState` so the camera can be framed.** `copyTo(stateGroup)` copies only the state COMMANDS; `ComputeBounds` needs the array state to read vertex positions. `Builder` sets `stateGroup->prototypeArrayState = config->getSuitableArrayState()` (`src/vsg/utils/Builder.cpp:165`).
- **A lit ShaderSet needs a light.** Phong/PBR render black without illumination; `createCommandGraphForView` adds a headlight by default (see lighting.md).
- **`init()` once, after all assigns.** Wrap with `sharedObjects->share(config, [](auto gpc){ gpc->init(); })` to dedupe pipelines across instances (`src/vsg/utils/Builder.cpp:154-157`).

## Composition examples
Delta only — drop this where you build your scene root; viewer/window/camera/loop live in examples/model-viewer.md. Mirrors `examples/utils/vsggraphicspipelineconfigurator/vsggraphicspipelineconfigurator.cpp:77-170`:
```cpp
auto shaderSet = vsg::createPhongShaderSet(options);
auto config = vsg::GraphicsPipelineConfigurator::create(shaderSet);

auto mat = vsg::PhongMaterialValue::create();
mat->value().diffuse.set(1.0f, 1.0f, 1.0f, 1.0f);
config->assignDescriptor("material", mat);                 // descriptor, not attribute

// (1) DATA — same Array<T> is CPU authoring buffer and GPU buffer
auto vertices = vsg::vec3Array::create({ /* positions */ });
auto normals  = vsg::vec3Array::create({ /* one per vertex */ });
auto colors   = vsg::vec4Value::create(vsg::vec4{1.0f, 1.0f, 1.0f, 1.0f}); // INSTANCE colour
auto indices  = vsg::ushortArray::create({0,1,2, 2,3,0 /* ... */});

// (2) PIPELINE — assignArray order defines the binding contract
vsg::DataList vertexArrays;
config->assignArray(vertexArrays, "vsg_Vertex", VK_VERTEX_INPUT_RATE_VERTEX,   vertices);
config->assignArray(vertexArrays, "vsg_Normal", VK_VERTEX_INPUT_RATE_VERTEX,   normals);
config->assignArray(vertexArrays, "vsg_Color",  VK_VERTEX_INPUT_RATE_INSTANCE, colors);
config->init();

auto stateGroup = vsg::StateGroup::create();
config->copyTo(stateGroup);
stateGroup->prototypeArrayState = config->getSuitableArrayState(); // so ComputeBounds works

// (3) DRAW — same arrays/indices, SAME order as assignArray above
auto vid = vsg::VertexIndexDraw::create();
vid->assignArrays(vertexArrays);          // not the raw arrays — the DataList config filled
vid->assignIndices(indices);
vid->indexCount = static_cast<uint32_t>(indices->size());
vid->instanceCount = 1;
stateGroup->addChild(vid);
// stateGroup is now a renderable subgraph; add it (optionally under a MatrixTransform) to the scene
```
Note: the canonical example uses the lower-level `BindVertexBuffers`/`BindIndexBuffer`/`DrawIndexed` trio (`vsggraphicspipelineconfigurator.cpp:150-153`); `VertexIndexDraw` collapses those three commands into one (`include/vsg/nodes/VertexIndexDraw.h:22-23`) and is what `Builder` emits (`src/vsg/utils/Builder.cpp:386-398`).

## Source references
- `include/vsg/nodes/VertexIndexDraw.h:24,32,42,43` — class, `indexCount`, `assignArrays`, `assignIndices`.
- `include/vsg/utils/GraphicsPipelineConfigurator.h:100,114,118,119,121,139,142,148` — create/enableArray/assignArray/assignDescriptor/assignTexture/init/getSuitableArrayState/copyTo.
- `include/vsg/utils/ShaderSet.h:207-213` — `createFlatShadedShaderSet`/`createPhongShaderSet`/`createPhysicsBasedRenderingShaderSet`.
- `include/vsg/state/material.h:90,134` — `PhongMaterialValue`, `PbrMaterialValue`.
- `include/vsg/core/Array.h:392,394,401-403` — `ushortArray`/`uintArray`/`vec2Array`/`vec3Array`/`vec4Array`.
- `src/vsg/utils/Builder.cpp:44-50,88-93,95-107,154-157,160-166` — authoritative ShaderSet→Configurator→assign→init→copyTo idiom + `prototypeArrayState`.
- `src/vsg/utils/Builder.cpp:386-398` — geometry assembly: `DataList` push order then `VertexIndexDraw` wiring.
- `examples/utils/vsggraphicspipelineconfigurator/vsggraphicspipelineconfigurator.cpp:77-170` — canonical end-to-end build.

## Common mistakes
- **Assuming a `vsg::Mesh` / `vsg::Geometry`-with-everything class exists.** It does not; you assemble data + pipeline + draw yourself.
- **`assignArray` order not matching `assignArrays(DataList)` order** — silent attribute swap (positions read as normals). Keep one ordered `DataList` and pass that exact list to both.
- **Passing the raw vertex arrays to `vid->assignArrays(...)`** instead of the `DataList` that `config->assignArray` populated — pass the `DataList`.
- **Per-vertex colour via the default `vsg_Color` binding** — stock ShaderSets declare it INSTANCE-rate; use `VK_VERTEX_INPUT_RATE_VERTEX` with a `vec4Array` sized to the vertex count.
- **Forgetting `config->init()` before `copyTo`** — the pipeline objects are built in `init()` (`:139`); `copyTo` has nothing to copy without it.
- **Omitting `prototypeArrayState`** — `ComputeBounds` cannot read positions, so the camera frames an empty/zero bound and the mesh appears off-screen.
- **Calling `assignTexture`/material "uniform" via deprecated names** — `assignUniform`/`enableUniform` are deprecated aliases (`GraphicsPipelineConfigurator.h:129`); use `assignDescriptor`.

## Things to never invent
- No `vsg::Mesh` class, no `setVertices()`/`setNormals()`/`setIndices()` setters — only `assignArrays(DataList)` and `assignIndices(Data)` (`include/vsg/nodes/VertexIndexDraw.h:42-43`).
- `GraphicsPipelineConfigurator` has `assignArray`/`enableArray`/`assignDescriptor`/`assignTexture`/`init`/`copyTo`/`getSuitableArrayState` — do not invent `addAttribute`, `setShaderSet`, `build()`, or `compile()` on it.
- `VertexIndexDraw` exposes `indexCount`/`instanceCount`/`firstIndex`/`vertexOffset`/`firstInstance`/`firstBinding` as plain members (`:32-38`) — they are not setter methods.
- Do not invent a `vsg::createCustomShaderSet()`; the only stock factories are the three in `ShaderSet.h:207-213`.
- Do not assume `vsg_Color` accepts a per-vertex array by default — it is INSTANCE-rate in the stock ShaderSets.
