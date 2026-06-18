# VSG skill evaluation — the stable rubric

## Why the first rubric was unstable (and this one isn't)

A flat 0/1 checklist graded by a human or an LLM is **unstable**: it is grader-dependent, it conflates a hallucinated API (the skill taught a lie) with a missing null-check (cosmetic), and it blames the skill for failures that are the **agent's** or the **environment's**. None of that is reproducible.

A *true* rubric has three properties, and every rule below has all three:

1. **Mechanical** — counts, regex, and compiler/linker exit codes. No judgment in the score. Same transcript + same skill ⇒ same numbers, every time.
2. **Attributed** — every defect is tagged `SKILL-GAP | SKILL-BUG | AGENT | ENV`. **Only `SKILL-*` count against the skill.** The MoltenVK crash is `ENV`; an agent ignoring a Hard rule is `AGENT`. Neither lowers the skill score.
3. **Evidence-anchored** — every finding quotes a transcript line, a file path, or a build error. No score without a citable cause.

The LLM's job is to **interpret** the mechanical evidence into fixes — it is **never the judge of the score**. That is the single change that removes the instability.

## What you are actually testing

The skill is an **adapter**: the unit under test is **(a cold agent + the skill) → code**. So you never grade the prose in isolation. You measure the *delta the skill failed to provide* — how much the agent had to go outside the skill, get wrong, or fabricate — **attributed to the skill**. A run can build perfectly while the skill contributed nothing (agent read raw source); that is a coverage failure even though the outcome was fine.

## Two engines

| Engine | Answers | How | File |
|---|---|---|---|
| **Replay build** (outcome / ground truth) | Is the produced code real & idiomatic? | Extract each produced `main.cpp` from the transcript, compile **and link** it against installed VSG via `find_package(vsg)`. | `replay` step (see below) |
| **Transcript analyzer** (process) | Did the agent stay in the skill & stay honest? | Deterministic parse of the `.jsonl`: reads, gap phrases, honesty markers, grep-verified symbols. | `eval-session.py` |

The replay build is authoritative for grounding: **code that compiles+links has zero hallucinated symbols and zero non-compiling idioms, by definition.** It subsumes any regex hallucination/idiom heuristic — those are only a fallback for code that did not build.

## The dimensions

| Dim | Question | Mechanical signal | Attribution of a failure |
|---|---|---|---|
| **D1 Grounding truth** | Is what the skill says real? | Replay build = PASS/FAIL; cite-resolution sweep (all `path:line` resolve, in range); link-probe of the slate. | FAIL ⇒ **SKILL-BUG** (critical) |
| **D2 Coverage** | Did the skill *contain* what the task needed? | Count of `Read`s to files **outside** the skill (VSG headers/`src`/examples) **not** cited by a loaded skill file. | each ⇒ **SKILL-GAP** (major) |
| **D3 Self-containment** | Did the skill *teach* or only *point*? | `Read`s to a file the loaded skill **cited** (the skill said "see X" and the agent had to open X). | each ⇒ **SKILL-GAP** (moderate; "pointer-not-coverage") |
| **D4 Idiom steering** | Right patterns? | Replay build PASS = idioms compile. Static cross-check: `new vsg::`, missing `compile()`, lit-Builder-no-light, unchecked nullable. | covered-by-skill-but-violated ⇒ **AGENT**; never-stated ⇒ **SKILL-GAP** |
| **D5 Honesty** | Did `[VERIFY]`/scope discipline transfer? | Grep-verified produced `vsg::` symbols (absent from headers ⇒ hallucination); count of decline/`[VERIFY]`/out-of-scope statements on OOS asks. | fabrication of a real-looking API ⇒ **SKILL-BUG**; correct decline ⇒ PASS |

**Severity weights** (not flat): SKILL-BUG = 3 (taught a falsehood), SKILL-GAP-coverage = 2, SKILL-GAP-pointer = 1.5, idiom-gap = 2, stylistic = 0.5. `AGENT`/`ENV` = 0 against the skill.

## The headline number (optional, derived)

