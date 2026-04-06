#!/usr/bin/env bash
set -e

cd "$(dirname "$0")/.."

cmake --build build-debug --target generate_messages

rm -f data/data.bin

build-debug/tools/generate_messages
