---
title: The scene graph & its traversals
description: VSG scenes are a DAG of ref-counted nodes walked by double-dispatch visitors; RecordTraversal is the visitor that turns the graph into Vulkan commands, pushing matrix and state stacks as it descends.
---

## What this covers
- What a VSG scene graph IS — a directed acyclic graph of `Node`-derived objects with no parent pointers, walked by visitors via `accept`/`apply` double dispatch.
- Internal nodes (`Group`, `Transform`/`MatrixTransform`, `StateGroup`, `Switch`, `LOD`) that group/decorate subgraphs vs leaf draw nodes (`Geometry`, `VertexIndexDraw`) that emit Vulkan draw calls.
- How STATE (pipelines, descriptor sets) decorates a subgraph through `StateGroup` holding `StateCommand`s, and how that state is pushed/popped during recording.
- The traversal family — `Visitor`/`ConstVisitor` (general graph operations) vs `RecordTraversal` (the per-frame walk that records command buffers) — and exactly what `RecordTraversal` does to matrix and state stacks.

## Mental model
A VSG scene graph is a **DAG of `ref_ptr`-managed nodes**. Internal nodes own their children in a `std::vector<ref_ptr<Node>>` (`Group::children`, `include/vsg/nodes/Group.h:36-37`); there are no parent pointers, so a single subtree can legally be referenced by multiple parents (shared/instanced). The DAG is purely **data** — nodes carry no behaviour for "rendering" themselves directly. Behaviour comes from **visitors** that walk the graph.

Walking is **double dispatch**. Every node implements `accept(visitor)`, which the `Inherit<>` CRTP base auto-generates to call `visitor.apply(static_cast<Subclass&>(*this))` (`include/vsg/core/Inherit.h:70-72`). So `node->accept(recordTraversal)` lands in the most-derived `RecordTraversal::apply(const ConcreteType&)` overload. `Object::accept` defaults to `visitor.apply(*this)` (`src/vsg/core/Object.cpp:134-137`), and the base `apply(const Object&)` just calls `object.traverse(*this)` (`src/vsg/app/RecordTraversal.cpp:121-127`) — meaning unrecognised node types are transparently descended into.

`traverse` is what a node does to push the visitor into its children. `Group::traverse` calls `child->accept(visitor)` for each child (`include/vsg/nodes/Group.h:48-56`). This is the recursion engine: `apply` → (work) → `traverse` → child `accept` → child `apply` → ...

There are two distinct traversal styles. `Visitor` and `ConstVisitor` are the **general-purpose** walkers (compute bounds, find nodes, compile, animate) and are mutating vs const respectively. `RecordTraversal` is a **separate, non-Visitor class** (`include/vsg/app/RecordTraversal.h:73`) used once per frame per view: it walks a const graph and **records Vulkan commands into a command buffer**, doing view-frustum culling and LOD selection along the way. There is no separate `CullTraversal` type in this codebase — culling is inlined into `RecordTraversal::apply` for the culling node types (`LOD`, `CullGroup`, `CullNode`, `DepthSorted`) via `state->intersect(...)` / `state->lodDistance(...)` tests (`src/vsg/app/RecordTraversal.cpp:153-178`, `269-289`, `311-323`).

The key invariant for code generation: **transforms and state are scoped by subgraph, applied via push-down/pop-up stacks during `RecordTraversal`, not stored on individual draw nodes.** A `MatrixTransform` does not bake its matrix into geometry; it pushes onto `State::modelviewMatrixStack` on the way down and pops on the way up (`src/vsg/app/RecordTraversal.cpp:423-443`). A `StateGroup` does not attach a pipeline to a draw; it pushes its `StateCommand`s onto per-slot state stacks on the way down and pops them on the way up (`src/vsg/app/RecordTraversal.cpp:515-529`). State is only flushed to the GPU lazily, when a leaf draw node forces `state->record()` (`src/vsg/app/RecordTraversal.cpp:343-359`).

## Key types
- `Node` — abstract base for internal scene-graph nodes; specialises `new`/`delete` for the node allocator pool (`include/vsg/nodes/Node.h:23`).
- `Group` — the basic internal node, owns `children` (a vector of `ref_ptr<Node>`) (`include/vsg/nodes/Group.h:23,36-37`).
- `Transform` / `MatrixTransform` — pure-virtual transform base and its 4x4-matrix concrete form; `MatrixTransform::transform(mv)` returns `mv * matrix` (`include/vsg/nodes/MatrixTransform.h:23,32`).
- `StateGroup` — a `Group` that also holds `StateCommands` pushed/popped during recording to decorate its subgraph with pipeline/descriptor state (`include/vsg/nodes/StateGroup.h:31,37`).
- `StateCommand` — base for state-binding Vulkan commands (bind pipeline, bind descriptor sets); carries a `slot` index into the state stacks (`include/vsg/state/StateCommand.h:23,36`).
- `Geometry` / `VertexIndexDraw` — leaf draw nodes (derive from `Command`, not `Node`) that bind vertex/index buffers and issue draw calls during recording (`include/vsg/nodes/Geometry.h:29`, `include/vsg/nodes/VertexIndexDraw.h:24`).
- `RecordTraversal` — the per-frame visitor that records the graph into a `CommandBuffer`, owning the `State` that holds the matrix and state stacks (`include/vsg/app/RecordTraversal.h:73,177`).
- `Visitor` / `ConstVisitor` — general mutating/const graph-operation visitors dispatched the same way (`include/vsg/core/Inherit.h:70-71`).

