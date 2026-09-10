#!/bin/bash

set -e

# Source shared test utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Initialize temp dir and register exit trap
TEMPDIR=$(mktemp -d)
enable_cleanup_trap

# Execute networkd test command
./rdii-networkd -o "$TEMPDIR" -a rd.peerdns=0

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-peerdns-networkd-1"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
