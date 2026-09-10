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
    ip=[2001:1234:56:8f63::10]::[2001:1234:56:8f63::1]:64:hogehoge:eth0:dhcp6

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-ip-networkd-15"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
