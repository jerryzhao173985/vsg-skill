---
title: Camera / LookAt / Perspective / Orthographic / EllipsoidPerspective / ViewportState
description: Bundles a projection matrix, a view matrix, and a viewport into the Camera object a View uses to decide what part of the scene to render and where.
---

## Public include
Use the barrel header (covers everything below):
```cpp
#include <vsg/all.h>   // pulls in app/Camera.h, app/ViewMatrix.h, app/ProjectionMatrix.h, state/ViewportState.h
```
Specific headers for reference: `include/vsg/app/Camera.h:26`, `include/vsg/app/ViewMatrix.h:22`, `include/vsg/app/ProjectionMatrix.h:23`, `include/vsg/state/ViewportState.h:23`. Namespace is `vsg::` for all symbols.

## When to use
Use `vsg::Camera` whenever you set up a `vsg::View` / command graph and need to define eye position, lens, and screen region (`include/vsg/app/Camera.h:23`). For the view matrix pick `vsg::LookAt` (gluLookAt eye/center/up, `include/vsg/app/ViewMatrix.h:53`) — not `vsg::LookDirection` (position+quaternion, `include/vsg/app/ViewMatrix.h:107`) unless you already have an orientation quaternion. For the projection, use `vsg::Perspective` for normal 3D (`include/vsg/app/ProjectionMatrix.h:49`), `vsg::Orthographic` for parallel/CAD/2D views (`include/vsg/app/ProjectionMatrix.h:101`), and `vsg::EllipsoidPerspective` for whole-earth scenes where near/far must auto-clamp to a planet ellipsoid (`include/vsg/app/ProjectionMatrix.h:189`).

## Key API
- `vsg::Camera::create(projectionMatrix, viewMatrix, viewportState = {})` — factory; viewportState defaults to empty `ref_ptr` (`include/vsg/app/Camera.h:31`).
- `Camera::projectionMatrix`, `Camera::viewMatrix`, `Camera::viewportState` — public `ref_ptr` members you can swap at runtime (`include/vsg/app/Camera.h:34`).
- `Camera::getViewport()` / `Camera::getRenderArea()` — convenience accessors that defer to the viewportState (`include/vsg/app/Camera.h:38`).
- `vsg::LookAt::create(eye, center, up)` — all three are `vsg::dvec3` (double precision); the ctor re-orthonormalizes `up` from look/side vectors (`include/vsg/app/ViewMatrix.h:71`).
- `LookAt::eye` / `LookAt::center` / `LookAt::up` — public `dvec3` members; defaults eye `(0,0,0)`, center `(0,1,0)`, up `(0,0,1)` (`include/vsg/app/ViewMatrix.h:56`).
- `vsg::Perspective::create(fov, aspectRatio, nearDistance, farDistance)` — `fov` is vertical field of view in degrees (`include/vsg/app/ProjectionMatrix.h:69`); defaults are fov `60.0`, aspect `1.0`, near `1.0`, far `10000.0` (`include/vsg/app/ProjectionMatrix.h:52`).
- `vsg::Orthographic::create(left, right, bottom, top, nearDistance, farDistance)` — glOrtho box (`include/vsg/app/ProjectionMatrix.h:125`); defaults `-1,1,-1,1,1,10000` (`include/vsg/app/ProjectionMatrix.h:104`).
- `vsg::EllipsoidPerspective::create(lookAt, ellipsoidModel, fov, aspectRatio, nearFarRatio, horizonMountainHeight)` — holds a `ref_ptr<LookAt>` and `ref_ptr<EllipsoidModel>`; defaults fov `60.0`, aspect `1.0`, nearFarRatio `0.0001`, horizonMountainHeight `1000.0` (`include/vsg/app/ProjectionMatrix.h:200`, `include/vsg/app/ProjectionMatrix.h:246`).
- `vsg::ViewportState::create(extent2D)` — single Viewport+Scissor pair at origin filling the given `VkExtent2D` (`include/vsg/state/ViewportState.h:30`); also `ViewportState::create(x, y, width, height)` (`include/vsg/state/ViewportState.h:33`).
- `ProjectionMatrix::changeExtent(prevExtent, newExtent)` — adjusts aspect ratio on window resize; overridden by each projection subclass (`include/vsg/app/ProjectionMatrix.h:42`).

