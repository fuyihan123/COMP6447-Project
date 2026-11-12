#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
STAGE=$(mktemp -d "${TMPDIR:-/tmp}/comp6447-package.XXXXXX")
trap 'rm -rf "$STAGE"' EXIT HUP INT TERM

mkdir -p "$STAGE/rootkit" "$STAGE/detector"
cp "$ROOT_DIR/rootkit/Makefile" "$ROOT_DIR/rootkit/rootkit.c" \
    "$ROOT_DIR/rootkit/install" "$ROOT_DIR/rootkit/elevate" \
    "$ROOT_DIR/rootkit/remote" "$ROOT_DIR/rootkit/log" "$STAGE/rootkit/"
cp "$ROOT_DIR/detector/detect" "$ROOT_DIR/detector/README.md" "$STAGE/detector/"

# Set archive permissions explicitly so Windows/DrvFs defaults cannot make
# source files executable or strip the entry-point execute bit.
chmod 0644 "$STAGE/rootkit/Makefile" "$STAGE/rootkit/rootkit.c" "$STAGE/detector/README.md"
chmod 0755 "$STAGE/rootkit/install" "$STAGE/rootkit/elevate" \
    "$STAGE/rootkit/remote" "$STAGE/rootkit/log" "$STAGE/detector/detect"
chmod 0755 "$STAGE/rootkit" "$STAGE/detector"

tar -cf "$ROOT_DIR/rootkit.tar" -C "$STAGE/rootkit" .
tar -cf "$ROOT_DIR/detector.tar" -C "$STAGE/detector" .

echo "Created rootkit.tar and detector.tar"
