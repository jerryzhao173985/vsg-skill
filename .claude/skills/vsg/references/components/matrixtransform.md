---
title: MatrixTransform (and Transform / AbsoluteTransform)
description: Positions a subgraph by pre-multiplying a 4x4 double-precision matrix into the modelview during RecordTraversal.
---

## Public include
```cpp
#include <vsg/all.h>            // barrel include (recommended)
// or the specific header:
#include <vsg/nodes/MatrixTransform.h>
```
Namespace is `vsg::`. The header includes `<vsg/nodes/Transform.h>` (`include/vsg/nodes/MatrixTransform.h:15`), which in turn includes `<vsg/nodes/Group.h>` (`include/vsg/nodes/Transform.h:15`).

## When to use
Use `vsg::MatrixTransform` to position/rotate/scale a subgraph by a relative 4x4 matrix that is multiplied into the inherited modelview matrix from above (`include/vsg/nodes/MatrixTransform.h:20-22`). Use `vsg::AbsoluteTransform` instead when the matrix should replace, not multiply, the inherited modelview — e.g. anchoring something in absolute world space such as a skybox or a light volume (`include/vsg/nodes/AbsoluteTransform.h:20-22`). `vsg::Transform` itself is a pure-virtual base (its `transform()` is abstract, `include/vsg/nodes/Transform.h:34`) — never instantiate it directly; pick `MatrixTransform` or `AbsoluteTransform`. For a plain non-positioning grouping node use `vsg::Group`.

## Key API
- `vsg::MatrixTransform::create()` — factory returning `ref_ptr<MatrixTransform>`, default-constructs an identity matrix (`include/vsg/nodes/MatrixTransform.h:26`, via `vsg::Inherit`).
- `vsg::MatrixTransform::create(const vsg::dmat4& in_matrix)` — factory taking an initial matrix (`include/vsg/nodes/MatrixTransform.h:28`).
- `vsg::dmat4 matrix;` — the public, double-precision transform matrix; assign to it directly (`include/vsg/nodes/MatrixTransform.h:30`).
- `dmat4 transform(const dmat4& mv) const` — returns `mv * matrix` (pre-multiply), called during RecordTraversal (`include/vsg/nodes/MatrixTransform.h:32`).
- `void addChild(vsg::ref_ptr<Node> child)` — inherited from `vsg::Group`; attach the subgraph to be transformed (`include/vsg/nodes/Group.h:39`).
- `bool subgraphRequiresLocalFrustum = true;` — inherited from `Transform`; set `false` when the subgraph has no cull nodes (LOD/CullGroup) to skip frustum transform into local frame (`include/vsg/nodes/Transform.h:30`).
- Matrix builders (from `<vsg/maths/transform.h>`): `vsg::translate(x,y,z)` / `vsg::translate(vec)` (`include/vsg/maths/transform.h:67`, `:79`), `vsg::rotate(angle_radians, x,y,z)` (`include/vsg/maths/transform.h:45`), `vsg::rotate(const t_quat&)` (`include/vsg/maths/transform.h:21`), `vsg::scale(s)` / `vsg::scale(sx,sy,sz)` (`include/vsg/maths/transform.h:86`, `:98`).
- `vsg::radians(double degrees)` — degrees-to-radians for rotate angles (`include/vsg/maths/common.h:32`).

## Best Practices
- Allocate via `MatrixTransform::create(...)`, never `new`; it returns a `vsg::ref_ptr` and is intrusively ref-counted (`include/vsg/nodes/MatrixTransform.h:26`).
- The matrix is `vsg::dmat4` — double precision — deliberately, so large-coordinate / whole-earth scenes do not lose precision; build it from the `double`-typed helpers (assign `vsg::translate(0.0, 0.0, r)` with `double` literals) (`include/vsg/nodes/MatrixTransform.h:30`).
- Compose transforms in the conventional order: outer-to-inner reads left-to-right because `transform()` pre-multiplies (`mv * matrix`). To scale/rotate about a pivot, write `translate(centre) * scale(...) * translate(-centre)` (`examples/nodes/vsgtransform/vsgtransform.cpp:67`, `include/vsg/nodes/MatrixTransform.h:32`).
- Always pass rotation angles in radians via `vsg::rotate(vsg::radians(deg), ...)`; `rotate` takes radians, not degrees (`include/vsg/maths/transform.h:45`, `include/vsg/maths/common.h:32`).
- Add the transform to the scene graph (as a child of a `Group` or another transform) and `viewer->compile()` the scene before the frame loop so its subgraph's GPU resources are built; updating `matrix` afterwards is cheap and needs no recompile because the matrix is consumed each frame during RecordTraversal (`include/vsg/nodes/MatrixTransform.h:21-22`; compile-before-loop: `include/vsg/app/Viewer.h:107`, `examples/app/vsghelloworld/vsghelloworld.cpp:74`).
- Leave `subgraphRequiresLocalFrustum` at its default `true` unless you know the subgraph contains no cull nodes; setting it `false` incorrectly can cull wrongly (`include/vsg/nodes/Transform.h:27-30`).
- Use `MatrixTransform` for relative placement; if you need the matrix to ignore inherited transforms entirely (absolute frame), use `AbsoluteTransform`, whose `transform()` returns `matrix` directly with no multiply (`include/vsg/nodes/AbsoluteTransform.h:21-22`, `:32`).

