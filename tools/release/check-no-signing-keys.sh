#!/usr/bin/env bash
# Fail if a signing key or a secret could reach the public repository.
#
# Usage:
#   tools/release/check-no-signing-keys.sh                # the committed tree
#   tools/release/check-no-signing-keys.sh --staged       # the index, before a commit
#   tools/release/check-no-signing-keys.sh --message FILE # one commit message
#
# The repository is public, so a secret that reaches a commit is published the
# moment that commit is pushed. CI runs this on every push, and a push is too
# late: the object is already on the remote and has to be rewritten out of the
# history. The pre-commit and commit-msg hooks in tools/release/hooks/ run it
# against the index instead, which is the last point where the commit can still
# be stopped.
#
# Checks:
#   1. no key store (*.p12, *.jks, *.keystore, *.pfx) or file under .signing/
#      or .vita-plus-signing/ is tracked or staged
#   2. those folders are ignored by git
#   3. no PEM private key is in the content, outside external/
#   4. no password from .signing/release/signing.env is in the content, and no
#      private key is in the commit message
#
# Nothing here ever prints a secret. A failure names the file and the variable,
# never the value.
#
# CI runs the tree mode on every push (.github/workflows/no-signing-keys.yml).
# Run `bash tools/release/install-hooks.sh` once per clone to get the local gate.
set -euo pipefail

cd "$(git rev-parse --show-toplevel)"

mode="tree"
message_file=""
while [[ $# -gt 0 ]]; do
    case "$1" in
        --staged) mode="staged" ;;
        --message)
            mode="message"
            message_file="${2:?--message needs a file}"
            shift
            ;;
        -h | --help)
            sed -n '2,/^set -euo/p' "$0" | sed '$d' | sed 's/^# \{0,1\}//'
            exit 0
            ;;
        *)
            echo "check-no-signing-keys: unknown argument $1" >&2
            exit 2
            ;;
    esac
    shift
done

status=0
report() {
    echo "check-no-signing-keys: $1" >&2
    status=1
}

# The file list to inspect: the index for --staged, the tree otherwise.
list_files() {
    if [[ "$mode" == "staged" ]]; then
        git diff --cached --name-only --diff-filter=ACMR
    else
        git ls-files
    fi
}

# Search content: the index for --staged, the working tree otherwise.
# external/ is a set of pinned third-party trees and is not searched, which is
# what the CI check has always done.
search() {
    local pattern="$1"
    if [[ "$mode" == "staged" ]]; then
        git grep --cached -lE -e "$pattern" -- . ':!external' 2> /dev/null || true
    else
        git grep -lE -e "$pattern" -- . ':!external' 2> /dev/null || true
    fi
}

search_literal() {
    local needle="$1"
    if [[ "$mode" == "staged" ]]; then
        git grep --cached -lF -e "$needle" -- . ':!external' 2> /dev/null || true
    else
        git grep -lF -e "$needle" -- . ':!external' 2> /dev/null || true
    fi
}
signing_env=".signing/release/signing.env"

# What the failure message calls the place the value was found. Set per mode so
# the message names a file list or the commit message.
where="these files"

# Read the release signing passwords and check each value against the content.
# The value is never printed: a failure names the variable only.
#
# A clone without .signing/ has no passwords to look for, so the check is skipped
# rather than failed. That is the normal case on CI and on a fresh clone, and a
# gate that blocks every commit there would be turned off within a day.
#
# `printf` instead of `read < file` so a missing file is not an error, and so the
# loop cannot exit the script: under `set -e` a `[[ ... ]] && continue` whose test
# is false returns 1, which ends the script unless the command is in a condition.
check_signing_values() {
    local name value hits
    [[ -r "$signing_env" ]] || return 0
    while IFS='=' read -r name value || [[ -n "$name" ]]; do
        if [[ "$name" =~ ^[[:space:]]*# ]]; then
            continue
        fi
        name="${name#"${name%%[![:space:]]*}"}"
        if [[ "$name" != "SIGNING_"* ]]; then
            continue
        fi
        value="${value#\"}"
        value="${value%\"}"
        # A short value would match ordinary text. These passwords are long.
        if (( ${#value} < 8 )); then
            continue
        fi
        hits="$(search_literal "$value")"
        if [[ -n "$hits" ]]; then
            echo "check-no-signing-keys: the value of $name is in $where:" >&2
            # The hits name the files in tree mode. In message mode they are the
            # path of git's temporary message file, which tells the reader nothing.
            if [[ "$mode" != "message" ]]; then
                echo "$hits" >&2
            fi
            status=1
        fi
    done < <(cat "$signing_env" 2> /dev/null)
}

if [[ "$mode" == "message" ]]; then
    # A commit message is not a file, so only the content checks apply. A message
    # is the easiest place for an agent to leak a password it read a moment
    # earlier, because nothing in the message looks like a file.
    [[ -r "$message_file" ]] || {
        echo "check-no-signing-keys: cannot read $message_file" >&2
        exit 2
    }
    if grep -qE -e '-----BEGIN ([A-Z]+ )?PRIVATE KEY-----' "$message_file"; then
        echo "check-no-signing-keys: the commit message holds a PEM private key." >&2
        echo "check-no-signing-keys: remove it and amend. Do not print it anywhere." >&2
        status=1
    fi
    # The message is a plain file, not the repository, so the search is a file read
    # rather than a git grep. Redefining the one helper keeps the needle loop in a
    # single place instead of copied into both modes.
    where="the commit message"
    search_literal() { grep -lF -e "$1" "$message_file" 2> /dev/null || true; }
    check_signing_values
    unset -f search_literal
    [[ "$status" == 0 ]] && echo "check-no-signing-keys: ok"
    exit "$status"
fi

# 1. Key stores and key folders.
bad_names="$(list_files | grep -E '\.(p12|jks|keystore|pfx)$|(^|/)\.signing/|(^|/)\.vita-plus-signing/' || true)"
if [[ -n "$bad_names" ]]; then
    echo "check-no-signing-keys: these key files would be recorded by git:" >&2
    echo "$bad_names" >&2
    status=1
fi

# 2. The ignore rules. .vita-plus-signing/ holds the release key itself:
# .signing/release/vita-plus-release.p12 is a symlink into it, so the check has
# to cover both folders or it misses the key when one rule is dropped.
for path in .signing/release/signing.env .signing/release/vita-plus-release.p12 \
    .signing/dev/debug.keystore .vita-plus-signing/vita-plus-release.p12; do
    if ! git check-ignore -q "$path"; then
        report "$path is not ignored by git. Fix .gitignore."
    fi
done

# 3. A PEM private key in the content.
pem="$(search '-----BEGIN ([A-Z]+ )?PRIVATE KEY-----')"
if [[ -n "$pem" ]]; then
    echo "check-no-signing-keys: a PEM private key is in these files:" >&2
    echo "$pem" >&2
    status=1
fi

# 4. A signing password in the content. This is the mistake CLAUDE.md warns
#    about, and it is the one an agent makes most easily, because the
#    password is on screen right after reading the signing.env file.
check_signing_values

# 5. A password on a command line, in a script or a ticket. The variable is
#    assigned from a literal rather than from a file or the environment.
assign="$(search '(PASSWORD|PASSWD|SECRET|TOKEN|API_KEY)=[A-Za-z0-9+/=_-]{8,}')"
if [[ -n "$assign" ]]; then
    echo "check-no-signing-keys: a secret is assigned from a literal in these files:" >&2
    echo "$assign" >&2
    status=1
fi

[[ "$status" == 0 ]] && echo "check-no-signing-keys: ok"
exit "$status"
