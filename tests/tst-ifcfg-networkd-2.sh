#!/bin/bash

# Test vlans

set -e

# Source shared test utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Initialize temp dir and register exit trap
TEMPDIR=$(mktemp -d)
enable_cleanup_trap

# Execute networkd test command
./rdii-networkd -o "$TEMPDIR" \
    "ifcfg=eth0.66=10.0.1.1/24,10.0.1.254" \
    "ifcfg=eth0.67=dhcp" \
    "ifcfg=eth1.33=dhcp"

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-ifcfg-networkd-2"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
