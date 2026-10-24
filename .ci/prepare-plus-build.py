#!/usr/bin/env python3
"""Supply the untracked build identity header required by Plus logging."""

from pathlib import Path

header = Path("vita3k/util/include/util/fork_build.h")
content = '#pragma once\n#define FORK_BUILD_SEQ 0\n#define FORK_BUILD_CHANGES "niilo/VITAplus experiment"\n'
if header.exists():
    if header.read_text() != content:
        raise SystemExit("A different local fork_build.h exists; preserve it before preparing this build")
else:
    header.write_text(content)
print("Prepared the Plus logging identity header")