## Best Practices
- Always create via `T::create(...)`, never `new` — every type here derives from `vsg::Object` through `vsg::Inherit` and is managed by `vsg::ref_ptr` (`include/vsg/app/Camera.h:26`, `include/vsg/app/ViewMatrix.h:53`). Cleanup is automatic when the last `ref_ptr` drops; vsghelloworld relies on this (`examples/app/vsghelloworld/vsghelloworld.cpp:89`).
- View and projection math is double precision: `LookAt` stores `dvec3` and `transform()` returns `dmat4` (`include/vsg/app/ViewMatrix.h:95`, `include/vsg/app/ProjectionMatrix.h:35`). Build camera positions from scene bounds in `double`, and pass `vsg::dvec3` literals (e.g. `vsg::dvec3(0.0, 0.0, 1.0)`), not float (`examples/app/vsghelloworld/vsghelloworld.cpp:42`).
- Derive `aspectRatio` (or ortho width/height) from the actual window extent so the image is not stretched: `window->extent2D().width / height` cast to `double` (`examples/app/vsghelloworld/vsghelloworld.cpp:58`, `examples/app/vsgortho/vsgortho.cpp:68`).
- Size the camera using the scene: run `vsg::ComputeBounds`, take the centre and a radius, then set near/far as ratios of that radius (`examples/app/vsghelloworld/vsghelloworld.cpp:40`, `examples/app/vsghelloworld/vsghelloworld.cpp:58`).
- For orthographic, scale the box by aspect ratio along the longer screen axis so geometry keeps its proportions (`examples/app/vsgortho/vsgortho.cpp:71`).
- Use `EllipsoidPerspective` only when the scene carries an `EllipsoidModel` (e.g. obtained via `getRefObject<vsg::EllipsoidModel>("EllipsoidModel")`); otherwise fall back to `Perspective` (`examples/app/vsghelloworld/vsghelloworld.cpp:50`).
- Compile the viewer before the frame loop: call `viewer->compile()` after assigning the command graph that uses the camera, and only then enter the `advanceToNextFrame()` loop (`examples/app/vsghelloworld/vsghelloworld.cpp:74`).
- Hand the camera to a `vsg::Trackball` event handler if you want interactive control; the trackball mutates the camera's `viewMatrix` between frames (`examples/app/vsghelloworld/vsghelloworld.cpp:67`).
- The `viewportState` argument is optional and defaults to empty (`include/vsg/app/Camera.h:31`); supply one for normal window rendering, otherwise `getViewport()`/`getRenderArea()` return empty Vulkan structs (`include/vsg/app/Camera.h:38`).

## Composition examples
Perspective camera sized from scene bounds (distilled from vsghelloworld):
```cpp
#include <vsg/all.h>

// compute scene extents to position the camera
vsg::ComputeBounds computeBounds;
vsg_scene->accept(computeBounds);
vsg::dvec3 centre = (computeBounds.bounds.min + computeBounds.bounds.max) * 0.5; // double precision
double radius = vsg::length(computeBounds.bounds.max - computeBounds.bounds.min) * 0.6;
double nearFarRatio = 0.001;

// view matrix: eye behind/below centre, looking at centre, Z up
auto lookAt = vsg::LookAt::create(centre + vsg::dvec3(0.0, -radius * 3.5, 0.0),
                                  centre, vsg::dvec3(0.0, 0.0, 1.0));

// projection: vertical fov 30deg, aspect from the window, near/far scaled by radius
double aspect = static_cast<double>(window->extent2D().width) /
                static_cast<double>(window->extent2D().height);
auto perspective = vsg::Perspective::create(30.0, aspect, nearFarRatio * radius, radius * 4.5);

// viewport fills the whole window
auto camera = vsg::Camera::create(perspective, lookAt,
                                  vsg::ViewportState::create(window->extent2D()));
```
(based on `examples/app/vsghelloworld/vsghelloworld.cpp:40`-`62`)

