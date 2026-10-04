#!/usr/bin/env bash
# SPDX-License-Identifier: BSD-3-Clause
#
# Checks the license boundary between the BSD-3-Clause tree (include/, source/)
# and the GPL-3.0-or-later tree (gpl/).
#
#   1. Every .c/.h file has an SPDX-License-Identifier line near the top.
#   2. Files under include/ and source/ use a BSD-3-Clause expression
#      ("BSD-3-Clause" or "BSD-3-Clause AND <permissive>"), never GPL.
#   3. Files under gpl/ use GPL-3.0-or-later.
#   4. Files under include/ and source/ never include a header that exists
#      only under gpl/include.
#
# Usage: tools/check_license_boundary.sh [pif root]
# Exits with 1 if any check fails.

set -u

ROOT="${1:-$(cd "$(dirname "$0")/.." && pwd)}"
cd "$ROOT" || exit 2

# Licenses that may be combined with BSD-3-Clause in the BSD tree.
BSD_ALLOWED='^BSD-3-Clause( AND (MIT|BSD-2-Clause|BSD-3-Clause|Apache-2\.0|Zlib))*$'
GPL_ALLOWED='^GPL-3\.0-or-later( AND (MIT|BSD-2-Clause|BSD-3-Clause|Apache-2\.0|Zlib|GPL-3\.0-only|LGPL-2\.1-or-later))*$'

errors=0

fail()
{
	echo "ERROR: $*" >&2
	errors=$((errors + 1))
}

# Prints the SPDX expression of a file, or nothing if it has none.
spdx_of()
{
	head -n 5 "$1" | tr -d '\r' | sed -n 's/.*SPDX-License-Identifier:[[:space:]]*\(.*[^[:space:]]\)[[:space:]]*$/\1/p' | head -n 1
}

check_tree()
{
	local pattern="$1"; shift
	local f expr
	while IFS= read -r f; do
		expr="$(spdx_of "$f")"
		if [ -z "$expr" ]; then
			fail "$f: missing SPDX-License-Identifier"
		elif ! [[ "$expr" =~ $pattern ]]; then
			fail "$f: license '$expr' is not allowed in this directory"
		fi
	done < <(find "$@" -type f \( -name '*.c' -o -name '*.h' \) 2>/dev/null | sort)
}

# 1-3. SPDX identifiers
check_tree "$BSD_ALLOWED" include source
[ -d gpl ] && check_tree "$GPL_ALLOWED" gpl

# 4. BSD files must not include GPL-only headers
if [ -d gpl/include ]; then
	declare -A gpl_only=()
	while IFS= read -r h; do
		h="${h#gpl/include/}"
		[ -e "include/$h" ] || gpl_only["$h"]=1
	done < <(find gpl/include -type f -name '*.h')

	while IFS= read -r line; do
		file="${line%%:*}"
		rest="${line#*:}"
		lineno="${rest%%:*}"
		hdr="$(printf '%s' "${rest#*:}" | tr -d '\r' | sed -n 's/^[[:space:]]*#[[:space:]]*include[[:space:]]*[<"]\([^>"]*\)[>"].*/\1/p')"
		[ -n "$hdr" ] || continue
		hdr="${hdr#./}"
		if [ -n "${gpl_only[$hdr]+x}" ]; then
			fail "$file:$lineno: BSD file includes GPL header \"$hdr\""
		fi
	done < <(grep -rnE '^[[:space:]]*#[[:space:]]*include' include source --include='*.c' --include='*.h')
fi

if [ "$errors" -ne 0 ]; then
	echo "License boundary check failed: $errors error(s)." >&2
	exit 1
fi
echo "License boundary check passed."
