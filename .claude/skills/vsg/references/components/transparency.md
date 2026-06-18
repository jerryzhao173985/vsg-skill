---
title: Transparency / correct alpha blending
description: Enabling src-alpha-over blending only colours pixels; correct "blends from any angle" comes from wrapping each translucent node in vsg::DepthSorted so it lands in a back-to-front sorted vsg::Bin, recomputed every frame.
---

## Public include
```cpp
#include <vsg/all.h>            // barrel include (recommended)
// or specifically:
#include <vsg/nodes/DepthSorted.h>   // vsg::DepthSorted
#include <vsg/nodes/Bin.h>           // vsg::Bin, Bin::SortOrder
#include <vsg/state/ColorBlendState.h>
#include <vsg/utils/Builder.h>       // vsg::StateInfo.blending
```
Namespace is `vsg::`. `vsg::DepthSorted` is declared at `include/vsg/nodes/DepthSorted.h:26`, `vsg::Bin` at `include/vsg/nodes/Bin.h:23`, and `StateInfo.blending` at `include/vsg/utils/Builder.h:30`.

## When to use
Use this whenever you draw translucent geometry (alpha < 1) that must "look right from any camera angle". Two separate concerns, both required:
1. **Blending must be ENABLED** on the pipeline — otherwise the fragment's alpha is ignored. `StateInfo.blending = true` (`include/vsg/utils/Builder.h:30`) makes `Builder` configure src-alpha-over; under the hood `ColorBlendState::configureAttachments(true)` sets `srcColorBlendFactor = SRC_ALPHA`, `dstColorBlendFactor = ONE_MINUS_SRC_ALPHA`, `colorBlendOp = ADD` (`src/vsg/state/ColorBlendState.cpp:52-65`).
2. **Draw order must be back-to-front** — `SRC_ALPHA / ONE_MINUS_SRC_ALPHA` is order-dependent, so a far panel recorded *after* a near one blends wrong. Enabling blending does NOT sort; naive translucent geometry records in scene-graph order and is wrong from at least some angles. Correct any-angle blending = wrap each translucent node in `vsg::DepthSorted` so it is binned and sorted per frame (`include/vsg/nodes/DepthSorted.h:21-25`).

Do NOT use `DepthSorted` for opaque geometry — opaque draws correctly with the depth buffer alone and binning just adds per-frame sort cost.

## Key API
- `vsg::DepthSorted::create(int32_t binNumber, const dsphere& bound, ref_ptr<Node> child)` — wrap one translucent subgraph; assigns it to `binNumber` and supplies the bounding sphere used for depth (`include/vsg/nodes/DepthSorted.h:31`). Members are public: `int32_t binNumber`, `dsphere bound`, `ref_ptr<Node> child` (`include/vsg/nodes/DepthSorted.h:33-35`).
- `vsg::dsphere{center, radius}` — `t_sphere<double>` (`include/vsg/maths/sphere.h:124`); ctor `t_sphere(const t_vec3<R>& c, T rad)` (`include/vsg/maths/sphere.h:70`), members `.center` / `.radius` (`include/vsg/maths/sphere.h:52-53`). The bound must enclose `child` in its LOCAL coordinates — its `center` is what gets depth-tested.
- `vsg::Bin::create(int32_t binNumber, Bin::SortOrder)` — a sorting bucket on the View (`include/vsg/nodes/Bin.h:35`). `enum SortOrder { NO_SORT, ASCENDING, DESCENDING }` (`include/vsg/nodes/Bin.h:26-31`). For translucency you want **`DESCENDING`** = farthest recorded first (`src/vsg/nodes/Bin.cpp:120-122`).
- `View::bins` — `std::vector<ref_ptr<Bin>>` on the View, **EMPTY by default** (`include/vsg/app/View.h:84`). `RecordTraversal::apply(const View&)` indexes straight into this vector to dispatch a `DepthSorted` (`src/vsg/app/RecordTraversal.cpp:586-598, 701-703`).
- `StateInfo.blending` — `bool`, default `false` (`include/vsg/utils/Builder.h:30`), passed to `Builder::createBox/createSphere/...` to get a blending pipeline.
- `ColorBlendState::configureAttachments(bool blendEnable)` — the manual route when you build pipelines yourself instead of `Builder` (`src/vsg/state/ColorBlendState.cpp:52`).

