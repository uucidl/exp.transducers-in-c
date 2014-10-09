#!/usr/bin/env sh
HERE="$(dirname ${0})"
BUILD="${HERE}/builds"
[ -d "${BUILD}" ] || mkdir -p "${BUILD}"
"${HERE}"/build.sh && "${HERE}"/builds/"$(hostname)"/main
