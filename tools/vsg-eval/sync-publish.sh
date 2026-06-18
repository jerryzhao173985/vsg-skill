#!/usr/bin/env bash
# sync-publish.sh — mirror the WORKING skill (+ harness) into the publish repo and validate,
# so the GitHub copy never silently diverges from where development actually happens.
# Run this before every `git push` of the publish repo. (Paths are environment-specific —
# edit the four below for your machine.)
set -uo pipefail

WORK=/Users/jerry/ds-skill/vsgExamples/.claude/skills/vsg            # where /vsg dev happens
HARNESS=/Users/jerry/ds-skill/vsgExamples/tools/vsg-eval            # the eval harness source
PUBREPO=/Users/jerry/ds-skill/vsg-skill                             # the git repo to publish
META=/Users/jerry/ds-skill/vsgExamples/.claude/skills/extract-ds-skill/scripts/check-skill-docs.sh
PUB="$PUBREPO/.claude/skills/vsg"

# 1) mirror skill + harness into the publish repo (drop un-validated scratch drafts)
rsync -a --delete "$WORK/" "$PUB/"
rsync -a --delete --exclude unvalidated-drafts "$HARNESS/" "$PUBREPO/tools/vsg-eval/"

# 2) validate the publish copy — block if anything is off
echo "== validating publish copy =="
"$PUBREPO/tools/vsg-eval/check-skill-refs.sh" "$PUB"
[ -f "$META" ] && bash "$META" "$PUB" 2>&1 | grep -E "CHECK_RESULT|=FAIL" || echo "(meta check-skill-docs.sh not found — skipped)"

# 3) confirm byte-identical to the working source
if [ -z "$(diff -rq "$WORK" "$PUB" 2>&1)" ]; then
  echo "SYNC=IDENTICAL  ($(ls "$PUB"/references/components/*.md | wc -l | tr -d ' ') components)"
  echo "Next:  cd $PUBREPO && git add -A && git commit -m 'sync skill' && git push"
else
  echo "SYNC=DIFF (investigate):"; diff -rq "$WORK" "$PUB"
fi
