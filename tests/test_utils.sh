#!/bin/bash

# Shared utility library for test scripts

# Cleanup temporary directory on script exit/signals
cleanup() {
    local exit_code=$?

    if [ -n "${TEMPDIR:-}" ] && [ -d "$TEMPDIR" ]; then
        rm -rf "$TEMPDIR"
    fi

    exit "$exit_code"
}

# Call this function after creating TEMPDIR to enable auto-cleanup
enable_cleanup_trap() {
    trap cleanup EXIT INT TERM
}

# Compare generated directory ($1) against reference directory ($2)
# Fails with exit code 1 if:
# - Either directory has files missing in the other
# - File contents differ
assert_dirs_match() {
    local gen_dir="$1"
    local ref_dir="$2"

    if [ ! -d "$gen_dir" ]; then
        echo "Error: Generated output directory '$gen_dir' does not exist." >&2
        return 1
    fi

    if [ ! -d "$ref_dir" ]; then
        echo "Error: Reference directory '$ref_dir' does not exist." >&2
        return 1
    fi

    # Save initial nullglob state
    local prev_nullglob
    prev_nullglob=$(shopt -p nullglob || true)
    shopt -s nullglob

    local gen_files=("$gen_dir"/*)
    local ref_files=("$ref_dir"/*)

    # Restore initial nullglob state
    eval "$prev_nullglob"

    if [ ${#gen_files[@]} -eq 0 ] && [ ${#ref_files[@]} -eq 0 ]; then
        echo "Warning: Both generated and reference directories are empty." >&2
        return 0
    fi

    # 1. Check for reference files missing in generated directory
    for ref in "${ref_files[@]}"; do
        local cfg
        cfg=$(basename "$ref")
        if [ ! -f "$gen_dir/$cfg" ]; then
            echo "Error: Reference file '$cfg' exists in '$ref_dir' but was not generated in '$gen_dir'" >&2
            return 1
        fi
    done

    # 2. Check generated files against reference directory and compare content
    for gen in "${gen_files[@]}"; do
        local cfg
        cfg=$(basename "$gen")
        local ref_file="$ref_dir/$cfg"

        if [ ! -f "$ref_file" ]; then
            echo "Error: Unexpected file '$cfg' generated in '$gen_dir' (not present in '$ref_dir')" >&2
            return 1
        fi

        if ! cmp -s "$gen" "$ref_file"; then
            echo "Error: Content mismatch in '$cfg':" >&2
            diff -u "$ref_file" "$gen"
            return 1
        fi
    done

    return 0
}