Orthographic camera (distilled from vsgortho):
```cpp
#include <vsg/all.h>

double aspectRatio = static_cast<double>(window->extent2D().width) /
                     static_cast<double>(window->extent2D().height);
double halfDim = radius * 1.1;
double halfHeight, halfWidth;
if (window->extent2D().width > window->extent2D().height) { halfHeight = halfDim; halfWidth = halfDim * aspectRatio; }
else                                                       { halfWidth = halfDim; halfHeight = halfDim / aspectRatio; }

// left, right, bottom, top, near, far  -- a parallel-projection box
auto projection = vsg::Orthographic::create(-halfWidth, halfWidth,
                                            -halfHeight, halfHeight,
                                            nearFarRatio * radius, radius * 4.5);
auto camera = vsg::Camera::create(projection, lookAt,
                                  vsg::ViewportState::create(window->extent2D()));
```
(based on `examples/app/vsgortho/vsgortho.cpp:68`-`85`)

## Source references
- `include/vsg/app/Camera.h` — `Camera` declaration, ctor, members, viewport accessors
- `include/vsg/app/ViewMatrix.h` — `ViewMatrix` base, `LookAt`, `LookDirection`, relative/tracking variants
- `include/vsg/app/ProjectionMatrix.h` — `ProjectionMatrix` base, `Perspective`, `Orthographic`, `EllipsoidPerspective`
- `include/vsg/state/ViewportState.h` — `ViewportState` ctors and accessors
- `examples/app/vsghelloworld/vsghelloworld.cpp` — perspective / ellipsoid camera setup from scene bounds
- `examples/app/vsgortho/vsgortho.cpp` — orthographic projection setup

## Common mistakes
- Calling `new vsg::Camera(...)` -> use `vsg::Camera::create(...)` so ref-counting and RTTI are wired (`include/vsg/app/Camera.h:31`).
- Passing constructor arguments in the wrong order -> the order is `(projectionMatrix, viewMatrix, viewportState)`, projection first (`include/vsg/app/Camera.h:31`).
- Passing `fov` in radians to `Perspective` -> `fieldOfViewY` is in degrees; `transform()` converts via `radians(...)` internally (`include/vsg/app/ProjectionMatrix.h:79`).
- Hard-coding `aspectRatio = 1.0` -> derive it from `window->extent2D()` or the image is stretched (`examples/app/vsghelloworld/vsghelloworld.cpp:58`).
- Using `vec3`/float for eye/center/up -> use `vsg::dvec3`; the API is double precision throughout (`include/vsg/app/ViewMatrix.h:100`).
- Forgetting the viewport -> without a `ViewportState`, `getViewport()`/`getRenderArea()` return empty `VkViewport{}` / `VkRect2D{}` (`include/vsg/app/Camera.h:38`).
- Entering the frame loop before `viewer->compile()` -> compile after assigning the command graph that holds the camera (`examples/app/vsghelloworld/vsghelloworld.cpp:74`).

## Things to never invent
- There is no `setProjectionMatrix()` / `setViewMatrix()` / `setViewport()` on `Camera`; assign the public members `projectionMatrix`, `viewMatrix`, `viewportState` directly (`include/vsg/app/Camera.h:34`).
- `LookAt` has no `lookAt()` / `setEye()` / `setLookAt()` methods; set `eye`, `center`, `up` members or call `set(const dmat4&)` (`include/vsg/app/ViewMatrix.h:93`, `include/vsg/app/ViewMatrix.h:100`).
- `Perspective` has no `fieldOfViewX`, `zNear`, or `zFar`; the members are `fieldOfViewY`, `aspectRatio`, `nearDistance`, `farDistance` (`include/vsg/app/ProjectionMatrix.h:89`).
- `Orthographic` does not take `(width, height)`; it takes the six planes `left, right, bottom, top, nearDistance, farDistance` (`include/vsg/app/ProjectionMatrix.h:125`).
- `EllipsoidPerspective` does not take an explicit `farDistance`; it auto-computes near/far from `nearFarRatio` and the `EllipsoidModel` (`include/vsg/app/ProjectionMatrix.h:200`, `include/vsg/app/ProjectionMatrix.h:230`).
- `ViewportState::create` has no overload taking a `VkViewport`; use a `VkExtent2D` or the `(x, y, width, height)` form (`include/vsg/state/ViewportState.h:30`).
