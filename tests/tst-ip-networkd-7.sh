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
    ip=192.168.0.10:192.168.0.2:192.168.0.1:255.255.255.0:hogehoge:eth0:on

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-ip-networkd-07"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
