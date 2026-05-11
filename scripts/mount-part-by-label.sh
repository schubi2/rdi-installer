#!/bin/bash

TARGET_LABEL=${1:-"images"}

# Find all partitions with the filesystem label
DEVICES=$(blkid -t LABEL="${TARGET_LABEL}" -o device)

if [ -z "$DEVICES" ]; then
    echo "No partitions with label \"${TARGET_LABEL}\" found."
    exit 0
fi

for DEV in $DEVICES; do
    DEV_NAME=$(basename "$DEV")
    MOUNTPOINT="/images/$DEV_NAME"
    mkdir -p "$MOUNTPOINT"
    mount "$DEV" "$MOUNTPOINT"
    echo "Successfully mounted $DEV to $MOUNTPOINT"
done