## How it works (implementation-grounded)
**Group recursion.** `RecordTraversal::apply(const Group&)` simply calls `group.traverse(*this)` (`src/vsg/app/RecordTraversal.cpp:129-139`), and `Group::traverse(RecordTraversal&)` calls `child->accept(visitor)` for every child (`include/vsg/nodes/Group.h:56` + `48-52`). Recursion is pure delegation; a plain `Group` adds no state.

**Transform pushes the modelview stack.** `RecordTraversal::apply(const MatrixTransform&)` does, in order: `state->modelviewMatrixStack.push(mt)`, set `state->dirty = true`, optionally push/pop the frustum (`if (mt.subgraphRequiresLocalFrustum)`), `mt.traverse(*this)` to descend, then `state->modelviewMatrixStack.pop()` and dirty again (`src/vsg/app/RecordTraversal.cpp:423-443`). The push overload computes the new top as `matrixStack.top() * transform.matrix` — i.e. it post-multiplies the inherited modelview, so child transforms compose in parent→child order (`include/vsg/vk/State.h:144-148`). The generic `Transform` push uses the virtual `transform.transform(top())` (`include/vsg/vk/State.h:138-142`).

**StateGroup pushes/pops state-command stacks.** `RecordTraversal::apply(const StateGroup&)` grabs `stateCommands.begin()/end()`, calls `state->push(begin,end)`, traverses the children, then `state->pop(begin,end)` with the same iterators (`src/vsg/app/RecordTraversal.cpp:515-529`). `State::push` routes each command to its own slot: `stateStacks[(*itr)->slot].push(*itr)` and marks `dirty` (`include/vsg/vk/State.h:357-365`). Note `StateGroup::traverse(RecordTraversal&)` deliberately traverses **only children, not the stateCommands** (`include/vsg/nodes/StateGroup.h:75-78`) — the commands are applied via the stack mechanism, not visited as nodes. (For a general `Visitor`, `t_traverse` visits both stateCommands and children — `include/vsg/nodes/StateGroup.h:66-71` — because compile/IO need to see the commands.)

**Leaf draw nodes flush state, then record.** `RecordTraversal::apply(const Geometry&)` / `apply(const VertexIndexDraw&)` call `state->record()` and then `node.record(*(state->_commandBuffer))` (`src/vsg/app/RecordTraversal.cpp:343-359`). `State::record()` is where deferred state actually reaches the GPU: if `dirty`, it walks each active slot's `stateStacks[slot].record(commandBuffer)` and then `projectionMatrixStack.record` / `modelviewMatrixStack.record`, then clears `dirty` (`include/vsg/vk/State.h:338-355`). Each `StateStack::record` only re-binds when the top changed since last record (`stack[0]` caches the last-recorded value) (`include/vsg/vk/State.h:76-84`) — so redundant pipeline/descriptor binds are elided automatically. This is why state is *lazy*: nothing is bound when a `StateGroup` or `Transform` is entered; it is bound the moment a draw needs it.

**Culling & LOD happen inside RecordTraversal.** `apply(const LOD&)` computes `state->lodDistance(sphere)`, returns early (culls) if `< 0`, else picks the first child whose `minimumScreenHeightRatio` cutoff is met and accepts only that one (`src/vsg/app/RecordTraversal.cpp:153-178`). `apply(const CullGroup&)` / `apply(const CullNode&)` only traverse if `state->intersect(bound)` (`src/vsg/app/RecordTraversal.cpp:269-289`). `apply(const Switch&)` accepts only children whose mask passes `traversalMask & (overrideMask | child.mask)` (`src/vsg/app/RecordTraversal.cpp:291-302`).

**View resets the stacks.** `apply(const View&)` calls `state->dirtyStateStacks()` and `state->setProjectionAndViewMatrix(...)`, seeding `modelviewMatrixStack` with the camera view matrix before traversing the scene (`src/vsg/app/RecordTraversal.cpp:559-613`; `include/vsg/vk/State.h:316-336`). Dirtying forces every stack to re-record on the next draw so per-view state is reapplied.

