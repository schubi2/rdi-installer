#!/bin/bash

set -e

# Source shared test utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Initialize temp dir and register exit trap
TEMPDIR=$(mktemp -d)
enable_cleanup_trap

# Execute networkd test command
./rdii-networkd -o "$TEMPDIR" -a rd.route=10.1.2.3/16:10.0.2.3:eth0

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-route-networkd-2"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
