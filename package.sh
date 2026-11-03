#!/usr/bin/env sh
set -eu

ROOT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)

chmod +x "$ROOT_DIR/rootkit/install" "$ROOT_DIR/rootkit/elevate" "$ROOT_DIR/rootkit/remote" "$ROOT_DIR/rootkit/log" "$ROOT_DIR/detector/detect"
tar -cf "$ROOT_DIR/rootkit.tar" -C "$ROOT_DIR/rootkit" .
tar -cf "$ROOT_DIR/detector.tar" -C "$ROOT_DIR/detector" .

echo "Created rootkit.tar and detector.tar"
