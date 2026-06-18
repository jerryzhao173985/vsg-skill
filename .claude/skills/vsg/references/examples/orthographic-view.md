---
title: Orthographic view (vsgortho)
description: Same viewer skeleton as the model viewer, but with an orthographic projection instead of perspective.
---

## What to copy
- Swap `vsg::Perspective` for `vsg::Orthographic::create(left, right, bottom, top, near, far)` — everything else in the viewer skeleton is identical.
- How the ortho extents are derived from the window aspect ratio so the scene is not stretched.

## The program
`examples/app/vsgortho/vsgortho.cpp` — a 111-line viewer that renders with an orthographic camera (no perspective foreshortening), useful for 2D overlays, CAD-style views, and isometric scenes.

## Key lines
- `LookAt` (same as perspective): `examples/app/vsgortho/vsgortho.cpp:66`
- `Orthographic::create(...)` projection: `examples/app/vsgortho/vsgortho.cpp:81`
- `Camera::create(projection, lookAt, viewport)`: `examples/app/vsgortho/vsgortho.cpp:85`
- Command graph + assign + compile: `examples/app/vsgortho/vsgortho.cpp:91`, `:92`, `:94`
- Frame loop: `examples/app/vsgortho/vsgortho.cpp:97`

## Snippet (the only real difference from a perspective viewer)

```cpp
// derive symmetric ortho extents from the window aspect ratio, then:
auto projection = vsg::Orthographic::create(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
auto camera = vsg::Camera::create(projection, lookAt, vsg::ViewportState::create(window->extent2D()));
// (examples/app/vsgortho/vsgortho.cpp:81-85)
```

## Related
- API detail: `references/components/camera.md` (Perspective vs Orthographic vs EllipsoidPerspective).
