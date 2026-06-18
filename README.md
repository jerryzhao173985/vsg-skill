# vsg-skill

A [Claude Code](https://claude.com/claude-code) skill for building **native C++ [VulkanSceneGraph](https://github.com/vsg-dev/VulkanSceneGraph) (VSG)** applications. It teaches an agent which headers are authoritative, what the public API actually is, the load-bearing idioms, and how to verify generated code compiles and links against the real library.

Every rule is cited to a `file:line` in the VulkanSceneGraph headers/`src` or the [vsgExamples](https://github.com/vsg-dev/vsgExamples) programs — no rule is ungrounded.

## Install

Copy the skill into your project's skills directory:

```sh
git clone https://github.com/jerryzhao173985/vsg-skill.git
cp -R vsg-skill/.claude/skills/vsg <your-project>/.claude/skills/
```

Then invoke it: `/vsg Build a viewer that loads a glTF and orbits it with a trackball`.

## What's inside

```
.claude/skills/vsg/
├── SKILL.md              # orchestrator: setup, idioms, routing table, hard rules
├── AGENTS.md             # how the skill is shaped + how to keep it honest
└── references/
    ├── components/       # 15 per-class contracts (Viewer, Builder, lighting, shadows,
    │                     #   multiview, intersection, …) — 8 sections each, every claim cited
    ├── foundations/      # 4 architecture mental-models, grounded in src/vsg implementations
    ├── examples/         # worked programs incl. a compile-tested headless-main.cpp
    ├── patterns.md       # end-to-end recipes
    └── anti-patterns.md  # Bad → Good → Why registry
```

The slate is the **core** of VSG (set up a viewer and render a scene graph), deliberately neat rather than exhaustive. It is grounded against VSG `v1.1.14`.

## How it's grounded

- **Headers are API authority**; `src/vsg` grounds runtime behaviour; the `examples/` are idiomatic usage.
- **Compile + link verified**: a `find_package(vsg)` probe links the whole claimed surface against the installed `libvsg`, catching ABI/signature drift a parse misses.
- **Cite-verified**: every `file:line` resolves; produced symbols are checked against the headers (0 hallucinations).

## Maintaining & extending — `tools/vsg-eval/`

A self-improvement harness that turns "feels incomplete" into measured, file-level evidence:

| File | Role |
|---|---|
| `RUBRIC.md` | the stable, mechanical, attributed evaluation rubric |
| `run-suite.sh` | spawn isolated `claude -p "/vsg …"` sessions, build+link the output, score it |
| `eval-session.py` | deterministic transcript analyzer (coverage / self-containment / honesty) |
| `check-skill-refs.sh` | catches dangling prose cross-references |
| `PROMPTS.md` | tiered test-prompt catalog (regression / edge-probe / honesty / composite) |

A backlog topic graduates to a component **only when a real prompt has to read its example** to get the work done — that discipline keeps the skill precise instead of bloated.

> The harness scripts reference a local VulkanSceneGraph checkout + install; set the paths at the top of `run-suite.sh` / `eval-session.py` for your environment.
