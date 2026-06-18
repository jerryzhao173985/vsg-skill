---
title: Text (Text / Font / StandardLayout)
description: Render high-quality signed-distance-field text labels — load a Font, lay it out with StandardLayout (position/alignment/color), set the string on a Text node, call setup(), add it to the scene.
---

## Public include

```cpp
#include <vsg/all.h>   // or <vsg/text/Text.h>, <vsg/text/Font.h>, <vsg/text/StandardLayout.h>
```

`vsg::Text` is a `vsg::Node` (`include/vsg/text/Text.h:29`), so it goes anywhere in the scene graph.

## When to use

Use `vsg::Text` for labels / HUD / annotations rendered as 3D geometry — signed-distance-field glyphs that stay crisp at any scale (`include/vsg/text/Text.h:24`). It does **not** provide frustum culling or LOD; decorate with a `CullNode`/`LOD` if you need it (`include/vsg/text/Text.h:25-28`). For many labels sharing one font, batch with `TextGroup`. Authoring fonts is out of scope.

## Key API

- `vsg::read_cast<vsg::Font>(font_path, options)` — load a font, e.g. `"fonts/times.vsgb"` (`examples/text/vsgtext/vsgtext.cpp:196`). Share one `Font` across all labels.
- `vsg::Text::create()` — then assign members and call `setup()` (`include/vsg/text/Text.h:29,54`):
  - `text->font` — `ref_ptr<Font>` (`include/vsg/text/Text.h:46`).
  - `text->layout` — `ref_ptr<TextLayout>`; use `StandardLayout` (`include/vsg/text/Text.h:49`).
  - `text->text` — `ref_ptr<Data>` (the string), e.g. `vsg::stringValue::create("hello")` (`include/vsg/text/Text.h:50`, `examples/text/vsgtext/vsgtext.cpp:283`).
  - `text->technique` — `ref_ptr<TextTechnique>`; `setup()` defaults it to `CpuLayoutTechnique`, use `GpuLayoutTechnique` for text updated each frame (`include/vsg/text/Text.h:48`, `src/vsg/text/Text.cpp:64`).
  - `text->setup(minimumAllocation = 0, options)` — builds the render backend; call **after** the members are set (`include/vsg/text/Text.h:54`).
- `vsg::StandardLayout` — `position` (vec3), `horizontalAlignment`/`verticalAlignment` (`BASELINE`/`LEFT`/`CENTER`/`RIGHT`/…), `color`/`outlineColor`/`outlineWidth`, `billboard` (face camera) (`include/vsg/text/StandardLayout.h:44-54`).

## Best Practices

- **Order matters:** set `font` + `layout` + `text`, THEN call `text->setup(0, options)` — `setup()` reads those members to build the glyph geometry (`examples/text/vsgtext/vsgtext.cpp:262-266`, `src/vsg/text/Text.cpp:61`).
- Share one `Font` (load it once, reuse) across every label (`examples/text/vsgtext/vsgtext.cpp:196`).
- Use the default `CpuLayoutTechnique` for static labels; switch `text->technique = vsg::GpuLayoutTechnique::create()` for text whose string/position changes per frame, so updates are cheap (`src/vsg/text/Text.cpp:64`, `include/vsg/text/Text.h:48`).
- `layout->position` is the anchor; `horizontalAlignment`/`verticalAlignment` place the string around it; `layout->billboard = true` keeps a label facing the camera (`include/vsg/text/StandardLayout.h:47,44-45,53`).
- A `Text` is a `Node` — add it under a `Group`/`MatrixTransform` to place it; prefer `TextGroup` for many labels (`include/vsg/text/Text.h:29`).

## Composition examples

Distilled from `examples/text/vsgtext/vsgtext.cpp` (builds). A single label:

```cpp
#include <vsg/all.h>
auto options = vsg::Options::create();                         // + vsgXchange for non-native assets
auto font = vsg::read_cast<vsg::Font>("fonts/times.vsgb", options);   // :196 — load once, reuse

auto layout = vsg::StandardLayout::create();                   // :224
layout->position = vsg::vec3(0.0f, 0.0f, 0.0f);                // :225 — anchor point
layout->horizontalAlignment = vsg::StandardLayout::CENTER_ALIGNMENT;  // :274
layout->color = vsg::vec4(1.0f, 1.0f, 1.0f, 1.0f);             // :228

auto text = vsg::Text::create();                               // :262
text->font = font;                                             // :263
text->layout = layout;                                         // :264
text->text = vsg::stringValue::create("VulkanSceneGraph");     // :283 — string is a Data, not std::string
text->setup(0, options);                                       // :266 — build the geometry (call last)

scene->addChild(text);                                         // a Text is a Node
```

For text updated each frame (counters, coordinates): set `text->technique = vsg::GpuLayoutTechnique::create();` before `setup()`, then reassign `text->text` and call `setup()` again.

## Source references

- `include/vsg/text/Text.h:29,46-50,54` — `Text` node; `font`/`shaderSet`/`technique`/`layout`/`text` members; `setup()`.
- `include/vsg/text/StandardLayout.h:21,27-35,44-54` — `StandardLayout`; `Alignment` enum; position/alignment/color/outline/billboard members.
- `include/vsg/text/Font.h` — `Font` (loaded via `read_cast<Font>`).
- `src/vsg/text/Text.cpp:61-64` — `setup()` defaults `technique` to `CpuLayoutTechnique` when unset.
- `examples/text/vsgtext/vsgtext.cpp:196,224-228,262-266,283` — font load, layout, Text setup, dynamic `stringValue`.

## Common mistakes

- Skipping `text->setup()` — without it the `Text` has no render backend and draws nothing (`include/vsg/text/Text.h:52-54`).
- Changing members AFTER `setup()` and expecting an update — `setup()` builds geometry from the current members; call it again after changing them (`src/vsg/text/Text.cpp:61`).
- Assigning a `std::string` to `text->text` — it is a `ref_ptr<Data>`; wrap with `vsg::stringValue::create("...")` (`include/vsg/text/Text.h:50`, `examples/text/vsgtext/vsgtext.cpp:283`).
- Using `CpuLayoutTechnique` (the default) for per-frame text — prefer `GpuLayoutTechnique` (`include/vsg/text/Text.h:48`).

## Things to never invent

- No `Text::setText(...)` / `setFont(...)` — `text`/`font`/`layout`/`technique` are public members assigned directly (`include/vsg/text/Text.h:46-50`).
- `text->text` is `ref_ptr<Data>`, not `std::string` — use `vsg::stringValue::create` / `wstringValue` (`include/vsg/text/Text.h:50`).
- `StandardLayout::Alignment` is `BASELINE`/`LEFT`/`TOP`(=LEFT)/`CENTER`/`RIGHT`/`BOTTOM`(=RIGHT) — no `JUSTIFY`/`MIDDLE` (`include/vsg/text/StandardLayout.h:27-35`).
- Do not expect `Text` to cull/LOD itself — decorate with `CullNode`/`LOD` (`include/vsg/text/Text.h:25-28`).
