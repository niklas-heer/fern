# Snapshot materialization and immutable cache publication; this file is authored shell source.

# Copy only the validated source list into a private tree, preserving literal relative paths.
style_snapshot_create() {
    local path relative parent
    style_snapshot=$style_work/snapshot
    mkdir -m 700 "$style_snapshot" || return 125
    for path in "${style_sources[@]}"; do
        relative=${path#"$style_root"/}
        [[ $relative != "$path" && $relative != ../* ]] || return 125
        parent=$style_snapshot/${relative%/*}
        [[ $relative == */* ]] || parent=$style_snapshot
        mkdir -p -- "$parent" || return 125
        /bin/cp -p -- "$path" "$style_snapshot/$relative" || return 125
    done
    mkdir -m 700 "$style_snapshot/build" "$style_snapshot/bin" || return 125
    : > "$style_snapshot/build/driver.cfg" || return 125
    chmod 400 "$style_snapshot/build/driver.cfg" || return 125
}

# Bound initial trusted compiler CPU and each regular output; limits stay in this subshell.
# This first helper has no supervisor yet: an OS-blocked compiler can extend wall time.
style_seed_compile() (
    ulimit -t 30 || exit 125
    ulimit -f 2048 || exit 125
    cd -P -- "$style_snapshot" || exit 125
    style_driver_run "$style_snapshot/build/driver.cfg" "$style_cc" -std=c11 -O2 -Wall -Wextra -Werror -MD -MF build/seed.d -MT fern-object -H \
        -c scripts/bootstrap/style_supervisor.c -o build/seed.o \
        > build/seed.stdout 2> build/seed.includes || { /usr/bin/tail -c 4096 build/seed.includes >&2; exit 125; }
    style_driver_run "$style_snapshot/build/driver.cfg" "$style_cc" build/seed.o -Wl,-t -o bin/supervisor > bin/supervisor.link.paths || exit 125
)

# Compile metadata decoding under the controller, retaining both independent header inventories.
style_metadata_compile() {
    local file
    cd -P -- "$style_snapshot" || return 125
    style_driver_run "$style_snapshot/build/driver.cfg" "$style_cc" -std=c11 -O2 -Wall -Wextra -Werror -MD -MF build/metadata.d -MT fern-object -H \
        -c scripts/bootstrap/style_metadata.c -o build/metadata.o \
        > build/metadata.stdout 2> build/metadata.includes || return 125
    style_driver_run "$style_snapshot/build/driver.cfg" "$style_cc" build/metadata.o -Wl,-t -o bin/metadata > bin/metadata.link.paths || return 125
    style_metadata=$style_snapshot/bin/metadata
    for file in seed metadata; do
        "$style_metadata" deps < "build/$file.d" > "build/$file.dep.paths" || return 125
        "$style_metadata" includes < "build/$file.includes" > "build/$file.include.paths" || return 125
    done
}

# Write all fixed artifact digests before atomic publication; incomplete folders never qualify.
style_publish() {
    local stage=$style_work/stage entry file digest
    mkdir -m 700 "$stage" || return 125
    /bin/cp -- "$style_snapshot/bin/check_style" "$stage/program" || return 125
    /bin/cp -- "$style_snapshot/bin/supervisor" "$stage/supervisor" || return 125
    /bin/cp -- "$style_snapshot/bin/metadata" "$stage/metadata" || return 125
    printf 'FERN_STYLE_ARTIFACTS_V1\n' > "$stage/artifacts" || return 125
    for file in program supervisor metadata; do
        chmod 500 "$stage/$file" || return 125
        digest=$("$style_openssl" dgst -sha256 -r "$stage/$file") || return 125
        printf '%s\n' "${digest:0:64}" > "$stage/$file.sha256" || return 125
    done
    for file in base dependencies search-roots search; do
        /bin/cp -- "$style_work/$file" "$stage/$file" || return 125
    done
    printf 'FERN_STYLE_ENTRY_V1\n' > "$stage/owner" || return 125
    style_bundle_digest "$stage" "$stage/bundle" || return 125
    entry=$(mktemp -d "$style_cache/entry.XXXXXXXX") || return 125
    /bin/mv -- "$stage" "$entry/ready" || return 125
    style_retain "$entry/ready"
}

# Complete bounded literal handoff only after the selected executable is retained and validated.
style_handoff() {
    local control=$1
    {
        printf '%s\n' "$style_run" &&
        style_identity "$style_run" &&
        style_identity "$style_run/program" &&
        style_identity "$style_run/supervisor"
    } > "$control/handoff.tmp" || return 125
    /bin/mv -- "$control/handoff.tmp" "$control/handoff" || return 125
}

# Check copied bytes against the source rows selected before copying; a mixed snapshot never builds.
style_snapshot_verify() {
    local path line digest selected
    local copies=()
    for path in "${style_sources[@]}"; do copies+=("$style_snapshot/${path#"$style_root"/}"); done
    style_hash_files "$style_work/copied" "${copies[@]}" || return 125
    : > "$style_work/copied.logical" || return 125
    while IFS= read -r line; do
        digest=${line:0:64}; selected=${line:65}
        [[ $selected == "$style_snapshot"/* ]] || return 125
        printf '%s %s/%s\n' "$digest" "$style_root" "${selected#"$style_snapshot"/}" >> "$style_work/copied.logical" || return 125
    done < "$style_work/copied"
    /usr/bin/head -n "${#style_sources[@]}" "$style_work/base" > "$style_work/selected" || return 125
    /usr/bin/cmp -s "$style_work/copied.logical" "$style_work/selected"
}

# Permit one fresh snapshot for changing inputs; the marker is bounded data, never a PID lock.
style_restart_snapshot() {
    if [[ -e $style_control/retry ]]; then
        printf 'fern style: inputs changed during both build attempts\n' >&2
        return 125
    fi
    printf '1\n' > "$style_control/retry" || return 125
    [[ $style_work == "$style_control/payload" && -d $style_work && ! -L $style_work ]] || return 125
    cd -P -- "$style_root" || return 125
    /bin/rm -rf -- "$style_work" || return 125
    mkdir -m 700 "$style_work" || return 125
    style_payload_identity=$(style_identity "$style_work") || return 125
    style_base_manifest "$style_work/base" || return 125
    style_snapshot_create || return 125
    style_snapshot_verify || return 125
}

# Replace only the supervised worker with the new snapshot's authored code after the one retry.
style_retry_worker() {
    style_restart_snapshot || return 125
    style_seed_compile < /dev/null || return 125
    exec /bin/bash "$style_snapshot/scripts/bootstrap/style_worker" "$style_control" \
        "$style_root" "$style_requested_cache" "$style_tool_path"
    return 125
}
