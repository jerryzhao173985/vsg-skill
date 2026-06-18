---
title: Animation (AnimationManager / Animation / FindAnimations)
description: Discover and play keyframe/morph/skeletal animations from a loaded model — FindAnimations collects them, viewer->animationManager->play(...) runs them, driven by viewer->update().
---

## Public include

```cpp
#include <vsg/all.h>   // pulls in everything below
// specific: <vsg/animation/AnimationManager.h>, <vsg/animation/Animation.h>, <vsg/animation/FindAnimations.h>
```

The `vsg::Viewer` owns the `AnimationManager` (`include/vsg/app/Viewer.h:91`); you do not create one.

## When to use

Use when a loaded model (glTF/etc. via vsgXchange) carries animations and you want to play them — keyframe transforms, morph targets, or skeletal (joint) animation. Animations are **discovered from the scene, not authored here**. For a static placement use `MatrixTransform`; for hand-coded per-frame motion mutate the matrix in the loop (see `matrixtransform.md`). Authoring animation data / writing samplers from scratch is out of scope.

## Key API

- `vsg::FindAnimations` — a `Visitor` that collects animations; run `scene->accept(findAnimations)`, then read `findAnimations.animations` and `findAnimations.animationGroups` (`include/vsg/animation/FindAnimations.h:20,23-24`; `examples/animation/vsganimation/vsganimation.cpp:415-419`).
- `viewer->animationManager` — the `vsg::AnimationManager` the Viewer creates and drives (`include/vsg/app/Viewer.h:91`).
- `animationManager->play(animation, startTime = 0.0)` — start an animation; pushes it onto the active `animations` list (`include/vsg/animation/AnimationManager.h:37`, `src/vsg/animation/AnimationManager.cpp:26-33`).
- `animationManager->stop(animation)` / `animationManager->stop()` — stop one / all (`include/vsg/animation/AnimationManager.h:40,43`).
- `animationManager->animations` — `std::list<ref_ptr<Animation>>` currently playing (`include/vsg/animation/AnimationManager.h:29`).
- `vsg::Animation` — public members `name`, `mode` (`ONCE`/`REPEAT`/`FORWARD_AND_BACK`, default `REPEAT`), `speed` (default `1.0`), `time`, `samplers` (`include/vsg/animation/Animation.h:49,51-59,65,62,69`).
- `vsg::AnimationSampler` — base for the samplers an `Animation` drives (`TransformSampler`/`MorphSampler`/`JointSampler`/`CameraSampler`) (`include/vsg/animation/Animation.h:23`).

## Best Practices

- **Discover, don't construct.** After `read_cast<Node>`, run `vsg::FindAnimations fa; scene->accept(fa);` — the loader built the `Animation`s + samplers; you just play them (`examples/animation/vsganimation/vsganimation.cpp:415-416`).
- **Play through `viewer->animationManager`**, never a hand-made manager — the Viewer's one is driven automatically: `Viewer::update()` calls `animationManager->run(frameStamp)` every frame (`src/vsg/app/Viewer.cpp:812`), so a played animation advances as long as your loop calls `viewer->update()`.
- Set `animation->mode` before/while playing: `REPEAT` loops, `ONCE` plays once then stops, `FORWARD_AND_BACK` ping-pongs (`include/vsg/animation/Animation.h:51-59`); scale time with `animation->speed`.
- **Skeletal animation needs no extra call** — the model's `Joint` hierarchy + `JointSampler` are part of the discovered `Animation`; playing it animates the joints. The model must be loaded by a loader that produces them (vsgXchange glTF; see `io.md`).

## Composition examples

Distilled from `examples/animation/vsganimation/vsganimation.cpp` (builds). Load a model, discover its animations, play them:

```cpp
#include <vsg/all.h>
// scene = vsg::read_cast<vsg::Node>(file, options); viewer set up — see model-viewer.md

vsg::FindAnimations findAnimations;
scene->accept(findAnimations);                          // collect animations + groups (vsganimation.cpp:415-416)

for (auto& animation : findAnimations.animations)
{
    animation->mode = vsg::Animation::REPEAT;           // loop (the default)
    viewer->animationManager->play(animation);          // Viewer drives it via update()
}
// frame loop: viewer->update() advances all playing animations (Viewer.cpp:812)
```

Animations grouped under an `AnimationGroup` (e.g. one per glTF clip): `for (auto& ag : findAnimations.animationGroups) viewer->animationManager->play(ag->animations.front());` (`vsganimation.cpp:419`).

## Source references

- `include/vsg/animation/AnimationManager.h:23,29,37,40,43,49` — `AnimationManager`: `animations`, `play`, `stop`, `run`.
- `include/vsg/animation/Animation.h:23,43,49,51-59,62,65,69` — `AnimationSampler`; `Animation` (name/mode/time/speed/samplers).
- `include/vsg/animation/FindAnimations.h:20,23-24` — `FindAnimations` visitor: `animations`, `animationGroups`.
- `include/vsg/app/Viewer.h:91` — `ref_ptr<AnimationManager> animationManager;` on the Viewer.
- `src/vsg/animation/AnimationManager.cpp:26-33,78` — `play()` starts + lists the animation; `run()` updates all active.
- `src/vsg/app/Viewer.cpp:32,812` — Viewer creates the manager; `update()` calls `animationManager->run(_frameStamp)`.
- `examples/animation/vsganimation/vsganimation.cpp:415-419` — discover via FindAnimations, play through `viewer->animationManager`.

## Common mistakes

- Building an `AnimationManager` by hand and calling `play` on it — use `viewer->animationManager`, the one `Viewer::update()` actually runs (`src/vsg/app/Viewer.cpp:812`); a detached manager never advances.
- Forgetting `viewer->update()` in the loop — animations only advance when `update()` runs `animationManager->run` (`src/vsg/app/Viewer.cpp:812`).
- Expecting to author animation here — samplers come from the loaded model via `FindAnimations`; this plays them, it does not create keyframes.
- Passing a name to `play()` — it takes a `ref_ptr<Animation>` (`include/vsg/animation/AnimationManager.h:37`); pick from `findAnimations.animations`.

## Things to never invent

- There is no `viewer->play(...)` or `Animation::play()` — playback is `viewer->animationManager->play(animation)` (`include/vsg/animation/AnimationManager.h:37`).
- `AnimationManager` has no `pause()` — only `play` / `stop(animation)` / `stop()` plus the internal `update`/`run` (`include/vsg/animation/AnimationManager.h:37-49`).
- `Animation::mode` is exactly `ONCE` / `REPEAT` / `FORWARD_AND_BACK` — no `LOOP`/`PINGPONG` spellings (`include/vsg/animation/Animation.h:51-56`).
- No `setSpeed()`/`setMode()` — `speed`/`mode` are public members assigned directly (`include/vsg/animation/Animation.h:59,65`).
