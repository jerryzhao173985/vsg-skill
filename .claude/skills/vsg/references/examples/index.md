# Examples

Annotated walkthroughs of the highest-leverage real programs in the [vsgExamples](https://github.com/vsg-dev/vsgExamples) repo. Each file names what to copy and cites the exact lines. For self-contained distilled recipes, see `references/patterns.md`; for per-class API, see `references/components/`.

- [model-viewer](./model-viewer.md) — The whole wire→compile→loop skeleton: load a model, frame it, orbit with a trackball, render (`examples/app/vsghelloworld`).
- [orthographic-view](./orthographic-view.md) — Swap `vsg::Perspective` for `vsg::Orthographic` — everything else in the viewer skeleton is identical (`examples/app/vsgortho`).
- [procedural-geometry](./procedural-geometry.md) — Generate primitive shapes at runtime with `Builder` instead of loading model files (`examples/utils/vsgbuilder`).
- [headless-rendering](./headless-rendering.md) — Render with no window: `Device`-from-`Instance`, offscreen `Framebuffer`, capture-to-file (`examples/app/vsgheadless`). ⚠ crashes on macOS/MoltenVK — see its Platform note.

## More example programs worth reading (in the vsgExamples tree)

These are not walked through here, but are the canonical references for their topic — read the real source under `examples/<group>/`:

- `examples/app/vsgviewer/` — fuller viewer with multi-threaded recording (`setupThreading`).
- `examples/app/vsgheadless/` — offscreen/headless rendering, frames driven manually (no window events).
- `examples/app/vsgwindows/` — multiple windows, one `CommandGraph` per window.
- `examples/nodes/vsgtransform/` — `MatrixTransform` scene assembly (translate/rotate/scale).
- `examples/state/` — building `GraphicsPipeline` + `StateGroup` from shaders.
- `examples/io/` — `read`/`write`/`Options` and custom `ReaderWriter`s.
