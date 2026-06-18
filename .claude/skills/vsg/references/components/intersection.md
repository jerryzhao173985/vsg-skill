---
title: Picking & intersection (LineSegmentIntersector / PolytopeIntersector)
description: Mouse picking and box-select against a VSG scene graph — build a LineSegmentIntersector or PolytopeIntersector from a Camera + pixel coords, run scene->accept(intersector), sort hits by ratio, walk nodePath, and toggle a shader-free highlight via vsg::Switch.
---

## Public include

`#include <vsg/utils/LineSegmentIntersector.h>` declares `vsg::LineSegmentIntersector` (`include/vsg/utils/LineSegmentIntersector.h:31`) and pulls in `<vsg/app/Camera.h>` + `<vsg/utils/Intersector.h>` (`include/vsg/utils/LineSegmentIntersector.h:15-16`).
`#include <vsg/utils/PolytopeIntersector.h>` declares `vsg::PolytopeIntersector` (`include/vsg/utils/PolytopeIntersector.h:26`).
`#include <vsg/utils/Intersector.h>` declares the base `vsg::Intersector` (`include/vsg/utils/Intersector.h:23`).
`#include <vsg/nodes/Switch.h>` declares `vsg::Switch` for the shader-free highlight (`include/vsg/nodes/Switch.h:24`).
In practice the example just uses `#include <vsg/all.h>` (`examples/utils/vsgintersection/vsgintersection.cpp:1`), which transitively includes all of the above.

## When to use

Use a `LineSegmentIntersector` for mouse picking: a single click/ray that returns the geometry under the cursor, nearest-first (`examples/utils/vsgintersection/vsgintersection.cpp:124-187`). Use a `PolytopeIntersector` for rubber-band / box selection: a screen-space rectangle that selects everything inside the frustum it sweeps (`examples/utils/vsgintersection/vsgintersection.cpp:189-200`, `include/vsg/utils/PolytopeIntersector.h:33`).

Both are `Inherit<Intersector, …>` subclasses (`include/vsg/utils/LineSegmentIntersector.h:31`, `include/vsg/utils/PolytopeIntersector.h:26`) and `Intersector` is itself an `Inherit<ConstVisitor, Intersector>` (`include/vsg/utils/Intersector.h:23`) — so you run them by calling `scene->accept(*intersector)` on the scene graph (`examples/utils/vsgintersection/vsgintersection.cpp:130`).

Because the base traversal goes through `Switch` (a `Node`), the intersector honors the active `Switch` child mask: `Switch::t_traverse` only recurses into children whose `child.mask` passes the visitor's `traversalMask` (`include/vsg/nodes/Switch.h:55-62`). That means only the currently-VISIBLE variant of a switched subgraph is eligible to be picked — which is what makes the `Switch` highlight pattern below work without any shader.

## Key API

`LineSegmentIntersector` constructors (`include/vsg/utils/LineSegmentIntersector.h:34-35`):
- `LineSegmentIntersector(const dvec3& s, const dvec3& e, ref_ptr<ArrayState> initialArrayData = {})` — explicit world-space segment.
- `LineSegmentIntersector(const Camera& camera, int32_t x, int32_t y, ref_ptr<ArrayState> initialArrayData = {})` — the picking constructor; unprojects pixel `(x, y)` through the camera.

The camera constructor reads `camera.getViewport()` and computes NDC by SUBTRACTING the viewport origin and dividing by viewport size: `ndc.set((x - viewport.x) / viewport.width, (y - viewport.y) / viewport.height)` (`src/vsg/utils/LineSegmentIntersector.cpp:141-147`). It only does this when `viewport.width > 0 && viewport.height > 0` (`src/vsg/utils/LineSegmentIntersector.cpp:144`). It then builds near/far NDC points (respecting reverse-depth when `projectionMatrix(2,2) > 0`), unprojects via `inverse(projectionMatrix)`, and pushes the eye- and world-space segments (`src/vsg/utils/LineSegmentIntersector.cpp:149-165`). The practical consequence: picking is correct even inside an INSET viewport — feed it the inset view's own `Camera` when the click is over that inset's rect.

