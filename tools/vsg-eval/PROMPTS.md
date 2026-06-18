# VSG skill — test prompt catalog

Run each in a **fresh** session (or `tools/vsg-eval/run-suite.sh <label> "<prompt>"`, which isolates the
skill copy). Pass bar per tier below. Score by the rubric: does it **build+link**, stay **inside the skill**
(0 outside-reads), use the **idioms**, and **scope-out** what VSG lacks?

## Tier 1 — Regression (MUST stay clean: build+link, 0 outside-reads)

These guard the proven core. Any regression here = a `SKILL-BUG`, fix first.

1. `Build a VSG viewer that loads a glTF file and orbits it with a trackball`
2. `Create a procedurally-generated VSG scene of lit boxes and spheres in a resizable window`  ← lighting regression
3. `Write a headless VSG renderer that renders a loaded model to an image file`  ← tests the headless-main.cpp starter
4. `Build a VSG app that places three copies of a model with different transforms, with mouse orbit`

**Pass:** `REPLAY_BUILD=PASS`, `out_reads=0`, idioms clean.

## Tier 2 — Edge probes (HUNT the next gap: which one makes the agent leave the skill?)

These map to the backlog. The one(s) that show `out_reads>0` or repeated `pointer_reads` are the next
extraction targets — promote a backlog topic to a real component ONLY when a probe here reads its example
to get the work done.

5. `Add real-time shadows to a lit VSG scene`  ← shadows (lighting.md marks shadow internals [VERIFY])
6. `Load and play a skeletal or keyframe animation from a glTF model in VSG`  ← animation
7. `Render 3D text labels floating over a model in VSG`  ← text
8. `Show a perspective view and a top-down orthographic view of the same VSG scene side by side`  ← multi-view
9. `Open two windows showing the same VSG scene`  ← multi-window
10. `Implement mouse-click picking in VSG — print which object was clicked`  ← intersection

**Pass:** build+link AND `out_reads=0`. **Finding:** any outside-read names the next component to add.

## Tier 3 — Honesty / scope (MUST decline or [VERIFY], NOT fabricate)

3 traps. The skill must refuse to invent.

11. `Add rigid-body physics so the boxes fall and collide`  ← VSG has no physics → must scope-out
12. `Write the GLSL fragment shader for a custom PBR material`  ← shader authoring is out of scope
13. `Use vsg::Button to add a UI button to the window`  ← does not exist (name-collision trap)

**Pass:** no produced code that fabricates an API; explicit out-of-scope / `[VERIFY]` / "does not exist".

## Tier 4 — Real-project composites (HIGHEST value: stress composition)

These combine features the way a real project does — where a skill earns or fails its keep.

14. `A model viewer with a ground plane, a directional light, and an on-screen filename label`
15. `An offscreen turntable renderer: load a model, render 36 frames rotating around it, write each to a file`
16. `A CAD-style inspector: a model in a perspective view with a small top-down orthographic inset, mouse-pick to highlight`

**Pass:** build+link AND `out_reads=0`. **Finding:** composites surface *composition* gaps the single-feature
prompts miss (e.g. "lit + shadow + text" interactions). A repeated outside-read across composites is a strong
add-signal.

## What to watch (manual eyeball = the rubric)

- **Build+link?** `cmake -B build -S . && cmake --build build` → an executable. The hard gate.
- **Stayed in the skill?** Did it open VSG headers / `src` / `examples` directly? Each = a coverage gap.
- **Pointer-not-coverage?** Did it read an example a skill ref *already cites*, repeatedly? The ref points but doesn't teach.
- **Idioms:** `T::create` (never `new`), `compile()` once before the loop, `dmat4` for scene transforms, null-checks on `read_cast`/`Window::create`, a `vsg::Light` for lit geometry.
- **Honesty (Tier 3):** explicit scope-out / `[VERIFY]`, not a confident fabricated API.

## The loop

Gap found → extract from headers + `src/` → adversarial cite-verify → link-probe → integrate (routing + slate) →
**re-run that exact prompt cold until it scores 0**. Never test against the live skill (it gets contaminated);
`run-suite.sh` isolates a copy for you.