Per task: **SkillDelta = Σ(skill-attributable severity)**. A clean run is `SkillDelta = 0`. The number is only a roll-up of the evidence rows — never type it without the rows. The **primary output is the structured per-dimension record**, because that is what tells you *what to fix*.

## Pass bar / when to act

- `SkillDelta = 0` across the fixed prompt suite ⇒ ship.
- A `SKILL-BUG` (replay FAIL, or a cite that doesn't resolve) ⇒ **block**; fix before anything else.
- A `SKILL-GAP` (coverage or pointer) ⇒ extract the missing knowledge (headers + `src/`), cite-verify, link-probe, integrate, then **re-run the originally-failing prompt cold** — it isn't fixed until that prompt scores 0.
- An `AGENT` or `ENV` finding ⇒ **do not change the skill** (unless you can make the skill *prevent* the agent error with a Hard rule). Disambiguate `ENV` by reproducing against the official example: identical failure ⇒ environment, not skill.

## How to run

```sh
# process signals for one session
python3 tools/vsg-eval/eval-session.py <session.jsonl> .claude/skills/vsg <vsg_repo> <examples_repo>

# ground truth: extract each produced main.cpp and build+link it (the replay step in this dir's history)
#   -> REPLAY_BUILD=PASS|FAIL per session
```

## Isolation (non-negotiable — a test must never mutate the skill)

Run every test in a **fresh session** AND against an **isolated copy** of the skill, never the live `.claude/skills/vsg/`. A `/vsg` agent has write tools; given a "next-gap" probe it will helpfully *author new component files and edit SKILL.md's routing table + slate directly into the live skill*. That happened in the E run (it injected 7 unvalidated component files + 14 SKILL.md lines), which silently inflates the skill with un-validated content and corrupts the next audit. Two safe protocols:

- **Copy-then-test:** `cp -R .claude/skills/vsg /tmp/vsg-under-test`, point the test agent at the copy, diff afterward. Promote only what passes the validation bar.
- **Read-only instruction:** tell the test agent it may read the skill and produce app code, but must **not** write inside `.claude/skills/`.

Anything a test writes into the skill is a draft, not a validated addition — quarantine it (see `unvalidated-drafts/`) and run it through extraction → verify → link-probe before it earns a routing row.

Fixed prompt suite (run each in a **fresh, isolated** session):
- **A** golden viewer · **B** lit procedural scene · **C** headless renderer · **D** the four traps (black/nullable/record-thread/precision) · **E** next-gap probe (shadows/animation/text/multi-view/picking/shader/compute) · **F** honesty (ask for invented/out-of-scope APIs).

## Baseline result (2026-06-17, 6 sessions)

| | Task | Replay build | D2 out-reads | D3 pointer-reads | Honesty | SkillDelta |
|---|---|---|---|---|---|---|
| A | golden viewer | PASS | 0 | 1 (expected lift) | clean | 0 |
| B | lit procedural | **PASS** | 0 | 0 | clean | **0** ← lighting fix confirmed |
| C | headless | PASS | 0 | **5× vsgheadless.cpp** | clean | 1.5 (pointer) → **fixed** by shipping `headless-main.cpp` |
| D | four traps | PASS | 0 | 0 | clean | 0 |
| E | next-gap probe | n/a (research) | 0 | 1 | mapped 7 topics | 0 |
| F | honesty/negative | n/a | 0 | 0 | declined invented APIs | 0 |

**Read:** 5/6 clean; the one gap (C, pointer-not-coverage) is closed. B is the previously-failing case, now `SkillDelta=0` — a passing regression test, mechanically.

## Backlog (from E — add only when a real task hits the wall)

E mapped these to real example dirs but no A–D task needed them yet, so they are **not** added (adding speculatively is the bloat that makes a skill worse): `shadows` (vsgshadow — already `[VERIFY]` in lighting.md), `animation` (vsganimation), `text` (vsgtext), `multi-view`/`multi-window` (vsgmultiviews/vsgwindows), `picking/intersection` (vsgintersection). `custom ShaderSet` and `compute` straddle the out-of-scope GLSL line — route to a shader skill. Promote one to a component only when a fresh-session task reads its example to get the work done.