`LineSegmentIntersector::Intersection` members (`include/vsg/utils/LineSegmentIntersector.h:43-51`):
- `dvec3 localIntersection` — hit point in the geometry's local coords.
- `dvec3 worldIntersection` — hit point in world coords.
- `double ratio` — distance along the segment (0=near .. 1=far); sort key for nearest-first.
- `dmat4 localToWorld` — local→world transform at the hit.
- `NodePath nodePath` — the chain of `const Node*` from scene root to the hit primitive (`NodePath = std::vector<const Node*>`, `include/vsg/utils/Intersector.h:26`).
- `DataList arrays` — the vertex arrays of the hit geometry.
- `IndexRatios indexRatios` — `{index, ratio}` barycentric weights of the hit triangle's three vertices (`include/vsg/utils/LineSegmentIntersector.h:22-28`).
- `uint32_t instanceIndex` — which instance was hit (for instanced draws).
- `operator bool() const { return !nodePath.empty(); }` — truthy when valid (`include/vsg/utils/LineSegmentIntersector.h:54`).

Results live in `intersections` (a `std::vector<ref_ptr<Intersection>>`, `include/vsg/utils/LineSegmentIntersector.h:57-58`). NOTE: this vector is NOT pre-sorted — the example sorts it by `ratio` ascending itself (`examples/utils/vsgintersection/vsgintersection.cpp:143`). LineSegment intersection requires `VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST` geometry; other topologies are skipped (`src/vsg/utils/LineSegmentIntersector.cpp:250`, `:275`).

`PolytopeIntersector` constructors (`include/vsg/utils/PolytopeIntersector.h:30-33`):
- `explicit PolytopeIntersector(const Polytope& in_polytope, …)` — explicit `Polytope = std::vector<dplane>` (`include/vsg/utils/PolytopeIntersector.h:23,30`).
- `PolytopeIntersector(const Camera& camera, double xMin, double yMin, double xMax, double yMax, …)` — window-space rectangle projected into world coords (`include/vsg/utils/PolytopeIntersector.h:33`).

`PolytopeIntersector::Intersection` (`include/vsg/utils/PolytopeIntersector.h:41-48`) has `localIntersection`, `worldIntersection`, `localToWorld`, `nodePath`, `arrays`, `instanceIndex` — but NO `ratio`. Instead of `indexRatios` it exposes `std::vector<uint32_t> indices` (`include/vsg/utils/PolytopeIntersector.h:47`). Its `intersections` is `std::vector<ref_ptr<Intersection>>` (`include/vsg/utils/PolytopeIntersector.h:54-55`).

`vsg::Switch` (shader-free highlight) (`include/vsg/nodes/Switch.h:24`):
- `void addChild(Mask mask, ref_ptr<Node> child)` / `void addChild(bool enabled, ref_ptr<Node> child)` (`include/vsg/nodes/Switch.h:40-43`).
- `void setAllChildren(bool enabled)` (`include/vsg/nodes/Switch.h:46`).
- `void setSingleChildOn(size_t index)` — turn one child on, all others off (`include/vsg/nodes/Switch.h:49`).
- `Children children` is `std::vector<Child>` where `Child{ Mask mask, ref_ptr<Node> node }` (`include/vsg/nodes/Switch.h:30-37`).

Event handlers derive `vsg::Inherit<vsg::Visitor, X>` and attach via `viewer->addEventHandler(...)` (`examples/utils/vsgintersection/vsgintersection.cpp:9`, `:379`).

## Best Practices

- Build a fresh intersector on each click — `LineSegmentIntersector::create(*camera, x, y)` then `scene->accept(*intersector)` (`examples/utils/vsgintersection/vsgintersection.cpp:126,130`). Intersectors accumulate state on a stack and are not meant to be reused across picks.
- Pass the `Camera` for whichever view the click landed in. The constructor's viewport-origin subtraction (`src/vsg/utils/LineSegmentIntersector.cpp:146`) makes inset/multi-view picking correct only if you hand it the matching camera.
- Sort `intersections` by `ratio` before using them; nearest hit is then `intersections.front()` (`examples/utils/vsgintersection/vsgintersection.cpp:143,186`). Always early-out on `intersections.empty()` first (`examples/utils/vsgintersection/vsgintersection.cpp:140`).
- Walk `intersection->nodePath` from root toward the leaf to find the meaningful "object" you care about (a named `MatrixTransform`, a `Switch`, etc.), rather than the bare primitive (`examples/utils/vsgintersection/vsgintersection.cpp:164-168`). Identify nodes via `node->className()` and `node->getValue("name", name)`.
- For highlighting, prefer `vsg::Switch{normalNode, highlightedNode}` and call `setSingleChildOn(index)` to flip variants (`include/vsg/nodes/Switch.h:49`). Since the intersector honors the switch mask (`include/vsg/nodes/Switch.h:60`), the visible variant is exactly what is picked — no custom shader, no recompile.
- Use `PolytopeIntersector` for box/marquee selection; a small symmetric rectangle around the cursor also gives a forgiving "fat click" point pick (`examples/utils/vsgintersection/vsgintersection.cpp:191-197`).
- Cache the last pointer event so a later key press can re-pick at the cursor without a fresh click (`examples/utils/vsgintersection/vsgintersection.cpp:38-40,107,121,247`).

