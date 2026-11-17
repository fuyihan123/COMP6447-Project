#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/comp6447-package.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

mkdir -p "$STAGE/rootkit" "$STAGE/detector"
cp "$ROOT_DIR/rootkit/Makefile" "$ROOT_DIR/rootkit/rootkit.c" \
    "$ROOT_DIR/rootkit/install" "$ROOT_DIR/rootkit/elevate" \
    "$ROOT_DIR/rootkit/remote" "$ROOT_DIR/rootkit/log" "$STAGE/rootkit/"
cp "$ROOT_DIR/detector/detect" "$ROOT_DIR/detector/README.md" \
    "$ROOT_DIR/detector/probe.c" "$STAGE/detector/"
if [ -f "$ROOT_DIR/detector/probe" ]; then
    cp "$ROOT_DIR/detector/probe" "$STAGE/detector/probe"
fi

# Set archive permissions explicitly so Windows/DrvFs defaults cannot make
# source files executable or strip the entry-point execute bit.
chmod 0644 "$STAGE/rootkit/Makefile" "$STAGE/rootkit/rootkit.c" \
    "$STAGE/detector/README.md" "$STAGE/detector/probe.c"
chmod 0755 "$STAGE/rootkit/install" "$STAGE/rootkit/elevate" \
    "$STAGE/rootkit/remote" "$STAGE/rootkit/log" "$STAGE/detector/detect"
if [ -f "$STAGE/detector/probe" ]; then
    chmod 0755 "$STAGE/detector/probe"
fi
chmod 0755 "$STAGE/rootkit" "$STAGE/detector"

tar -cf "$ROOT_DIR/rootkit.tar" -C "$STAGE/rootkit" .
tar -cf "$ROOT_DIR/detector.tar" -C "$STAGE/detector" .

echo "Created rootkit.tar and detector.tar"
