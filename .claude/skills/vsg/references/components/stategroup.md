---
title: StateGroup
description: A Group node that pushes/pops a list of Vulkan StateCommands (pipeline, descriptor-set bindings) onto the RecordTraversal state stacks for its subgraph.
---

## Public include
```cpp
#include <vsg/all.h>            // barrel include (recommended)
// or specifically:
#include <vsg/nodes/StateGroup.h>
```
Namespace is `vsg::`. The header declares `vsg::StateGroup` (`include/vsg/nodes/StateGroup.h:31`) and pulls in `StateCommand` (`include/vsg/nodes/StateGroup.h:17`).

## When to use
Use `StateGroup` when a subgraph must have Vulkan state applied to it during recording — i.e. you have one or more `vsg::StateCommand`s such as `BindGraphicsPipeline` or `BindDescriptorSet` to push before the children are recorded and pop afterwards (`include/vsg/nodes/StateGroup.h:27-30`). It is the typical root of a scene/command graph that owns the pipeline. Not for plain child grouping with no state — use `vsg::Group` (`include/vsg/nodes/Group.h:23`). Not for spatial transforms — use `vsg::MatrixTransform`. Not for on/off child selection — use `vsg::Switch`. Note that `StateCommand`s can also be attached directly as ordinary scene-graph nodes (e.g. `transform->addChild(bindDescriptorSet)`), but only a `StateGroup` gives proper push/pop scoping of state to its subgraph (`include/vsg/state/StateCommand.h:22`).

## Key API
- `vsg::StateGroup::create()` — factory returning `vsg::ref_ptr<StateGroup>`; inherited from `Inherit<Group, StateGroup>` (`include/vsg/nodes/StateGroup.h:31`).
- `void add(ref_ptr<StateCommand> stateCommand)` — append a state command to be pushed for the subgraph (`include/vsg/nodes/StateGroup.h:48`).
- `StateCommands stateCommands` — the ordered list of state commands; `StateCommands` is `std::vector<ref_ptr<StateCommand>, ...>` (`include/vsg/nodes/StateGroup.h:37`, `include/vsg/state/StateCommand.h:43`).
- `void addChild(vsg::ref_ptr<Node> child)` — inherited from `Group`; adds a child to the decorated subgraph (`include/vsg/nodes/Group.h:39`).
- `Children children` — inherited child list, `std::vector<ref_ptr<Node>, ...>` (`include/vsg/nodes/Group.h:36-37`).
- `template<class T> bool contains(const T value) const` — test whether a state command is present (`include/vsg/nodes/StateGroup.h:42-46`).
- `template<class T> void remove(const T value)` — erase a state command if present (`include/vsg/nodes/StateGroup.h:53-60`).
- `ref_ptr<ArrayState> prototypeArrayState` — optional custom array-to-vertex mapping when shaders don't treat array 0 as xyz vertex; default unset (`include/vsg/nodes/StateGroup.h:39-40`).

## Best Practices
- Always construct via `vsg::StateGroup::create()` and hold the result in a `vsg::ref_ptr<>`; never `new vsg::StateGroup`. Lifetime is managed by intrusive ref-counting (`include/vsg/nodes/StateGroup.h:31`, `include/vsg/core/Object.h`).
- Add state with `add(...)` and add geometry/transform children with `addChild(...)`; these are two distinct lists (`stateCommands` vs `children`). Putting a `StateCommand` in `add()` gives it proper subgraph-scoped push/pop; putting it via `addChild()` records it inline without scoping (`include/vsg/nodes/StateGroup.h:37,48`, `include/vsg/nodes/Group.h:36-39`).
- Order matters: a `BindGraphicsPipeline` must be added before, or pushed by an ancestor before, any `BindDescriptorSet` / draw that depends on its layout. Example adds pipeline then descriptor set (`examples/state/vsgtexturearray/vsgtexturearray.cpp:299-300`).
- Build all Vulkan-backed objects before the frame loop: call `viewer->compile()` after assigning the scene graph and creating command graphs, so the pipeline and descriptor sets referenced by the `StateCommand`s are realized on the device (`examples/state/vsgsampler/vsgsampler.cpp:307`).
- During `RecordTraversal` the `stateCommands` are pushed to the `vsg::State` stacks, applied lazily when a `Command` is encountered, and popped after the subgraph is traversed — so a `StateGroup`'s state only affects its own children (`include/vsg/nodes/StateGroup.h:27-30`). The `RecordTraversal` overload deliberately traverses only `children`, not `stateCommands` (`include/vsg/nodes/StateGroup.h:75-78`).
- Nest `StateGroup`s to layer state: a root `StateGroup` binds the pipeline + shared descriptors, inner `StateGroup`s bind per-object descriptor sets (`examples/state/vsgtexturearray/vsgtexturearray.cpp:333-340`).
- Set `prototypeArrayState` only when your vertex shader's array 0 is not the xyz position; otherwise leave it null and the default mapping is used (`include/vsg/nodes/StateGroup.h:39-40`). [VERIFY] No example in the examples tree sets this in C++ code.
- Visitor `traverse(Visitor&)` / `traverse(ConstVisitor&)` visit `stateCommands` first then `children`, so generic visitors see the state commands — only the record path skips them (`include/vsg/nodes/StateGroup.h:66-74`).