## Composition examples

Distilled and compile-faithful from `examples/utils/vsgintersection/vsgintersection.cpp` (which builds). A picking handler plus a shader-free `Switch` highlight:

```cpp
#include <vsg/all.h>
#include <algorithm>

// Handler derives Inherit<Visitor, X> and is attached via viewer->addEventHandler.
class PickHandler : public vsg::Inherit<vsg::Visitor, PickHandler>
{
public:
    vsg::ref_ptr<vsg::Camera> camera;
    vsg::ref_ptr<vsg::Group>  scene;

    PickHandler(vsg::ref_ptr<vsg::Camera> in_camera, vsg::ref_ptr<vsg::Group> in_scene) :
        camera(in_camera), scene(in_scene) {}

    void apply(vsg::ButtonPressEvent& ev) override
    {
        if (ev.button == 1) pickLineSegment(ev);   // left = ray pick
        else if (ev.button == 2) pickPolytope(ev); // middle = box pick
    }

    void pickLineSegment(vsg::PointerEvent& ev)
    {
        // ctor reads camera.getViewport(), subtracts viewport.x/y -> correct inside an inset
        auto intersector = vsg::LineSegmentIntersector::create(*camera, ev.x, ev.y);
        scene->accept(*intersector);
        if (intersector->intersections.empty()) return;

        // NOT pre-sorted: sort nearest-first by ratio
        std::sort(intersector->intersections.begin(), intersector->intersections.end(),
                  [](auto& lhs, auto& rhs) { return lhs->ratio < rhs->ratio; });

        auto nearest = intersector->intersections.front();
        vsg::info("world hit = ", nearest->worldIntersection, " ratio = ", nearest->ratio);

        // walk root -> leaf to find a named/Switch node, then toggle the highlight variant
        for (auto& node : nearest->nodePath)
        {
            if (auto sw = const_cast<vsg::Switch*>(dynamic_cast<const vsg::Switch*>(node)))
            {
                sw->setSingleChildOn(1); // index 1 = highlighted variant
                break;
            }
        }
    }

    void pickPolytope(vsg::PointerEvent& ev)
    {
        double s = 5.0; // half-width of the screen-space selection rectangle
        auto intersector = vsg::PolytopeIntersector::create(
            *camera, ev.x - s, ev.y - s, ev.x + s, ev.y + s);
        scene->accept(*intersector);
        for (auto& hit : intersector->intersections)
            vsg::info("box-select world = ", hit->worldIntersection,
                      " #indices = ", hit->indices.size());
    }
};

// Build a switchable, shader-free highlightable object: {normal, highlighted}.
vsg::ref_ptr<vsg::Switch> makeHighlightable(vsg::ref_ptr<vsg::Node> normal,
                                            vsg::ref_ptr<vsg::Node> highlighted)
{
    auto sw = vsg::Switch::create();
    sw->addChild(true,  normal);       // index 0, on
    sw->addChild(false, highlighted);  // index 1, off
    return sw; // intersector honors the mask -> only the visible child is picked
}

// ... in main(), after viewer->assignRecordAndSubmitTaskAndPresentation({commandGraph}):
//   viewer->addEventHandler(vsg::Trackball::create(camera));
//   viewer->addEventHandler(PickHandler::create(camera, scene));
```

The real example uses the nearest hit's `worldIntersection` as the spawn position for builder-created shapes on key press (`examples/utils/vsgintersection/vsgintersection.cpp:58-63,186`), and prints `nodePath` class names + `name` values for every hit (`examples/utils/vsgintersection/vsgintersection.cpp:164-168`).

## Source references

