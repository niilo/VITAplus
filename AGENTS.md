# Vita3K: agent entry point

All rules and working notes for this repository are in
[`CLAUDE.md`](./CLAUDE.md). Read it in full before doing any work here, not a
summary of it. It starts with the writing standard that applies to every piece
of English written for the project, and its `## Security` section applies to
every commit.

Two rules that hold whether or not you have read anything else:

1. **Never commit or print a secret.** This repository is public. Before every
   commit run `bash tools/release/check-no-signing-keys.sh --staged`, and after
   writing a commit message run
   `bash tools/release/check-no-signing-keys.sh --message <file>`. A secret in a
   pushed commit is already published, and the history has to be rewritten to
   remove it. `bash tools/release/install-hooks.sh` turns both checks into real
   git hooks, so a mistake cannot pass. Do not turn them off.
2. **Stay inside the task.** Change what was asked. Report what you found and
   what you left alone. Do not remove files, force push, rewrite history, or
   publish anything unless that was requested.

Do not add rules to this file. Edit `CLAUDE.md` instead.