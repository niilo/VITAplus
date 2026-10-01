#!/usr/bin/env bash
# Store the VITA+ Android release signing key in 1Password, check the copy, and
# then (if you agree) delete the local files.
#
# Usage: tools/release/store-signing-key-in-1password.sh [vault name]
#
# It reads the files in ~/.vita-plus-signing (or the folder in VITA_SIGNING_DIR):
#   vita-plus-release.p12   the key store
#   store-password.txt      the store and key password (one password for both)
#   key-alias.txt           the key alias
#
# It makes one Secure Note item with these fields: keystore (base64), store
# password, key password, key alias. The values go from files to the 1Password
# CLI. They are not printed and not put in a command line.
#
# Needs the 1Password CLI: brew install --cask 1password-cli
# Then open the 1Password app: Settings > Developer > "Integrate with 1Password
# CLI". Check with: op whoami
set -euo pipefail

dir="${VITA_SIGNING_DIR:-$HOME/.vita-plus-signing}"
vault="${1:-}"
title="VITA+ Android release signing key"
vault_args=()
[[ -n "$vault" ]] && vault_args=(--vault "$vault")

die() {
    echo "store-signing-key-in-1password.sh: $*" >&2
    exit 1
}

command -v op > /dev/null || die "the 1Password CLI (op) is not installed. Run: brew install --cask 1password-cli"
command -v python3 > /dev/null || die "python3 is needed"
op whoami > /dev/null 2>&1 || die "op is not signed in. Open the 1Password app, turn on Settings > Developer > Integrate with 1Password CLI, and try again."

for f in vita-plus-release.p12 store-password.txt key-alias.txt; do
    [[ -s "$dir/$f" ]] || die "missing or empty file: $dir/$f"
done

if op item get "$title" "${vault_args[@]}" > /dev/null 2>&1; then
    die "an item named '$title' already exists in 1Password. Check it or rename it first. Nothing was changed."
fi

umask 077
template="$(mktemp "$dir/template.XXXXXX")"
trap 'rm -f "$template"' EXIT

# Build the item template from the files. Python reads the files itself, so no
# secret goes through a command line.
DIR="$dir" TITLE="$title" python3 - > "$template" << 'PY'
import base64, json, os
d, title = os.environ["DIR"], os.environ["TITLE"]
def read(name):
    with open(os.path.join(d, name), "rb") as f:
        return f.read()
note = ("Release signing key for the VITA+ Android app (org.vita3k.emulator).\n"
        "GitHub repository secrets: KEYSTORE (the base64 field), SIGNING_STORE_PASSWORD, "
        "SIGNING_KEY_PASSWORD, SIGNING_KEY_ALIAS. See docs/release.md in the repository.\n"
        "Keystore type PKCS12, RSA 4096. If this key is lost, installed apps cannot be updated.")
item = {
    "title": title,
    "category": "SECURE_NOTE",
    "fields": [
        {"id": "notesPlain", "type": "STRING", "purpose": "NOTES", "label": "notesPlain", "value": note},
        {"label": "keystore (base64)", "type": "CONCEALED", "value": base64.b64encode(read("vita-plus-release.p12")).decode()},
        {"label": "store password", "type": "CONCEALED", "value": read("store-password.txt").decode()},
        {"label": "key password", "type": "CONCEALED", "value": read("store-password.txt").decode()},
        {"label": "key alias", "type": "STRING", "value": read("key-alias.txt").decode()},
    ],
}
print(json.dumps(item))
PY

op item create --template "$template" "${vault_args[@]}" > /dev/null
rm -f "$template"
echo "Item created: $title"

# Read the values back and compare them with the local files.
field() { op item get "$title" "${vault_args[@]}" --fields "label=$1" --reveal; }
hash() { shasum -a 256 | cut -d' ' -f1; }
ok=1
[[ "$(field 'store password' | tr -d '\n' | hash)" == "$(tr -d '\n' < "$dir/store-password.txt" | hash)" ]] || { echo "MISMATCH: store password" >&2; ok=0; }
[[ "$(field 'key password' | tr -d '\n' | hash)" == "$(tr -d '\n' < "$dir/store-password.txt" | hash)" ]] || { echo "MISMATCH: key password" >&2; ok=0; }
[[ "$(field 'key alias' | tr -d '\n')" == "$(tr -d '\n' < "$dir/key-alias.txt")" ]] || { echo "MISMATCH: key alias" >&2; ok=0; }
[[ "$(field 'keystore (base64)' | tr -d '\n' | base64 --decode | hash)" == "$(hash < "$dir/vita-plus-release.p12")" ]] || { echo "MISMATCH: key store" >&2; ok=0; }

if [[ "$ok" != 1 ]]; then
    die "the copy in 1Password does not match. The local files are kept. Delete the 1Password item '$title' and try again."
fi
echo "Checked: all four values in 1Password match the local files."

read -r -p "Delete the local copy in $dir now? [y/N] " answer
if [[ "$answer" == y || "$answer" == Y ]]; then
    # rm -P overwrites the file once. On an SSD with APFS this is no guarantee,
    # but the files are small and the folder is private.
    for f in vita-plus-release.p12 store-password.txt key-alias.txt; do
        rm -P "$dir/$f" 2> /dev/null || rm -f "$dir/$f"
    done
    rmdir "$dir" 2> /dev/null || true
    echo "Local copy deleted."
else
    echo "Local copy kept in $dir. Delete it yourself when you are done: rm -rP $dir"
fi
