#!/usr/bin/env bash
# Turn on the local secret checks. Run once per clone:
#
#   bash tools/release/install-hooks.sh
#
# The hooks are tracked in the repository so that every clone and every agent
# gets the same ones, and core.hooksPath is set to point at them. This does not
# change .git/hooks and does not need write access to it.
#
# CI runs the same check on every push. That is the backstop, not the gate: a
# secret in a pushed commit is already on the remote and has to be rewritten out
# of the history, which is why the check has to run before the commit.
#
# To turn the local checks off again: git config --unset core.hooksPath
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"
hooks_rel="tools/release/hooks"
[[ -d "$hooks_rel" ]] || {
    echo "install-hooks.sh: $hooks_rel is missing" >&2
    exit 1
}

# git ignores the executable bit for files under core.hooksPath only if they are
# not executable, and a hook that is not executable is skipped silently. That is
# the whole gate failing without a word.
for hook in "$hooks_rel"/pre-commit "$hooks_rel"/commit-msg; do
    [[ -f "$hook" ]] || {
        echo "install-hooks.sh: $hook is missing" >&2
        exit 1
    }
    [[ -x "$hook" ]] || chmod +x "$hook"
done

git config core.hooksPath "$hooks_rel"
echo "core.hooksPath is $hooks_rel"
echo "the checks run on every commit in this clone."