## Composition examples
```cpp
#include <vsg/all.h>

// Build the pipeline-binding state commands.
auto graphicsPipeline = vsg::GraphicsPipeline::create(pipelineLayout,
    vsg::ShaderStages{vertexShader, fragmentShader}, pipelineStates);
auto bindGraphicsPipeline = vsg::BindGraphicsPipeline::create(graphicsPipeline);
auto bindDescriptorSet = vsg::BindDescriptorSet::create(
    VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline->layout, 0, descriptorSet);

// StateGroup is the root: it decorates the whole subgraph with the pipeline + descriptors.
auto scenegraph = vsg::StateGroup::create();
scenegraph->add(bindGraphicsPipeline);   // pushed first  -> stateCommands
scenegraph->add(bindDescriptorSet);      // pushed second -> stateCommands

// Per-object subtree: transform + geometry are CHILDREN, not state.
auto transform = vsg::MatrixTransform::create(vsg::translate(position));
transform->addChild(geometry);
scenegraph->addChild(transform);

// Realize all Vulkan objects (pipeline, descriptor sets) before rendering.
auto commandGraph = vsg::createCommandGraphForView(window, camera, scenegraph);
viewer->compile();
// distilled from examples/state/vsgtexturearray/vsgtexturearray.cpp:262-329
// and examples/state/vsgsampler/vsgsampler.cpp:303-307
```

```cpp
// Nested StateGroup to layer per-object state under a shared-pipeline root.
auto tileSettingsGroup = vsg::StateGroup::create();
tileSettingsGroup->add(uniformBindDescriptorSet); // per-tile descriptor set
tileSettingsGroup->addChild(transform);           // its decorated subgraph
scenegraph->addChild(tileSettingsGroup);          // attach under the root StateGroup
// from examples/state/vsgtexturearray/vsgtexturearray.cpp:333-340
```

## Source references
- `include/vsg/nodes/StateGroup.h` — `StateGroup` declaration: `add`, `stateCommands`, `prototypeArrayState`, traverse overrides.
- `include/vsg/nodes/Group.h` — base `Group`: `addChild`, `children`.
- `include/vsg/state/StateCommand.h` — `StateCommand` base and `StateCommands` typedef.
- `examples/state/vsgtexturearray/vsgtexturearray.cpp` — root StateGroup binding pipeline + descriptors, and nested per-tile StateGroup.
- `examples/state/vsgsampler/vsgsampler.cpp` — StateGroup root plus `viewer->compile()` before the frame loop.

## Common mistakes
- Calling `new vsg::StateGroup` -> use `vsg::StateGroup::create()` so it is heap-allocated and ref-counted (`include/vsg/nodes/StateGroup.h:31`).
- Putting a pipeline/descriptor `StateCommand` into `addChild()` expecting scoped state -> use `add()` so it is pushed/popped around the subgraph (`include/vsg/nodes/StateGroup.h:48`).
- Putting geometry or transforms into `add()` -> those are `Node`s, use `addChild()` (`include/vsg/nodes/Group.h:39`).
- Adding a `BindDescriptorSet` whose layout comes from a pipeline that is never bound/compiled -> bind the pipeline (`add(bindGraphicsPipeline)`) and call `viewer->compile()` first (`examples/state/vsgtexturearray/vsgtexturearray.cpp:299`, `examples/state/vsgsampler/vsgsampler.cpp:307`).
- Using a plain `vsg::Group` and wondering why state isn't applied -> only `StateGroup` carries `stateCommands` (`include/vsg/nodes/Group.h:23` has no state list).

## Things to never invent
- There is no `setStateCommand`, `setState`, `bindPipeline`, or `addState` method — the only mutators are `add(ref_ptr<StateCommand>)` and `remove(...)` plus direct access to the `stateCommands` vector (`include/vsg/nodes/StateGroup.h:48,53`).
- `add()` takes a `ref_ptr<StateCommand>`, not a `GraphicsPipeline` or `DescriptorSet` directly — wrap those in `BindGraphicsPipeline` / `BindDescriptorSet` first (`include/vsg/nodes/StateGroup.h:48`, `include/vsg/state/GraphicsPipeline.h:110`, `include/vsg/state/BindDescriptorSet.h:91`).
- There is no `getStateCommands()` accessor; access the public `stateCommands` member directly (`include/vsg/nodes/StateGroup.h:37`).
- `prototypeArrayState` is a `ref_ptr<ArrayState>` member, not a method like `setArrayState()` (`include/vsg/nodes/StateGroup.h:40`).
- StateGroup does not skip its children during recording — its `traverse(RecordTraversal&)` records `children`; do not assume state-only nodes (`include/vsg/nodes/StateGroup.h:75-78`).
