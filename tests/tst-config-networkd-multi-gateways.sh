#!/bin/bash

set -e

cleanup()
{
    local exit_code=$?

    if [ -n "$TEMPDIR" ] && [ -d "$TEMPDIR" ]; then
        rm -rf "$TEMPDIR"
    fi

    exit $exit_code
}

trap cleanup EXIT


TEMPDIR=$(mktemp -d)

./rdii-networkd -o "$TEMPDIR" -c ../tests/tst-config-networkd-multi-gateways.rdii-config

for cfg in "${TEMPDIR}"/*; do
    cfg=$(basename "$cfg")
    if ! cmp "$TEMPDIR/$cfg" "../tests/tst-config-networkd-multi-gateways/$cfg" ; then
       diff -u "../tests/tst-config-networkd-multi-gateways/$cfg" "$TEMPDIR/$cfg"
       exit 1
    fi
done
