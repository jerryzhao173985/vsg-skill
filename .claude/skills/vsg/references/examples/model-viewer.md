---
title: Model viewer (vsghelloworld)
description: The canonical interactive VSG app — load a model, frame it, orbit with a trackball, render.
---

## What to copy
- The whole wire→compile→loop skeleton: `Options`+`read_cast` → `Viewer`+`Window` → bounds-fit `Camera` → `CloseHandler`+`Trackball` → `createCommandGraphForView` → `compile()` → frame loop.
- The bounds-fit camera trick: `vsg::ComputeBounds` over the scene to place the `LookAt` and size the near/far planes.
- The null checks on `read_cast` and `Window::create`.

## The program
`examples/app/vsghelloworld/vsghelloworld.cpp` — the smallest complete VSG viewer that loads a file and displays it. Built only when `vsgXchange_FOUND` (it reads non-native formats).

## Key lines
- Options + loaders: `examples/app/vsghelloworld/vsghelloworld.cpp:14`
- Load the scene (nullable): `examples/app/vsghelloworld/vsghelloworld.cpp:25`
- Viewer + Window (nullable): `examples/app/vsghelloworld/vsghelloworld.cpp:29`, `:30`
- Bounds-fit camera: `examples/app/vsghelloworld/vsghelloworld.cpp:40-61`
- Close + trackball handlers: `examples/app/vsghelloworld/vsghelloworld.cpp:64`, `:67`
- Command graph + assign: `examples/app/vsghelloworld/vsghelloworld.cpp:70`, `:71`
- `compile()` before the loop: `examples/app/vsghelloworld/vsghelloworld.cpp:74`
- Canonical frame loop: `examples/app/vsghelloworld/vsghelloworld.cpp:77-87`

## Related
- Full distilled recipe with code: `references/patterns.md` (Recipe 1).
- API detail: `references/components/io.md`, `viewer.md`, `camera.md`, `trackball.md`, `commandgraph.md`.
