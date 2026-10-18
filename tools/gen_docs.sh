#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
#
# Builds the PIF manual with Doxygen.
#
# Usage: tools/gen_docs.sh [--open]
#
#   --open   Open docs/build/html/index.html when the build is done.
#
# Doxygen is taken from $DOXYGEN, then from the PATH, then from the default
# Windows install folder.
# The output goes to docs/build/html, the warnings to docs/build/warnings.log.

set -u

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT" || exit 2

DOXYGEN="${DOXYGEN:-doxygen}"
# Git Bash on Windows: the installer does not always put Doxygen on the PATH.
if ! command -v "$DOXYGEN" >/dev/null 2>&1 && [ -x "/c/Program Files/doxygen/bin/doxygen.exe" ]; then
	DOXYGEN="/c/Program Files/doxygen/bin/doxygen.exe"
fi
if ! command -v "$DOXYGEN" >/dev/null 2>&1; then
	echo "ERROR: doxygen not found. Install it from https://www.doxygen.nl or set DOXYGEN." >&2
	exit 2
fi

PIF_DOC_VERSION="$(git describe --tags --always --dirty 2>/dev/null || true)"
export PIF_DOC_VERSION

rm -rf docs/build
mkdir -p docs/build
"$DOXYGEN" docs/Doxyfile || exit 1

warnings=0
[ -s docs/build/warnings.log ] && warnings="$(grep -c 'warning:' docs/build/warnings.log)"
echo "Manual: docs/build/html/index.html ($warnings warning(s), see docs/build/warnings.log)"

if [ "${1:-}" = "--open" ]; then
	index="docs/build/html/index.html"
	case "$(uname -s)" in
		MINGW*|MSYS*|CYGWIN*) start "" "$index" ;;
		Darwin*) open "$index" ;;
		*) xdg-open "$index" >/dev/null 2>&1 & ;;
	esac
fi
