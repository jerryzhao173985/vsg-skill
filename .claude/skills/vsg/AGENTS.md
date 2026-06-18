# AGENTS — vsg

Cross-agent guidance for the `vsg` skill. Any agent touching this skill reads `SKILL.md` first, then loads the per-domain files in `references/` named by the routing table. This file explains *how* the skill is shaped and *how to keep it honest*.

## What this skill is (and how it differs from a web DS skill)

This skill was produced by the `extract-ds-skill` meta-skill, which is built for web/UI design systems (React components, CSS tokens, icon assets). VulkanSceneGraph is a **C++17 Vulkan scene-graph library**, so the extraction was adapted:

- **In scope (and present):** component descriptions and component APIs (`references/components/`), architectural mental-models (`references/foundations/`), worked example programs (`references/examples/`), cross-cutting recipes (`references/patterns.md`), and a pitfall registry (`references/anti-patterns.md`).
- **`references/foundations/` is repurposed for C++.** In a web-DS skill, foundations are token-usage rules extracted from docs prose. Here they are four VSG **architecture mental-models** (object-model, scene-graph, application-and-rendering, io-and-data) whose behavioural claims are grounded in the `src/vsg/` implementation, not just header doc-comments — so an agent understands *why* the APIs are shaped as they are.
- **Not applicable (and intentionally absent):** `tokens.md`, `assets.md`, `design-craft.md`. VSG has no design tokens, no icon/font assets, and no web-UI craft. Their absence is correct, not a gap.
- **Validation was a two-layer C++ probe**, not a TypeScript typecheck: (1) a `-fsyntax-only` parse of a `probe.cpp` that `#include <vsg/all.h>` and exercises every claimed API against the repo headers + Vulkan SDK, and (2) a `find_package(vsg)` CMake target that **compiles and links** the same surface against the installed `libvsg.dylib` (1.1.14, == the cited repo tag). Both passed — the C++ equivalent of "props verified against source, 0 hallucinations," and the link layer additionally proves every non-inline signature resolves in the binary.

## Source model and citation convention

Two repositories are the source of truth:

- **VulkanSceneGraph** (`@ v1.1.14`) — the library. Headers under `include/vsg/` are **authoritative** for every class, method, member, and default. Implementation under `src/vsg/` is consulted only to confirm runtime behaviour.
- **vsgExamples** (`@ master`) — real consumer apps under `examples/`. Authoritative for idiomatic composition and call order.

Citations are **repo-relative paths**, which self-disambiguate because the repos' top-level dirs don't overlap:

- `include/vsg/<...>.h:<line>` → resolves against the VulkanSceneGraph repo.
- `examples/<...>.cpp:<line>` → resolves against the vsgExamples repo.

Code wins over prose on conflict. An uncited API claim is a defect; a claim that genuinely cannot be grounded carries a literal `[VERIFY]` marker.

**Read headers and examples with the `Read` tool, not `cat`.** A `file:line` citation needs line numbers — `Read` returns them, a bare `cat` does not, and a `cat` of a whole file is an un-citeable full read. Use `grep`/`sed`/`head` to *search* a file (fine); but to read a header or example for grounding, open it with `Read` so every claim stays precisely citeable.

## Per-component file contract (8 sections)

Every `references/components/<name>.md` ships these 8 H2 sections, in order. The headings are the load contract — agents key off the exact strings:

1. **Public include** — the `#include` line(s) (prefer the barrel `#include <vsg/all.h>`) + namespace.
2. **When to use** — when to reach for this class vs its real siblings (`Group` vs `StateGroup` vs `Switch`; `MatrixTransform` vs `AbsoluteTransform`).
3. **Key API** — the factory + members/methods an agent actually reaches for, each one line + a `path:line` cite.
4. **Best practices** — the VSG correctness rules. This is where the web-DS "Accessibility" section is re-mapped onto VSG's real cross-cutting concern: **lifecycle, ownership, and threading** (`ref_ptr` ownership, `compile()`-before-loop, record-thread constraints, double-vs-float precision). Flat list under 10 rules / 3 axes, else subsectioned.
5. **Composition examples** — short, real, copy-pasteable C++ distilled from a cited example file; the annotated code *is* the pattern.
6. **Source references** — the headers + example files the file draws from.
7. **Common mistakes** — wrong-path → right-path, one line each.
8. **Things to never invent** — methods/members/enums that look plausible but are NOT in the header (e.g. `Builder::createTorus`, `MatrixTransform::setMatrix`, `observer_ptr::expired`).

