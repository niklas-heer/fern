#!/bin/sh
# Compatibility entry point: stage a package without changing any editor profile.
set -eu
SCRIPT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
exec python3 "$SCRIPT_DIR/../scripts/package_zed.py" "$@"