## Best Practices
- **`viewer->compile()` auto-creates the bins for you — this is the normal path.** During compile, `CollectResourceRequirements::apply(const DepthSorted&)` inserts `depthSorted.binNumber` into the view's collected indices (`src/vsg/vk/ResourceRequirements.cpp:280-282`), then `Viewer::compile()` walks those indices and, for any binNumber not already present, pushes `Bin::create(binNumber, sortOrder)` onto `view->bins` (`src/vsg/app/Viewer.cpp:328-347`). So you usually only place `DepthSorted` nodes in the scene and call `compile()`; you do NOT have to register bins by hand.
- **Use a POSITIVE binNumber for translucency.** The auto-created bin's sort order is chosen by sign: `binNumber < 0 -> ASCENDING`, `== 0 -> NO_SORT`, `> 0 -> DESCENDING` (`src/vsg/app/Viewer.cpp:343`). Positive => `DESCENDING` => back-to-front, exactly what alpha-over needs. A common choice is `binNumber = 1`.
- **Manual `view->bins.push_back(...)` is the OVERRIDE path**, not the required one. Do it only when you need a sort order that differs from the sign rule, or when you construct your own bins before compile. If you do, the binNumber MUST match the `DepthSorted` binNumber — `addToBin` indexes `bins[binNumber - minimumBinNumber]` with no bounds creation, so a mismatch is undefined behaviour (`src/vsg/app/RecordTraversal.cpp:701-703`).
- **The bound must actually enclose the child.** At record time the depth key is the bound CENTRE's view-space z: `value = -(mv[0][2]*cx + mv[1][2]*cy + mv[2][2]*cz + mv[3][2])` (`src/vsg/app/RecordTraversal.cpp:317-321`) — farther from the eye => larger value. A wrong/zero centre sorts wrong. The bound is also frustum-tested (`state->intersect(depthSorted.bound)`, `RecordTraversal.cpp:315`), so a too-small radius can cull the node.
- **Sorting is re-derived every frame**, so it stays correct as the camera orbits: `Bin::traverse` re-sorts `_binElements` each record pass (`src/vsg/nodes/Bin.cpp:115-125`), and the bin is cleared+refilled per View per frame (`src/vsg/app/RecordTraversal.cpp:594-598`).
- **Order within the View is fixed: opaque subgraph first, then bins.** `RecordTraversal::apply(const View&)` traverses the View's normal children (recording opaque draws immediately), THEN calls `bin->accept(*this)` for each bin (`src/vsg/app/RecordTraversal.cpp:639-656`). So opaque geometry is correctly behind the sorted translucent layer without extra wiring.
- **Per-object granularity.** Sorting is per `DepthSorted` node (its bound centre), not per triangle. For correct results give each separable translucent object its own `DepthSorted`; a single `DepthSorted` over many interpenetrating panels can still self-overlap wrong.

## Composition examples
Delta only — see `references/examples/model-viewer.md` for the Viewer/Window/Camera/render-loop scaffold.
```cpp
#include <vsg/all.h>

// --- 1. translucent geometry: blending must be ENABLED on the pipeline ---
auto builder = vsg::Builder::create();
vsg::StateInfo si;
si.blending = true;                 // src-alpha-over; does NOT sort  (Builder.h:30)

auto scene = vsg::Group::create();
const int32_t translucentBin = 1;   // POSITIVE -> auto Bin is DESCENDING (back-to-front)

// --- 2. wrap EACH translucent panel in a DepthSorted so it gets binned+sorted ---
for (auto& placement : panelPlacements)
{
    vsg::GeometryInfo gi;
    gi.position = placement.center;
    gi.dx = {placement.size, 0.0f, 0.0f};
    gi.dy = {0.0f, placement.size, 0.0f};
    gi.color = {placement.rgb.x, placement.rgb.y, placement.rgb.z, 0.4f}; // alpha < 1
    auto panel = builder->createQuad(gi, si);

    // bound must ENCLOSE the panel; its centre is the depth key (RecordTraversal.cpp:317-321)
    vsg::dsphere bound(vsg::dvec3(placement.center), placement.size);
    scene->addChild(vsg::DepthSorted::create(translucentBin, bound, panel));
}

auto camera = createCameraForScene(scene, window);   // see model-viewer.md
auto commandGraph = vsg::createCommandGraphForView(window, camera, scene);
viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph});

// --- 3. compile() AUTO-CREATES Bin(1, DESCENDING) on the View from the DepthSorted nodes ---
viewer->compile();   // ResourceRequirements.cpp:280-282 + Viewer.cpp:328-347
// distilled from include/vsg/nodes/DepthSorted.h:31, include/vsg/nodes/Bin.h:35,
// src/vsg/app/Viewer.cpp:328-347 ; built as: translucent_panels
```
Override the auto sort order only when you need something other than the sign rule:
```cpp
// Optional: pre-register the bin yourself BEFORE compile() to force a specific SortOrder.
// view->bins is empty by default (View.h:84); binNumber MUST match the DepthSorted's.
auto view = vsg::View::create(camera, scene);
view->bins.push_back(vsg::Bin::create(translucentBin, vsg::Bin::DESCENDING));
// compile() then sees the bin already present and will not overwrite it (Viewer.cpp:334-345)
```