## Rules that follow
- Build scenes by composing nodes with `addChild` (`include/vsg/nodes/Group.h:39-42`) and `StateGroup::add` for state commands (`include/vsg/nodes/StateGroup.h:48-51`); never give a node a parent pointer — there is none, and a subgraph may be shared by multiple parents.
- Put pipeline/descriptor binds in a `StateGroup` that is an **ancestor** of the draws they apply to; state is scoped to the StateGroup's subgraph and popped on exit (`src/vsg/app/RecordTraversal.cpp:515-529`). A `BindGraphicsPipeline` placed below the geometry it should affect will not bind in time.
- Position geometry with a `MatrixTransform` ancestor and set its `matrix`; do not pre-transform vertex data. The matrix composes with inherited transforms via `top() * matrix` during recording (`include/vsg/vk/State.h:144-148`, `src/vsg/app/RecordTraversal.cpp:427`).
- A `StateGroup` typically needs a `BindGraphicsPipeline` (or compatible) somewhere above any draw, because draws assume the state stacks are populated when `state->record()` runs (`include/vsg/vk/State.h:338-355`); an unbound pipeline at draw time is undefined Vulkan.
- Leaf draw nodes (`Geometry`, `VertexIndexDraw`) derive from `Command`, so they must be reachable by a `RecordTraversal` (i.e. inside a `CommandGraph`/`RenderGraph`/`View` subgraph), and their `compile(Context&)` must have run to upload buffers before the first record (`include/vsg/nodes/VertexIndexDraw.h:52`).
- Operations that walk or mutate the whole graph (compute bounds, search, compile) are `Visitor`/`ConstVisitor` subclasses dispatched via `accept` — implement `apply` overloads and call `node.traverse(*this)` to recurse (`include/vsg/nodes/Group.h:48-55`); do not hand-roll child iteration on raw `children`.

## Common mistakes
- "I set the transform matrix on the geometry node." -> Geometry has no transform; wrap it in a `MatrixTransform` and set `mt->matrix` (`include/vsg/nodes/MatrixTransform.h:30`, `examples/nodes/vsgtransform/vsgtransform.cpp:60-62`).
- "I added the `BindGraphicsPipeline` as a child of the draw node." -> State decorates a subgraph from above; add it to a `StateGroup` that is an ancestor via `stateGroup->add(...)` (`examples/app/vsgsubpass/vsgsubpass.cpp:198`, `src/vsg/app/RecordTraversal.cpp:524`).
- "RecordTraversal is a Visitor / I'll add an `apply` to my Visitor to record." -> `RecordTraversal` is its own class with its own dispatch, separate from `Visitor`/`ConstVisitor` (`include/vsg/app/RecordTraversal.h:73`, `include/vsg/core/Inherit.h:70-72`).
- "I expected binding the pipeline to record immediately when the StateGroup is entered." -> State is lazy; it is only flushed by `State::record()` at the next draw, and unchanged binds are skipped (`include/vsg/vk/State.h:76-84,338-355`).
- "I gave a node a parent so I could walk upward." -> The graph is a parent-pointerless DAG; collect ancestry yourself during a top-down `Visitor` if you need it (`include/vsg/nodes/Group.h:36-37`).
- "I traversed a StateGroup's `stateCommands` during recording to apply them." -> During `RecordTraversal` a `StateGroup` traverses only children; the commands flow through the state stacks, not the visit (`include/vsg/nodes/StateGroup.h:75-78`).

## Source references
- `include/vsg/nodes/Node.h` — Node base, node-allocator new/delete.
- `include/vsg/nodes/Group.h` — children container, `addChild`, `traverse`/`t_traverse`.
- `include/vsg/nodes/StateGroup.h` — `stateCommands`, `add`, RecordTraversal-only-children traverse.
- `include/vsg/nodes/Transform.h`, `include/vsg/nodes/MatrixTransform.h` — transform contract, `transform(mv)` = `mv * matrix`.
- `include/vsg/nodes/Geometry.h`, `include/vsg/nodes/VertexIndexDraw.h` — leaf draw nodes deriving from `Command`.
- `include/vsg/app/RecordTraversal.h` — RecordTraversal class, `apply` overloads, owned `State`.
- `include/vsg/state/StateCommand.h` — StateCommand base and `slot`.
- `include/vsg/core/Inherit.h`, `include/vsg/core/Object.h` — `accept`/`apply` double-dispatch.
- `include/vsg/vk/State.h` — `StateStack`/`MatrixStack`, `push`/`pop`/`record`, `dirtyStateStacks`, `setProjectionAndViewMatrix`.
- `src/vsg/nodes/Group.cpp`, `src/vsg/nodes/StateGroup.cpp` — child/stateCommand storage and IO.
- `src/vsg/app/RecordTraversal.cpp` — the per-frame walk: Group recursion, Transform stack push/pop, StateGroup push/pop, leaf record, LOD/cull/Switch logic, View setup.
- `src/vsg/core/Object.cpp` — default `accept` dispatch.
- `examples/nodes/vsgtransform/vsgtransform.cpp` — MatrixTransform composition and `addChild`.
- `examples/app/vsgsubpass/vsgsubpass.cpp` — StateGroup holding pipeline/descriptor binds over draw subgraphs.
