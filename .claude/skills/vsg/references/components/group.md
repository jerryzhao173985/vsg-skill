---
title: Group (and Node base)
description: The basic scene-graph container holding an ordered vector of child nodes, traversed by visitors via accept().
---

## Public include
Consumers normally pull in the barrel header and use the `vsg::` namespace:

```cpp
#include <vsg/all.h>   // barrel include
// specific headers for reference:
// #include <vsg/nodes/Group.h>
// #include <vsg/nodes/Node.h>
```

`Group` is declared in `include/vsg/nodes/Group.h:23` and the abstract base `Node` in `include/vsg/nodes/Node.h:23`.

## When to use
Use `Group` when you need a general-purpose internal node that holds a variable number of children with no extra semantics — it just owns and traverses its `children`. Not for a fixed 4-child quad tree (use `QuadGroup`, `include/vsg/nodes/QuadGroup.h:27`), not for selectively enabling/disabling children (use `Switch`), not for attaching Vulkan state to a subgraph (use `StateGroup`), and not for distance/screen-ratio child selection (use `LOD`, see `examples/nodes/vsglod/vsglod.cpp:37`). `Node` itself is the abstract base for all internal scene-graph nodes (`include/vsg/nodes/Node.h:21`) and is rarely instantiated directly except as a leaf placeholder (`examples/nodes/vsggroups/vsggroups.cpp:122`).

## Key API
- `vsg::Group::create(numChildren = 0)` — factory from `Inherit`, returns `vsg::ref_ptr<Group>`; optional preallocated child count (`include/vsg/core/Inherit.h:35`, ctor default `include/vsg/nodes/Group.h:26`).
- `children` — public `std::vector<ref_ptr<Node>, ...>` of children, directly assignable/indexable (`include/vsg/nodes/Group.h:36`).
- `addChild(vsg::ref_ptr<Node> child)` — appends a child via `children.push_back` (`include/vsg/nodes/Group.h:39`).
- `accept(Visitor&)` / `accept(ConstVisitor&)` / `accept(RecordTraversal&)` — visitor dispatch wired by `Inherit` (`include/vsg/core/Inherit.h:70`).
- `traverse(Visitor&)` / `traverse(ConstVisitor&)` / `traverse(RecordTraversal&)` — visits each child's `accept()` (`include/vsg/nodes/Group.h:54`).
- `t_traverse(node, visitor)` — static templated traversal helper, iterates `node.children` calling `child->accept(visitor)` (`include/vsg/nodes/Group.h:48`).
- `clone(copyop)` — returns a copy via `Group::create(*this, copyop)` (`include/vsg/nodes/Group.h:45`).
- `read(Input&)` / `write(Output&)` — serialization hooks (`include/vsg/nodes/Group.h:58`).

## Best Practices
- Always construct with `vsg::Group::create(...)`, never `new vsg::Group` — `create` returns a `vsg::ref_ptr<Group>` and the object is intrusively reference-counted (`include/vsg/core/Inherit.h:35`).
- Hold the returned `ref_ptr`; children are owned as `ref_ptr<Node>` in `children`, so a node stays alive as long as any parent group references it and is freed automatically when the last `ref_ptr` drops (`include/vsg/nodes/Group.h:36`).
- Add children with `addChild(child)` for the common append case; assign or index `children` directly only when you preallocated with `create(n)` (e.g. `t->children[0] = ...` after `Group::create(4)`) (`include/vsg/nodes/Group.h:39`, `examples/nodes/vsggroups/vsggroups.cpp:125,132`).
- Children are stored and traversed in insertion order; `traverse` calls `child->accept(visitor)` for every child in sequence (`include/vsg/nodes/Group.h:51`).
- A scene graph is a DAG: the same child `ref_ptr<Node>` may be added to multiple groups for sharing. There is no parent pointer on `Node`, so a node does not know its parents (`include/vsg/nodes/Node.h:23`).
- Compile the scene before the frame loop — call `viewer->compile()` after assigning the scene to a `CommandGraph`/`View` so Vulkan objects in the subgraph are created; record-time traversal of an uncompiled subgraph is invalid (`include/vsg/app/Viewer.h:107`, `examples/app/vsghelloworld/vsghelloworld.cpp:74`).
- `RecordTraversal` traverses the group on the record thread via the const `traverse(RecordTraversal&)`; mutating `children` while recording is unsafe (`include/vsg/nodes/Group.h:56`).
- New node types should derive via `vsg::Inherit<Group, MyGroup>` (or `Inherit<Node, MyNode>`), which wires `create()`, `accept()`, RTTI, and ref-counting — do not derive from `Group`/`Node` directly (`include/vsg/core/Inherit.h:25`).

