#!/usr/bin/env bash
# Copy libpotrace .c/.h into libs/potrace for a fully flat vendor tree.
# Uses libs/potrace-upstream (git submodule, or shallow clone if missing).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
UPSTREAM="${ROOT}/libs/potrace-upstream"
SRC="${UPSTREAM}/src"
DEST="${ROOT}/libs/potrace"

ensure_upstream() {
	if [[ -f "${SRC}/potracelib.c" ]]; then
		return 0
	fi

	# Try git submodule init when this addon is a git checkout with a registered submodule.
	if [[ -f "${ROOT}/.gitmodules" ]]; then
		if git -C "${ROOT}" rev-parse --git-dir >/dev/null 2>&1; then
			echo "Initializing submodule libs/potrace-upstream..."
			git -C "${ROOT}" submodule update --init --recursive libs/potrace-upstream 2>/dev/null \
				|| git -C "${ROOT}" submodule update --init --recursive
		fi
	fi

	if [[ -f "${SRC}/potracelib.c" ]]; then
		return 0
	fi

	# .gitmodules alone is not enough — clone upstream directly (no parent-repo submodule entry).
	echo "Submodule not present; cloning https://github.com/skyrpex/potrace (depth 1)..."
	rm -rf "${UPSTREAM}"
	git clone --depth 1 https://github.com/skyrpex/potrace.git "${UPSTREAM}"
}

ensure_upstream

if [[ ! -f "${SRC}/potracelib.c" ]]; then
	echo "error: expected ${SRC}/potracelib.c after clone" >&2
	exit 1
fi

FILES=(
	potracelib.h potracelib.c
	curve.h curve.c
	trace.h trace.c
	decompose.h decompose.c
	lists.h auxiliary.h bitmap.h progress.h
)

mkdir -p "${DEST}"

for f in "${FILES[@]}"; do
	cp -f "${SRC}/${f}" "${DEST}/${f}"
done

cp -f "${UPSTREAM}/COPYING" "${DEST}/COPYING"

echo "Synced flat vendor to ${DEST}"
ls -la "${DEST}"
