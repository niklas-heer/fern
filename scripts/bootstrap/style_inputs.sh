# Content/ownership validation for the private style cache; all paths remain literal arguments.

# Reject names that cannot be represented unambiguously in the bounded line manifests.
style_path() {
    [[ $1 == /* && ${#1} -le 4096 && $1 != *$'\n'* && $1 != *$'\r'* ]]
}

# Read canonical parent identity while retaining a final symlink's logical lookup name.
style_absolute() {
    local path=$1 parent name
    [[ $path == /* ]] || path=$PWD/$path
    style_path "$path" || return 125
    parent=${path%/*}; name=${path##*/}
    parent=$(cd -P -- "$parent" && pwd) || return 125
    printf '%s/%s\n' "$parent" "$name"
}

# Read a file/directory's uid, octal mode, byte size and native type with fixed fields.
style_stat() {
    if [[ $style_platform == Darwin ]]; then
        "$style_stat_tool" -L -f '%u %Lp %z %HT' "$@"
    else
        "$style_stat_tool" -L -c '%u %a %s %F' "$@"
    fi
}

# Check cache leaf ownership before following children; writable foreign roots never qualify.
style_private() {
    local path=$1 row owner mode size kind
    [[ -d $path && ! -L $path ]] || return 125
    row=$(style_stat "$path") || return 125
    read -r owner mode size kind <<< "$row"
    [[ $owner == "$style_uid" && $mode != *[!0-7]* ]] || return 125
    (( (8#$mode & 077) == 0 ))
}

# Accept only root/user-owned regular immutable inputs; group/world writes fail closed.
style_file_rows() {
    local owner mode size kind total=0 count=0
    style_stat "$@" > "$style_work/stats" || return 125
    while read -r owner mode size kind; do
        [[ $owner == 0 || $owner == "$style_uid" ]] || return 125
        [[ $mode != *[!0-7]* && $size != *[!0-9]* && -n $size ]] || return 125
        [[ $kind == 'Regular File' || $kind == 'regular file' || $kind == 'regular empty file' ]] || return 125
        (( (8#$mode & 022) == 0 && size <= ${style_file_limit:-268435456} )) || return 125
        total=$((total + size)); count=$((count + 1))
        ((total <= 536870912)) || return 125
    done < "$style_work/stats"
    [[ $count == $# ]] || return 125
    style_hashed_bytes=$((style_hashed_bytes + total))
    ((style_hashed_bytes <= 536870912))
}

# Hash a bounded batch in one OpenSSL process, verifying every returned literal filename.
style_hash_batch() {
    local output=$1 line digest index=0 expected
    shift
    style_file_rows "$@" || return 125
    "$style_openssl" dgst -sha256 -r "$@" > "$style_work/hashes" || return 125
    local paths=("$@")
    while IFS= read -r line; do
        ((index < ${#paths[@]})) || return 125
        digest=${line:0:64}; expected=${paths[$index]}
        [[ ${#digest} == 64 && $digest != *[!0-9a-f]* && ${line:64:2} == ' *' && ${line:66} == "$expected" ]] || return 125
        printf '%s %s\n' "$digest" "$expected" >> "$output" || return 125
        index=$((index + 1))
    done < "$style_work/hashes"
    [[ $index == ${#paths[@]} ]]
}

# Batch by both argument count and bytes; never spawn one digest process per header.
style_hash_files() {
    local output=$1 path bytes=0
    shift
    local batch=()
    : > "$output" || return 125
    style_hashed_bytes=0
    for path in "$@"; do
        style_path "$path" || return 125
        if (( ${#batch[@]} >= 256 || bytes + ${#path} + 1 > 65536 )); then
            style_hash_batch "$output" "${batch[@]}" || return 125
            batch=(); bytes=0
        fi
        batch+=("$path"); bytes=$((bytes + ${#path} + 1))
    done
    if (( ${#batch[@]} )); then style_hash_batch "$output" "${batch[@]}" || return 125; fi
}

# Bound metadata file size before reading it into shell data structures.
style_small_file() {
    local row owner mode size kind
    [[ -f $1 && ! -L $1 ]] || return 125
    row=$(style_stat "$1") || return 125
    read -r owner mode size kind <<< "$row"
    [[ $owner == "$style_uid" && -n $size && $size != *[!0-9]* && $mode != *[!0-7]* ]] || return 125
    ((size <= $2 && (8#$mode & 022) == 0))
}

# Select all authored bootstrap/compiler/runtime files; refuse symlinks and runaway snapshots.
style_source_paths() {
    local path relative count=0
    style_sources=()
    "$style_find" "$style_root/src" "$style_root/lib" "$style_root/include" \
        "$style_root/runtime" "$style_root/deps" "$style_root/scripts/bootstrap" \
        "$style_root/compiler-rs/backend" -name .git -prune -o \( -type f -o -type l \) -print0 > "$style_work/source.paths" || return 125
    style_small_file "$style_work/source.paths" 16777216 || return 125
    while IFS= read -r -d '' path; do
        style_path "$path" && [[ -f $path && ! -L $path ]] || return 125
        count=$((count + 1)); ((count <= 4092)) || return 125
        style_sources+=("$path")
    done < "$style_work/source.paths"
    style_sources+=("$style_root/mise.toml" "$style_root/scripts/build_config" "$style_root/scripts/check_style.fn" "$style_root/scripts/check_style")
    printf '%s\n' "${style_sources[@]}" | "$style_sort" > "$style_work/source.sorted" || return 125
    style_sources=()
    while IFS= read -r path; do style_sources+=("$path"); done < "$style_work/source.sorted"
}

# Record effective build configuration without making Python or a compiler a warm-run dependency.
style_environment() {
    local name PATH=$style_tool_path
    : > "$style_work/environment" || return 125
    for name in PATH SDKROOT MACOSX_DEPLOYMENT_TARGET CPATH C_INCLUDE_PATH CPLUS_INCLUDE_PATH \
        OBJC_INCLUDE_PATH LIBRARY_PATH COMPILER_PATH GCC_EXEC_PREFIX PKG_CONFIG_PATH PKG_CONFIG_LIBDIR \
        PKG_CONFIG_SYSROOT_DIR ARCHFLAGS SOURCE_DATE_EPOCH DEVELOPER_DIR FERN_STYLE_CC LD_LIBRARY_PATH LD_PRELOAD DYLD_LIBRARY_PATH \
        DYLD_FALLBACK_LIBRARY_PATH DYLD_INSERT_LIBRARIES; do
        printf '%s\0%s\0%s\0' "$name" "${!name+x}" "${!name-}" >> "$style_work/environment" || return 125
    done
    printf '%s\0%s\0' "$style_platform" "$style_root" >> "$style_work/environment" || return 125
    /bin/bash "$style_root/scripts/build_config" >> "$style_work/environment" || return 125
    for name in bdw-gc sqlite3 openssl; do
        printf '%s\0' "$name" >> "$style_work/environment" || return 125
        "$style_pkg" --cflags --libs "$name" >> "$style_work/environment" || return 125
        "$style_pkg" --variable=libdir "$name" >> "$style_work/environment" || return 125
    done
    if [[ $style_platform == Darwin ]]; then
        /usr/bin/xcrun --show-sdk-path >> "$style_work/environment" || return 125
        /usr/bin/sw_vers >> "$style_work/environment" || return 125
    fi
    /usr/bin/uname -sm >> "$style_work/environment" || return 125
    style_tool_links || return 125
    style_small_file "$style_work/environment" 1048576
}

# Produce the current declared input manifest, independent of mtimes and invocation cwd.
style_base_manifest() {
    local destination=$1 digest
    style_source_paths || return 125
    style_environment || return 125
    style_file_limit=16777216
    style_hash_files "$destination" "${style_sources[@]}" || return 125
    style_file_limit=268435456
    ((style_hashed_bytes <= 67108864)) || return 125
    style_hash_files "$style_work/tool.manifest" "${style_tools[@]}" || return 125
    cat "$style_work/tool.manifest" >> "$destination" || return 125
    digest=$("$style_openssl" dgst -sha256 -r "$style_work/environment") || return 125
    printf 'ENV %s\n' "${digest:0:64}" >> "$destination" || return 125
}

# Produce a bounded native directory inventory; its helper is validated before cached execution.
style_search_inventory() {
    local roots=$1 output=$2
    style_small_file "$roots" 131072 || return 125
    "$style_metadata" tree < "$roots" > "$output" || return 125
    "$style_sort" -o "$output" "$output" || return 125
    style_small_file "$output" 33554432
}

# Reject renameable cache ancestors; root-owned sticky temporary roots are the explicit exception.
style_cache_ancestors() {
    local path=$1 owner mode size kind row depth=0
    path=$(cd -P -- "$path" && pwd) || return 125
    while :; do
        depth=$((depth + 1)); ((depth <= 128)) || return 125
        row=$(style_stat "$path") || return 125
        read -r owner mode size kind <<< "$row"
        [[ $owner == 0 || $owner == "$style_uid" ]] || return 125
        [[ -n $mode && $mode != *[!0-7]* ]] || return 125
        if (( (8#$mode & 022) != 0 )); then
            [[ $owner == 0 ]] && (( (8#$mode & 01000) != 0 )) || return 125
        fi
        [[ $path != / ]] || break
        path=${path%/*}
        [[ -n $path ]] || path=/
    done
}

# Validate the nearest existing parent before creating a private cache leaf or missing parents.
style_cache_parent() {
    local path=$1 depth=0
    while [[ ! -d $path ]]; do
        depth=$((depth + 1)); ((depth <= 128)) || return 125
        path=${path%/*}
        [[ -n $path ]] || path=/
    done
    style_cache_ancestors "$path"
}

# Record tool lookup identity as well as content; equal-byte symlink retargets can change resources.
style_tool_links() {
    local selected path target depth
    for selected in "${style_tools[@]}"; do
        path=$(style_absolute "$selected") || return 125
        for ((depth=0; depth<128; depth++)); do
            style_path "$path" || return 125
            if [[ ! -L $path ]]; then
                printf 'TOOL_FILE\0%s\0' "$path" >> "$style_work/environment" || return 125
                break
            fi
            target=$(/usr/bin/readlink "$path") || return 125
            [[ ${#target} -le 4096 && $target != *$'\n'* && $target != *$'\r'* ]] || return 125
            printf 'TOOL_LINK\0%s\0%s\0' "$path" "$target" >> "$style_work/environment" || return 125
            [[ $target == /* ]] || target=${path%/*}/$target
            path=$(style_absolute "$target") || return 125
        done
        ((depth < 128)) || return 125
    done
}
