#!/usr/bin/env bash
# run-suite.sh — self-improvement loop for the `vsg` skill.
#
# Spawns a FRESH, ISOLATED `claude -p "/vsg <prompt>"` session per test, then scores
# each with the harness (eval-session.py + replay build). Isolation is non-negotiable:
# each run gets its own COPY of the skill, so an agent that "helpfully" authors files
# into the skill (the E-session contamination) cannot touch the live one. Grounding
# repos are added read-only; the live skill dir is never in an --add-dir path.
#
# Usage:
#   tools/vsg-eval/run-suite.sh                       # default 4-prompt build suite
#   tools/vsg-eval/run-suite.sh A "Build a VSG viewer that loads a glTF and orbits it"
#   MODEL=opus tools/vsg-eval/run-suite.sh            # override model (default: sonnet)
set -uo pipefail

REPO=/Users/jerry/ds-skill/vsgExamples
SKILL="$REPO/.claude/skills/vsg"
EVAL="$REPO/tools/vsg-eval"
VSG=/Users/jerry/VulkanSceneGraph
EXAMPLES="$REPO/examples"          # examples subdir ONLY — never the repo root (it holds the live skill)
VKSDK="${VULKAN_SDK:-/Users/jerry/VulkanSDK/1.4.341.1/macOS}"
MODEL="${MODEL:-sonnet}"
OUT="${OUT:-/tmp/vsg-suite}"
mkdir -p "$OUT"

LINK_CMAKE='cmake_minimum_required(VERSION 3.14)
project(replay LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
find_package(vsg REQUIRED)
find_package(vsgXchange QUIET)
add_executable(replay main.cpp)
target_link_libraries(replay vsg::vsg)
if (vsgXchange_FOUND)
  target_compile_definitions(replay PRIVATE vsgXchange_FOUND)
  target_link_libraries(replay vsgXchange::vsgXchange)
endif()'

run_one() {
  local label="$1" prompt="$2"
  local ws="$OUT/$label"
  echo "──────── $label ────────"
  rm -rf "$ws"; mkdir -p "$ws/.claude/skills"
  cp -R "$SKILL" "$ws/.claude/skills/vsg"        # ISOLATED skill copy

  ( cd "$ws" && timeout 1200 claude -p "/vsg $prompt" \
        --model "$MODEL" --permission-mode bypassPermissions \
        --add-dir "$VSG" --add-dir "$EXAMPLES" >claude.out 2>claude.err ) || true

  # locate the session transcript (claude escapes the REAL cwd: macOS /tmp -> /private/tmp)
  local proj=~/.claude/projects/$(realpath "$ws" | sed 's#[/.]#-#g')
  local jl; jl=$(ls -t "$proj"/*.jsonl 2>/dev/null | head -1)
  if [[ -z "${jl:-}" || ! -f "$jl" ]]; then echo "  NO TRANSCRIPT (claude.err:)"; tail -3 "$ws/claude.err"; return; fi

  # process signals (analyzer points at the ISOLATED copy as 'the skill')
  python3 "$EVAL/eval-session.py" "$jl" "$ws/.claude/skills/vsg" "$VSG" "$REPO" > "$ws/eval.json" 2>/dev/null
  local outr ptr halluc; outr=$(python3 -c "import json;print(json.load(open('$ws/eval.json'))['D2_coverage']['out_reads'])" 2>/dev/null||echo '?')
  ptr=$(python3 -c "import json;print(json.load(open('$ws/eval.json'))['D3_self_contained']['pointer_reads'])" 2>/dev/null||echo '?')
  halluc=$(python3 -c "import json;print(len(json.load(open('$ws/eval.json'))['D5_honesty']['hallucinated_symbols']))" 2>/dev/null||echo '?')

  # outcome: replay-build the agent's produced main.cpp (it writes into the workspace cwd)
  mkdir -p "$ws/replay"
  local mainf; mainf=$(find "$ws" -maxdepth 2 -name '*.cpp' \
        -not -path '*/build/*' -not -path '*/.claude/*' -not -path '*/replay/*' 2>/dev/null \
        | xargs ls -S 2>/dev/null | head -1)
  [[ -n "${mainf:-}" ]] && cp "$mainf" "$ws/replay/main.cpp"
  local build="n/a (no code produced)"
  if [[ -f "$ws/replay/main.cpp" ]]; then
    printf '%s' "$LINK_CMAKE" > "$ws/replay/CMakeLists.txt"
    ( cd "$ws/replay" && cmake -B build -S . -DCMAKE_PREFIX_PATH="/usr/local;$VKSDK" >c.log 2>&1 && cmake --build build >b.log 2>&1 ) || true
    [[ -x "$ws/replay/build/replay" ]] && build="PASS (compiles+links)" || build="FAIL"
  fi

  # contamination check: did the agent mutate the isolated skill copy vs the live one?
  local contam; contam=$(diff -rq "$SKILL" "$ws/.claude/skills/vsg" 2>/dev/null | grep -c .)
  echo "  REPLAY_BUILD=$build | out_reads=$outr pointer_reads=$ptr halluc=$halluc | skill_mutations=$contam"
  [[ "$contam" -gt 0 ]] && diff -rq "$SKILL" "$ws/.claude/skills/vsg" | sed 's/^/    /'
}

if [[ $# -ge 2 ]]; then
  run_one "$1" "$2"
else
  run_one A "Build a VSG viewer that loads a model file and orbits it with a trackball"
  run_one B "Create a procedurally-generated VSG scene of lit boxes and spheres in a resizable window"
  run_one C "Write a headless VSG renderer that renders a model to an image file"
  run_one D "Build a VSG app placing three copies of a model with different transforms, with mouse orbit"
fi
echo "Artifacts under $OUT/<label>/ (claude.out, eval.json, replay/build)."
