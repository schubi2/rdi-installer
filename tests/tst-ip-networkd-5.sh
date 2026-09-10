#!/bin/bash

set -e

# Source shared test utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Initialize temp dir and register exit trap
TEMPDIR=$(mktemp -d)
enable_cleanup_trap

# Execute networkd test command
./rdii-networkd -o "$TEMPDIR" -a \
    ip=10.99.37.44::10.99.10.1:255.255.0.0::eth0:off

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-ip-networkd-05"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