- `include/vsg/utils/LineSegmentIntersector.h:31` — `LineSegmentIntersector : public Inherit<Intersector, LineSegmentIntersector>`.
- `include/vsg/utils/LineSegmentIntersector.h:34-35` — the two constructors (explicit segment; `Camera`+`x`+`y`).
- `include/vsg/utils/LineSegmentIntersector.h:43-58` — `Intersection` members and the `intersections` vector.
- `include/vsg/utils/LineSegmentIntersector.h:22-28` — `IndexRatio{index, ratio}` / `IndexRatios`.
- `src/vsg/utils/LineSegmentIntersector.cpp:138-166` — Camera ctor: `getViewport()`, subtract `viewport.x/y`, unproject near/far.
- `src/vsg/utils/LineSegmentIntersector.cpp:250,275` — TRIANGLE_LIST topology requirement.
- `include/vsg/utils/Intersector.h:23,26` — base `Intersector : Inherit<ConstVisitor, Intersector>`; `NodePath = std::vector<const Node*>`.
- `include/vsg/utils/PolytopeIntersector.h:26,30-33,41-55` — class, both ctors, `Intersection` (has `indices`, no `ratio`), `intersections`.
- `include/vsg/nodes/Switch.h:24,40-49,55-62` — `Switch`, `addChild`, `setAllChildren`, `setSingleChildOn`, mask-aware traversal.
- `examples/utils/vsgintersection/vsgintersection.cpp:9` — handler `Inherit<vsg::Visitor, IntersectionHandler>`.
- `examples/utils/vsgintersection/vsgintersection.cpp:124-187` — `intersection_LineSegmentIntersector`: create, accept, sort by ratio, walk nodePath.
- `examples/utils/vsgintersection/vsgintersection.cpp:189-200` — `intersection_PolytopeIntersector`: rectangle around cursor.
- `examples/utils/vsgintersection/vsgintersection.cpp:105-122,247` — button/pointer dispatch and cached `lastPointerEvent`.
- `examples/utils/vsgintersection/vsgintersection.cpp:377-379` — construct handler and `viewer->addEventHandler`.

## Common mistakes

- Assuming `intersections` arrives sorted. It does not — you must `std::sort` by `ratio` for nearest-first (`examples/utils/vsgintersection/vsgintersection.cpp:143`).
- Passing raw window pixels to an inset view's pick with the WRONG camera. The constructor subtracts `viewport.x/y` from the camera you give it (`src/vsg/utils/LineSegmentIntersector.cpp:146`); use the camera whose viewport contains the click.
- Forgetting the TRIANGLE_LIST requirement — line/point/strip topologies return no `LineSegmentIntersector` hits (`src/vsg/utils/LineSegmentIntersector.cpp:250,275`).
- Reading `ratio` or `indexRatios` off a `PolytopeIntersector::Intersection` — it has neither; it exposes `indices` instead (`include/vsg/utils/PolytopeIntersector.h:41-47`).
- Reusing one intersector across multiple picks. Create a fresh one per click (`examples/utils/vsgintersection/vsgintersection.cpp:126`).
- Expecting hidden `Switch` children to be pickable. The intersector skips masked-off children (`include/vsg/nodes/Switch.h:60`), so an off variant is invisible to picking — which is the intended behavior for the highlight pattern.

## Things to never invent

- Do NOT call a `ratio` field on `PolytopeIntersector::Intersection` — it does not exist (`include/vsg/utils/PolytopeIntersector.h:41-48`).
- Do NOT assume an auto-sort, a `sort()` method, or a `nearest()`/`first()` accessor on the intersector — there is none; sort `intersections` yourself.
- Do NOT invent a `LineSegmentIntersector(window, x, y)` or `(viewer, ...)` constructor — only `(s, e, …)` and `(Camera&, x, y, …)` exist (`include/vsg/utils/LineSegmentIntersector.h:34-35`).
- Do NOT expect `Switch` to recolor or restyle a node. It only toggles which child is recorded/visible (`include/vsg/nodes/Switch.h:23,46-49`); per-pixel highlight recoloring requires a custom GLSL `ShaderSet` (as in `vsghighlight.cpp`), which is OUT OF SCOPE for this skill — use the `Switch{normal, highlighted}` approach here.
- Do NOT assume `Intersector` derives from `Visitor`; it is a `ConstVisitor` subclass (`include/vsg/utils/Intersector.h:23`), so it traverses via `scene->accept(*intersector)` against const-visitable nodes.
- Do NOT invent members on `Intersection` beyond `localIntersection`, `worldIntersection`, `ratio`, `localToWorld`, `nodePath`, `arrays`, `indexRatios`, `instanceIndex` (`include/vsg/utils/LineSegmentIntersector.h:43-51`).