## Composition examples
```cpp
#include <vsg/all.h>

// Position, scale-about-centre, and rotate three copies of a loaded model.
// Distilled from examples/nodes/vsgtransform/vsgtransform.cpp:56-77
auto scene = vsg::Group::create();
vsg::dvec3 centre = (bounds.min + bounds.max) * 0.5;
double radius = vsg::length(bounds.max - bounds.min) * 0.6;
double scale = 0.5;

// simple translation
auto tm_1 = vsg::MatrixTransform::create();
tm_1->matrix = vsg::translate(-radius * (0.75 + scale * 0.5), 0.0, 0.0);
tm_1->addChild(model);
scene->addChild(tm_1);

// scale about a pivot point (translate to origin, scale, translate back)
auto tm_2 = vsg::MatrixTransform::create();
tm_2->matrix = vsg::translate(centre) * vsg::scale(scale, scale, scale) * vsg::translate(-centre);
tm_2->addChild(model);
scene->addChild(tm_2);

// rotate 90 degrees about X about the pivot
auto tm_3 = vsg::MatrixTransform::create();
tm_3->matrix = vsg::translate(centre) * vsg::rotate(vsg::radians(90.0), 1.0, 0.0, 0.0) * vsg::translate(-centre);
tm_3->addChild(model);
scene->addChild(tm_3);
```

```cpp
// Pass the matrix straight into the factory, then mutate it per-frame.
// Pattern from examples/nodes/vsgtextureprojection/vsgtextureprojection.cpp:489,522
auto transform = vsg::MatrixTransform::create(vsg::translate(0.0, 0.0, 100.0));
transform->addChild(model);
// later, e.g. inside the frame loop, animate it:
transform->matrix = vsg::translate(newLocation);   // cheap; consumed next RecordTraversal
```

## Source references
- `include/vsg/nodes/MatrixTransform.h` — `MatrixTransform` declaration, `matrix` member, `transform()` (pre-multiply).
- `include/vsg/nodes/Transform.h` — abstract `Transform` base, `subgraphRequiresLocalFrustum`, virtual `transform()`.
- `include/vsg/nodes/AbsoluteTransform.h` — sibling that replaces rather than multiplies the modelview.
- `include/vsg/maths/transform.h` — `translate` / `rotate` / `scale` matrix builders.
- `include/vsg/maths/common.h` — `radians` / `degrees` conversions.
- `examples/nodes/vsgtransform/vsgtransform.cpp` — translate/scale-about-pivot/rotate usage.
- `examples/nodes/vsgtextureprojection/vsgtextureprojection.cpp` — `create(matrix)` factory and per-frame `matrix` update.
- `examples/nodes/vsgcoordinateframe/vsgcoordinateframe.cpp` — chained `rotate * translate` matrices.

## Common mistakes
- Calling `new vsg::MatrixTransform` -> use `vsg::MatrixTransform::create(...)`, which returns a managed `ref_ptr` (`include/vsg/nodes/MatrixTransform.h:26`).
- Instantiating `vsg::Transform` directly -> it is abstract (pure-virtual `transform()`); use `MatrixTransform` or `AbsoluteTransform` (`include/vsg/nodes/Transform.h:34`).
- Passing degrees to `vsg::rotate` -> wrap with `vsg::radians(...)` (`include/vsg/maths/transform.h:45`, `include/vsg/maths/common.h:32`).
- Using `AbsoluteTransform` for relative placement (it discards inherited modelview) -> use `MatrixTransform` for relative, `AbsoluteTransform` only for absolute-frame anchoring (`include/vsg/nodes/AbsoluteTransform.h:32` vs `include/vsg/nodes/MatrixTransform.h:32`).
- Building the matrix from `float`/`vsg::mat4` for large-coordinate scenes -> the member is `vsg::dmat4`; use `double` literals and helpers to keep precision (`include/vsg/nodes/MatrixTransform.h:30`).
- Forgetting to `addChild()` the subgraph -> a transform with no children transforms nothing (`include/vsg/nodes/Group.h:39`).

## Things to never invent
- There is no `setMatrix()` / `getMatrix()` — `matrix` is a plain public member; assign it directly (`include/vsg/nodes/MatrixTransform.h:30`).
- There is no `MatrixTransform::create(const vsg::mat4&)` float overload — the constructor takes `const dmat4&` (`include/vsg/nodes/MatrixTransform.h:28`).
- There is no `setPosition()`, `setRotation()`, `setScale()`, `translation`, or `pivot` member on `MatrixTransform`/`Transform` — compose the matrix yourself with `translate`/`rotate`/`scale` (`include/vsg/nodes/MatrixTransform.h:25-42`, `include/vsg/nodes/Transform.h:23-43`).
- `transform()` is not a mutator/setter — it is a const method returning `mv * matrix` used by RecordTraversal; do not call it to "apply" a transform (`include/vsg/nodes/MatrixTransform.h:32`).
- There is no `absolute` bool flag on `MatrixTransform` to toggle absolute mode — use the separate `vsg::AbsoluteTransform` class (`include/vsg/nodes/AbsoluteTransform.h:23`).
