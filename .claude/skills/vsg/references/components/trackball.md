---
title: Trackball / CloseHandler (event handlers)
description: Visitor-based event handlers attached to a Viewer; Trackball is a mouse/touch/keyboard camera manipulator, CloseHandler quits on window-close or Escape.
---

## Public include
```cpp
#include <vsg/all.h>            // barrel include (preferred)
// or specifically:
#include <vsg/app/Trackball.h>     // include/vsg/app/Trackball.h:27
#include <vsg/app/CloseHandler.h>  // include/vsg/app/CloseHandler.h:22
```
Namespace is `vsg::`. Both classes live under `vsg/app/`.

## When to use
Use `vsg::Trackball` as the default camera manipulator that maps mouse drag / scroll / touch / keys onto a `vsg::Camera`'s `vsg::LookAt` (`include/vsg/app/Trackball.h:26`). Use `vsg::CloseHandler` to close the viewer on the window-close button or Escape (`include/vsg/app/CloseHandler.h:21`). Both are event handlers — i.e. `vsg::Visitor` subclasses — registered on the viewer via `viewer->addEventHandler(...)`. For a turntable-style manipulator instead of a free trackball, write a custom handler (see `examples/app/vsgturntablecamera/Turntable.h`); Trackball is not a turntable.

## Key API
Trackball:
- `vsg::Trackball::create(camera)` / `create(camera, ellipsoidModel)` — factory; `ref_ptr<EllipsoidModel>` arg defaults to `{}` (`include/vsg/app/Trackball.h:30`).
- `addKeyViewpoint(KeySymbol key, ref_ptr<LookAt> lookAt, double duration = 1.0)` — bind a key to animate to a `LookAt` (`include/vsg/app/Trackball.h:68`).
- `addKeyViewpoint(KeySymbol key, double latitude, double longitude, double altitude, double duration = 1.0)` — geographic viewpoint; requires an `EllipsoidModel` was passed at construction (`include/vsg/app/Trackball.h:70-71`).
- `setViewpoint(ref_ptr<LookAt> lookAt, double duration = 1.0)` — animate to a `LookAt`; `0.0` snaps instantly (`include/vsg/app/Trackball.h:73-75`).
- `addWindow(ref_ptr<Window> window, const ivec2& offset = {})` — also respond to events from another window (`include/vsg/app/Trackball.h:65`).
- `rotate(double, const dvec3&)`, `zoom(double)`, `pan(const dvec2&)` — virtual manipulation hooks, overridable (`include/vsg/app/Trackball.h:51-53`).
- Button masks: `rotateButtonMask = BUTTON_MASK_1`, `panButtonMask = BUTTON_MASK_2`, `zoomButtonMask = BUTTON_MASK_3` (`include/vsg/app/Trackball.h:123,126,129`).
- Key bindings (public members, reassignable): `turnLeftKey=KEY_a`, `turnRightKey=KEY_d`, `pitchUpKey=KEY_w`, `pitchDownKey=KEY_s`, `rollLeftKey=KEY_q`, `rollRightKey=KEY_e`, `moveForwardKey=KEY_o`, `moveBackwardKey=KEY_i`, `moveLeftKey=KEY_Left`, `moveRightKey=KEY_Right`, `moveUpKey=KEY_Up`, `moveDownKey=KEY_Down` (`include/vsg/app/Trackball.h:87-120`).
- `zoomScale = 1.0` — zoom sensitivity (`include/vsg/app/Trackball.h:135`); `supportsThrow = true` — continue moving after release while in motion (`include/vsg/app/Trackball.h:138`).

CloseHandler:
- `vsg::CloseHandler::create(viewer)` — factory taking a `Viewer*` (`include/vsg/app/CloseHandler.h:25`).
- `closeKey = KEY_Escape` — key that triggers close (`include/vsg/app/CloseHandler.h:28`).
- `close()` — virtual; calls `viewer->close()` (`include/vsg/app/CloseHandler.h:30-35`).

Viewer attachment:
- `viewer->addEventHandler(ref_ptr<Visitor>)` — appends one handler (`include/vsg/app/Viewer.h:71`).
- `viewer->addEventHandlers(const EventHandlers&)` — appends many; `EventHandlers` is `std::list<vsg::ref_ptr<vsg::Visitor>>` (`include/vsg/app/Viewer.h:73`, `include/vsg/ui/UIEvent.h:44`).

