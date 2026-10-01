#!/usr/bin/env bash
# Fail if a signing key could reach the public repository.
# Usage: tools/release/check-no-signing-keys.sh      (run in the repository)
#
# Checks:
#   1. no tracked file is a key store (*.p12, *.jks, *.keystore, *.pfx) or is in .signing/
#   2. the folder .signing/ is ignored by git
#   3. no tracked file outside external/ holds a PEM private key
# CI runs it on every push (.github/workflows/format.yml).
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"
status=0

bad_names="$(git ls-files | grep -E '\.(p12|jks|keystore|pfx)$|(^|/)\.signing/|(^|/)\.vita-plus-signing/' || true)"
if [[ -n "$bad_names" ]]; then
    echo "check-no-signing-keys: key files are tracked by git:" >&2
    echo "$bad_names" >&2
    status=1
fi

for path in .signing/release/signing.env .signing/release/vita-plus-release.p12 .signing/dev/debug.keystore; do
    if ! git check-ignore -q "$path"; then
        echo "check-no-signing-keys: $path is not ignored by git. Fix .gitignore." >&2
        status=1
    fi
done

pem="$(git grep -lE -e '-----BEGIN ([A-Z]+ )?PRIVATE KEY-----' -- . ':!external' || true)"
if [[ -n "$pem" ]]; then
    echo "check-no-signing-keys: a private key is in these tracked files:" >&2
    echo "$pem" >&2
    status=1
fi

[[ "$status" == 0 ]] && echo "check-no-signing-keys: ok"
exit "$status"