## How to verify generated VSG code

When this skill is used to generate code, prove it before claiming it works:

- **Strongest:** if a VSG install is present, build a tiny `find_package(vsg)` CMake target that compiles **and links** the snippet (`target_link_libraries(probe vsg::vsg)`, then `cmake --build`). A successful link proves every signature resolves in `libvsg`, catching ABI/signature mismatches a parse misses. (During extraction this linked a 213KB `vsglinkprobe` executable against the installed 1.1.14 lib.)
- **Cheaper fallback:** `clang++ -std=c++17 -fsyntax-only -I<vsg-include> -I<vulkan-include> snippet.cpp` — a clean parse proves the API surface and headers resolve, without a link step.
- **Idiom audit:** confirm `T::create()` (never `new`), `Inherit<>` for new types, `viewer->compile()` once before the loop, nullable `create`/`read_cast` checked, and `dmat4` (not `mat4`) for scene transforms.
- **Cite audit:** every VSG class used should be traceable to its header.

## Common agent failure modes (VSG-specific)

- **Reaching for `new vsg::X`.** VSG objects are created via `vsg::X::create(...)`; `new`/`delete` won't even compile (protected destructor). → `references/components/core-objects.md`.
- **Deriving directly from a base** (`class Foo : public vsg::Group`). Use `vsg::Inherit<vsg::Group, Foo>` so `create()`/RTTI/`accept()` are wired. → `core-objects.md`.
- **Skipping `viewer->compile()`** or calling it before the scene/tasks are wired. It must run once, after wiring, before the loop. → `viewer.md`, `commandgraph.md`.
- **Inventing convenience APIs** — `viewer->run()`, `Builder::createPlane`, `MatrixTransform::setPosition`, `View::setCamera`. None exist; see each file's "Things to never invent."
- **Mixing precision** — passing `dvec3` into `Builder::GeometryInfo` (float) or building a scene `MatrixTransform` from `mat4`. → `builder.md`, `matrixtransform.md`.
- **Mutating the graph mid-record.** Structural edits during `recordAndSubmit()` race the record traversal. → `group.md`.
- **Treating docs/memory as authoritative over the header.** If `include/vsg/**.h` disagrees with recollection, the header wins.
- **Black geometry.** Lit pipelines (the `Builder` default, `StateInfo.lighting=true`) render black unless a `vsg::Light` is in the View/scene; add `vsg::createHeadlight()` (or rely on `createCommandGraphForView`'s default headlight). → `references/components/lighting.md`.
- **Headless rendering on macOS.** Surface-less offscreen rendering crashes under MoltenVK (`BindGraphicsPipeline::record` null pipeline array) — not a wiring bug; the official `vsgheadless` reproduces it. On macOS, capture from a window-backed swapchain instead. → `references/examples/headless-rendering.md` (Platform note).

## Note for future maintainers

When VSG bumps version, re-run the extraction against the new `include/vsg/` tree and re-run the compile-probe; the headers are the only thing that can mechanically refute a drifted claim. The slate here is the **core** of VSG (the "set up a viewer and render a scene graph" path), deliberately neat rather than exhaustive — VSG ships ~238 public classes across 18 subsystems (`animation`, `lighting`, `raytracing`, `text`, `meshshaders`, …). Extend the slate by adding a `references/components/<name>.md` (same 8-section contract, same citation discipline) and a routing-table row in `SKILL.md`.
