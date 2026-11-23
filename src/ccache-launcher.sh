#!/bin/sh
# CMake can retain the launcher path after a build with USE_CCACHE=1.
if [ -n "${GRISP_CCACHE:-}" ]; then
    exec "$GRISP_CCACHE" "$@"
fi
exec "$@"