## Composition examples

Building a simple scene-graph subtree (distilled from `examples/nodes/vsgtransform/vsgtransform.cpp:56`):

```cpp
#include <vsg/all.h>

// root container
auto scene = vsg::Group::create();                 // ref_ptr<vsg::Group>

auto tm = vsg::MatrixTransform::create();           // a Group-derived transform
tm->matrix = vsg::translate(1.0, 0.0, 0.0);
tm->addChild(model);                                // model is a ref_ptr<vsg::Node>

scene->addChild(tm);                                // group owns the transform subtree
```

Preallocating a fixed-arity group and assigning children by index (from `examples/nodes/vsggroups/vsggroups.cpp:125-135`):

```cpp
#include <vsg/all.h>

auto t = vsg::Group::create(4);                     // reserve 4 child slots
t->children[0] = vsg::Node::create();               // index-assign into children vector
t->children[1] = vsg::Node::create();
t->children[2] = vsg::Node::create();
t->children[3] = vsg::Node::create();
// traverse the whole tree with a visitor:
t->accept(myVisitor);                               // dispatches to visitor.apply(Group&)
```

## Source references
- `include/vsg/nodes/Group.h` — `Group` declaration: `children`, `addChild`, `traverse`, `t_traverse`, `clone`.
- `include/vsg/nodes/Node.h` — abstract `Node` base and custom new/delete via the `Allocator`.
- `include/vsg/core/Inherit.h` — `create()` factory and `accept()` visitor dispatch wiring.
- `include/vsg/nodes/QuadGroup.h` — fixed 4-child sibling for contrast.
- `examples/nodes/vsggroups/vsggroups.cpp` — building/traversing `Group` (and `QuadGroup`) trees, index assignment, custom visitors.
- `examples/nodes/vsgtransform/vsgtransform.cpp` — idiomatic `Group::create()` + `addChild` scene assembly.

## Common mistakes
- Writing `new vsg::Group(...)` -> use `vsg::Group::create(...)` which returns a managed `ref_ptr` (`include/vsg/core/Inherit.h:35`).
- Indexing `children[i] = ...` without preallocating -> call `vsg::Group::create(n)` first, or use `addChild` to grow the vector (`include/vsg/nodes/Group.h:26`, `:39`).
- Storing children as raw `Node*` -> store `vsg::ref_ptr<Node>`; `children` is a vector of `ref_ptr<Node>` and owning by raw pointer breaks ref-counting (`include/vsg/nodes/Group.h:36`).
- Calling a method on a possibly-null child during traversal expecting `Group` to guard it -> `traverse` calls `child->accept(visitor)` unconditionally; do not leave null entries in `children` (`include/vsg/nodes/Group.h:51`).
- Reaching for a fixed-arity quad tree with `Group` and worrying about overhead -> use `QuadGroup` whose 4 children are a `std::array` and must all be set before traversal (`include/vsg/nodes/QuadGroup.h:33`).

## Things to never invent
- There is no `removeChild`, `removeChildren`, `getNumChildren`, `getChild`, `setChild`, or `insertChild` on `Group` — manipulate the public `children` vector directly (`include/vsg/nodes/Group.h:36`-`42`).
- There is no parent pointer or `getParent()` on `Node` or `Group`; nodes do not track parents (`include/vsg/nodes/Node.h:23`).
- `Group` has no `numChildren()` accessor; use `children.size()`.
- Do not call a non-existent `Group::traverse()` with no argument — `traverse` requires a `Visitor`, `ConstVisitor`, or `RecordTraversal` reference (`include/vsg/nodes/Group.h:54`-`56`).
- `setChild(index, child)` exists on the experimental `SharedPtrGroup` test type, not on `vsg::Group` — do not call it on a `vsg::Group` (`examples/nodes/vsggroups/vsggroups.cpp:182`).