## Source references
- `include/vsg/nodes/DepthSorted.h` — `DepthSorted(binNumber, dsphere bound, ref_ptr<Node> child)` ctor and public members (lines 31, 33-35); class doc (21-25).
- `include/vsg/nodes/Bin.h` — `Bin(int32_t, SortOrder)` ctor (35) and `enum SortOrder` (26-31).
- `src/vsg/nodes/Bin.cpp` — `Bin::traverse` sorts `_binElements`; `DESCENDING` records largest-value (farthest) first (115-125); `Bin::add` stores the key (60-107).
- `src/vsg/app/RecordTraversal.cpp` — `apply(const DepthSorted&)` computes `value = -z_view` and calls `addToBin` (311-323); `apply(const View&)` resizes/clears `bins` from `view.bins` then records opaque then bins (586-598, 639-656); `addToBin` indexes `bins[binNumber - minimumBinNumber]` (701-703).
- `include/vsg/app/View.h` — `std::vector<ref_ptr<Bin>> bins`, empty by default (84).
- `src/vsg/vk/ResourceRequirements.cpp` — `CollectResourceRequirements::apply(const DepthSorted&)` collects `binNumber` (280-282); `apply(const View&)` scopes the bin indices (243-278).
- `src/vsg/app/Viewer.cpp` — `compile()` auto-creates a `Bin` per collected binNumber, sort order by sign (328-347).
- `src/vsg/state/ColorBlendState.cpp` — `configureAttachments(true)` => `SRC_ALPHA / ONE_MINUS_SRC_ALPHA / ADD` (52-65).
- `include/vsg/utils/Builder.h` — `StateInfo.blending` (30).
- `include/vsg/maths/sphere.h` — `dsphere` alias (124), ctor (70), `.center`/`.radius` (52-53).

## Common mistakes
- Setting `StateInfo.blending = true` and expecting correct results without `DepthSorted` -> blending is enabled but unsorted; panels blend in scene-graph order and are wrong from some angles (`src/vsg/state/ColorBlendState.cpp:52` enables, nothing sorts).
- Using `DepthSorted` but constructing the View's bins by hand with a **mismatched** binNumber -> `addToBin` indexes `bins[binNumber - minimumBinNumber]` with no allocation; out-of-range = UB (`src/vsg/app/RecordTraversal.cpp:701-703`). Match the numbers (or just let `compile()` create the bin).
- Manually choosing `Bin::ASCENDING` (or a negative binNumber, which auto-selects ASCENDING) for translucency -> records near-to-far, the opposite of what alpha-over needs; use `DESCENDING` / a positive binNumber (`src/vsg/nodes/Bin.cpp:117-122`, `src/vsg/app/Viewer.cpp:343`).
- Giving the `DepthSorted` a degenerate/zero `bound` -> wrong depth key and/or frustum-culled away (`src/vsg/app/RecordTraversal.cpp:315-319`). Size the `dsphere` to enclose the child.
- Forgetting `viewer->compile()` after adding `DepthSorted` nodes -> the bins are never auto-created from the collected indices and the View has no bin to dispatch into (`src/vsg/app/Viewer.cpp:328-347`).
- Wrapping opaque geometry in `DepthSorted` -> needless per-frame sorting; the depth buffer already orders opaque draws.

## Things to never invent
- There is no "automatic" transparency sort triggered by alpha alone, and no `setTransparent()` / `setBlending()` toggle on a node — you must place a `vsg::DepthSorted` node and enable blending on the pipeline (`include/vsg/nodes/DepthSorted.h:26`, `include/vsg/utils/Builder.h:30`).
- `DepthSorted` takes a `dsphere` bound, not a box and not "auto-computed from child" — the ctor signature is `(int32_t, const dsphere&, ref_ptr<Node>)` with no bound-less overload (`include/vsg/nodes/DepthSorted.h:31`).
- `Bin` has no `setSortOrder()` / `sort()` method to call yourself; sort order is fixed at construction via the ctor and applied inside `Bin::traverse` during recording (`include/vsg/nodes/Bin.h:35,48`, `src/vsg/nodes/Bin.cpp:109-125`).
- There is no `View::addBin()` or `View::setBin()` accessor — `bins` is a public `std::vector<ref_ptr<Bin>>` you push into directly (`include/vsg/app/View.h:84`).
- `Bin::SortOrder` has exactly three values `NO_SORT, ASCENDING, DESCENDING` — there is no `BACK_TO_FRONT` / `FRONT_TO_BACK` enumerator (`include/vsg/nodes/Bin.h:26-31`).
- `ColorBlendState::configureAttachments` takes a single `bool`; there is no per-factor overload — for custom factors edit the `attachments` `VkPipelineColorBlendAttachmentState` directly (`src/vsg/state/ColorBlendState.cpp:52`, members read/written at 103-113).