## Best Practices
- Handlers are `vsg::Visitor` subclasses (`Inherit<Visitor, Trackball>` at `include/vsg/app/Trackball.h:27`; `Inherit<Visitor, CloseHandler>` at `include/vsg/app/CloseHandler.h:22`). Always create with `::create(...)` returning `ref_ptr`, never `new` — `addEventHandler` stores a `ref_ptr<Visitor>` so the viewer owns the handler (`include/vsg/app/Viewer.h:71,163`).
- Register handlers before the frame loop; they only fire when `viewer->handleEvents()` is pumped each frame (`examples/app/vsghelloworld/vsghelloworld.cpp:80`). Trackball reacts to a `FrameEvent` too, so its animation/throw advances per frame (`include/vsg/app/Trackball.h:49`).
- Pass the same `vsg::Camera` to the Trackball that you used to build the `CommandGraph`/`View`; the Trackball mutates that camera's `LookAt` in place. Construct the camera (and its `LookAt`) first, then the Trackball (`examples/app/vsghelloworld/vsghelloworld.cpp:61,67`).
- For geo/whole-earth scenes pass the scene's `EllipsoidModel` so panning/zoom respect the globe and geographic `addKeyViewpoint(key, lat, lon, alt)` works; without it that overload has no ellipsoid and `clampToGlobe()` is inert (`include/vsg/app/Trackball.h:30,59,70-71`, `examples/nodes/vsgtiledatabase/vsgtiledatabase.cpp:223-232`).
- Order CloseHandler relative to other handlers as you like, but a common idiom is to add `CloseHandler` first so Escape/close is always honored (`examples/app/vsghelloworld/vsghelloworld.cpp:64-67`).
- Trackball math is double precision (`dvec2`/`dvec3`/`LookAt`), matching VSG's double-precision camera path — do not down-convert to float (`include/vsg/app/Trackball.h:33,36,51-53`).
- To customize keys/buttons, assign the public members after `create` (e.g. `trackball->zoomButtonMask = vsg::BUTTON_MASK_2;`) before the frame loop; they are plain public fields (`include/vsg/app/Trackball.h:87-138`).

## Composition examples
```cpp
#include <vsg/all.h>

// camera already built from a LookAt + ProjectionMatrix (see vsghelloworld.cpp:47-61)
auto viewer = vsg::Viewer::create();
// ... viewer->addWindow(window); build `camera` and `ellipsoidModel` ...

// close on window-close button and Escape
viewer->addEventHandler(vsg::CloseHandler::create(viewer));        // vsghelloworld.cpp:64

// trackball manipulator drives `camera`'s LookAt; ellipsoidModel may be {}.
viewer->addEventHandler(vsg::Trackball::create(camera, ellipsoidModel)); // vsghelloworld.cpp:67

// ... assignRecordAndSubmitTaskAndPresentation, then compile, THEN loop:
viewer->compile();
while (viewer->advanceToNextFrame())
{
    viewer->handleEvents();   // dispatches events to Trackball + CloseHandler  // vsghelloworld.cpp:80
    viewer->update();
    viewer->recordAndSubmit();
    viewer->present();
}
```

```cpp
// Geographic key-bound viewpoints (whole-earth scene); needs an EllipsoidModel.
auto trackball = vsg::Trackball::create(camera, ellipsoidModel);
trackball->addKeyViewpoint(vsg::KeySymbol('1'), 51.50151, -0.14181, 2000.0, 2.0); // animate to London over 2s
trackball->addKeyViewpoint(vsg::KeySymbol('2'), 48.85826,  2.29450, 2000.0, 2.0); // Paris
viewer->addEventHandler(trackball);   // examples/nodes/vsgtiledatabase/vsgtiledatabase.cpp:223-241
```

## Source references
- `include/vsg/app/Trackball.h` — `Trackball` declaration, key/button defaults, viewpoint API.
- `include/vsg/app/CloseHandler.h` — `CloseHandler` declaration (header-only `apply`/`close`).
- `include/vsg/app/Viewer.h` — `addEventHandler` / `addEventHandlers` / `getEventHandlers`.
- `include/vsg/ui/UIEvent.h` — `EventHandlers` typedef.
- `examples/app/vsghelloworld/vsghelloworld.cpp` — minimal CloseHandler + Trackball wiring and frame loop.
- `examples/nodes/vsgtiledatabase/vsgtiledatabase.cpp` — Trackball with `EllipsoidModel` and `addKeyViewpoint`.

## Common mistakes
- Constructing via `new vsg::Trackball(...)` -> use `vsg::Trackball::create(...)`, which returns the `ref_ptr` that `addEventHandler` expects (`include/vsg/app/Trackball.h:30`, `include/vsg/app/Viewer.h:71`).
- Adding handlers but never calling `viewer->handleEvents()` in the loop -> events never dispatch, so the trackball appears dead (`examples/app/vsghelloworld/vsghelloworld.cpp:80`).
- Passing a different camera to the Trackball than the one in the render graph -> the view never moves; pass the same `vsg::Camera` (`examples/app/vsghelloworld/vsghelloworld.cpp:61,67`).
- Using the lat/lon/alt `addKeyViewpoint` overload without supplying an `EllipsoidModel` to `create` -> no geographic frame to resolve against (`include/vsg/app/Trackball.h:70-71`).
- Calling `vsg::CloseHandler::create(window)` -> it takes the `Viewer`, not a `Window` (`include/vsg/app/CloseHandler.h:25`).

## Things to never invent
- There is no `setCamera`/`getCamera` accessor on `Trackball`; the camera is a protected `_camera` set only at construction (`include/vsg/app/Trackball.h:141`).
- There is no `enable`/`disable`/`setEnabled` toggle on Trackball — to limit scope use `windowOffsets`/`addWindow` (`include/vsg/app/Trackball.h:62,65`).
- Button/key bindings are plain public members, not setters — there is no `setRotateButtonMask`/`setTurnLeftKey` (`include/vsg/app/Trackball.h:87-132`).
- `Viewer` exposes `addEventHandler`/`addEventHandlers`, not `setEventHandler` or `removeEventHandler` (`include/vsg/app/Viewer.h:71-79`).
- `CloseHandler` has no `closeButton` or mouse mapping; it responds to `closeKey`, `CloseWindowEvent`, and `TerminateEvent` only (`include/vsg/app/CloseHandler.h:28,37-50`).
