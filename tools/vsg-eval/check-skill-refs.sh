#!/usr/bin/env bash
# check-skill-refs.sh — catch DANGLING prose cross-references.
# check-skill-docs.sh validates routing-TABLE rows; it does NOT check files that prose merely
# mentions ("see references/components/shadows.md"). A live test surfaced exactly that gap
# (lighting.md pointed at a quarantined shadows.md). This closes it: every references/<...>.(md|cpp)
# named anywhere in the skill must exist.
set -uo pipefail
SK="${1:-.claude/skills/vsg}"
bad=0
for ref in $(grep -rhoE 'references/[A-Za-z0-9_./-]+\.(md|cpp)' "$SK" | sort -u); do
  [ -f "$SK/$ref" ] || { echo "DANGLING: $ref"; bad=1; }
done
[ "$bad" -eq 0 ] && echo "SKILL_REFS=OK (no dangling prose references)" || { echo "SKILL_REFS=FAIL"; exit 1; }
