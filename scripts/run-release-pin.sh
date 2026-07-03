#!/bin/bash
set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$SCRIPT_DIR/.."

TIMESTAMP=$(date +%Y%m%d_%H%M%S)
OUTFILE="$ROOT/results/run_pin_$TIMESTAMP.txt"

cd "$ROOT"

"$ROOT/build-release/src/market_data_feed" --pin | tee "$OUTFILE"

echo "Results saved to $OUTFILE"
