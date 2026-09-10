#!/bin/bash

set -e

# Source shared test utilities
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/test_utils.sh"

# Initialize temp dir and register exit trap
TEMPDIR=$(mktemp -d)
enable_cleanup_trap

# Execute networkd test command
./rdii-networkd -o "$TEMPDIR" \
		"ifcfg=*=dhcp" \
		"ifcfg=00:11:22:33:44:55=dhcp,rfc2132" \
		ifcfg='"eth1=192.168.0.2/24 192.158.10.12/24,192.168.0.1,8.8.8.8,mydomain.com"'

# Run bidirectional assertion against reference directory
REF_DIR="../tests/tst-ifcfg-networkd-1"
assert_dirs_match "$TEMPDIR" "$REF_DIR"
