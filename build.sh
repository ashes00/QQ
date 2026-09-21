#!/usr/bin/env bash
set -euo pipefail

cd "$(dirname "$0")"

APP_NAME=$(grep '_APP_NAME' src/version.h | sed -E 's/.*"(.*)".*/\1/')
VERSION=$(grep '_VERSION' src/version.h | sed -E 's/.*"(.*)".*/\1/')

rm -rf build
mkdir build
cd build
cmake ..
make -j"$(nproc)"
cd ..

# build/qq is the disposable day-to-day test command. It lives in build/,
# not dist/, specifically because build/ is wiped and recreated from
# scratch on every run (see above) — so this copy can never go stale or
# hit a "text file busy" failure the way a persistent dist/ file could
# (that happened once for real; see docs/RELEASE-NOTES.md v0.0.3).
cp "build/${APP_NAME}_v${VERSION}" "build/qq"
echo "Built ${APP_NAME} v${VERSION} -> build/qq (test command)"

# --distribute archives this exact build as the permanent, versioned release
# file in dist/. Only run this after the version has been bumped and testing
# has been explicitly approved (Sprint step 7, after step 6's version bump)
# — never as part of routine test-builds, or it will try to re-archive
# whatever version is currently in src/version.h every time.
if [[ "${1:-}" == "--distribute" ]]; then
    mkdir -p dist
    ARCHIVE="dist/${APP_NAME}-v-${VERSION}"
    if [ -e "$ARCHIVE" ]; then
        echo "Error: $ARCHIVE already exists — refusing to overwrite a released version." >&2
        echo "Bump the version in src/version.h if you meant to release something new." >&2
        exit 1
    fi
    cp "build/${APP_NAME}_v${VERSION}" "$ARCHIVE"
    echo "Archived ${APP_NAME} v${VERSION} -> $ARCHIVE"
fi
