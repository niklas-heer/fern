# Immutable cache bundles. Only owned invocation directories are ever mutable.

# Resolve a trusted executable once; recording its path and bytes detects replacement.
style_tool() {
    local selected PATH=$style_tool_path
    selected=$(command -v -- "$1") || return 125
    [[ -f $selected && -x $selected ]] || return 125
    style_absolute "$selected"
}

# Resolve the finite tool set and platform identity without running a C compiler on warm hits.
style_tools_init() {
    style_platform=$(/usr/bin/uname -s) || return 125
    [[ $style_platform == Darwin || $style_platform == Linux ]] || return 125
    style_uid=$(/usr/bin/id -u) || return 125
    style_stat_tool=$(style_tool stat) || return 125
    style_openssl=$(style_tool openssl) || return 125
    style_find=$(style_tool find) || return 125
    style_sort=$(style_tool sort) || return 125
    style_just=$(style_tool just) || return 125
    style_pkg=$(style_tool pkg-config) || return 125
    style_ar=$(style_tool ar) || return 125
    if [[ -n ${FERN_STYLE_CC-} ]]; then
        style_cc=$(style_tool "$FERN_STYLE_CC") || return 125
    else
        local configured
        configured=$("$style_just" --justfile "$style_root/Justfile" --evaluate cc) || return 125
        [[ -n $configured && ${#configured} -le 4096 && $configured != *$'\n'* && $configured != *$'\r'* ]] || return 125
        style_cc=$(style_tool "$configured") || return 125
    fi
    style_cc=$(style_absolute "$style_cc") || return 125
    style_tools=("$style_cc" "$style_just" "$style_pkg" "$style_ar" "$style_openssl"
        "$style_find" "$style_sort" "$style_stat_tool" /bin/bash /bin/ls /bin/cp /bin/mv /bin/rm /bin/cat /bin/mkdir /bin/chmod /bin/ln /usr/bin/cmp /usr/bin/mktemp /usr/bin/head /usr/bin/tail /usr/bin/grep /usr/bin/readlink /usr/bin/id /usr/bin/uname /bin/rmdir /bin/sleep)
    if [[ $style_platform == Darwin ]]; then
        style_tools+=(/usr/bin/xcrun /usr/bin/sw_vers "$(/usr/bin/xcrun --find clang)" "$(/usr/bin/xcrun --find ld)")
    else
        style_ldd=$(style_tool ldd) || return 125
        style_tools+=("$(style_tool ld)" "$style_ldd")
    fi
}

# Validate/create a private cache leaf; never repurpose an unmarked nonempty directory.
style_cache_root() {
    local cache=$1 marker
    style_path "$cache" || return 125
    while [[ $cache == */ && $cache != / ]]; do cache=${cache%/}; done
    [[ ! -L $cache ]] || return 125
    style_cache_parent "$cache" || return 125
    if [[ ! -e $cache ]]; then mkdir -p -m 700 -- "$cache" || return 125; fi
    style_private "$cache" || return 125
    cache=$(cd -P -- "$cache" && pwd) || return 125
    style_cache_ancestors "$cache" || return 125
    [[ $cache != "$style_root" && $cache != "$style_root"/* ]] || return 125
    style_cache_marker "$cache" || return 125
    local digest
    digest=$(printf '%s\0' "$style_root" | "$style_openssl" dgst -sha256 -r) || return 125
    digest=${digest:0:64}
    [[ ${#digest} == 64 && $digest != *[!0-9a-f]* ]] || return 125
    style_cache=$cache/$digest
    if [[ ! -e $style_cache ]]; then mkdir -m 700 -- "$style_cache" 2>/dev/null || [[ -d $style_cache ]] || return 125; fi
    style_private "$style_cache"
}

# Make an invocation-owned directory; no shared build directory or PID lock is involved.
style_work_create() {
    style_work=$(mktemp -d "$style_cache/work.XXXXXXXX") || return 125
    style_work_identity=$(style_identity "$style_work") || return 125
    printf 'FERN_STYLE_WORK_V1\n' > "$style_work/owner" || return 125
}

# Read stable filesystem identity for deletion/rename checks; content keys never use these values.
style_identity() {
    if [[ $style_platform == Darwin ]]; then
        "$style_stat_tool" -f '%d:%i' "$1"
    else
        "$style_stat_tool" -c '%d:%i' "$1"
    fi
}

# Remove only this invocation's original directory after verifying marker and identity.
style_work_remove() {
    [[ -n ${style_work-} && -d $style_work && ! -L $style_work ]] || return 0
    [[ $(style_identity "$style_work") == "$style_work_identity" ]] || return 125
    [[ $(cat "$style_work/owner") == FERN_STYLE_WORK_V1 ]] || return 125
    /bin/rm -rf -- "$style_work"
}

# Parse stored digest rows as data, bounding both paths and aggregate work.
style_manifest_paths() {
    local file=$1 line digest path count=0
    style_small_file "$file" 16777216 || return 125
    style_paths=()
    while IFS= read -r line; do
        digest=${line:0:64}; path=${line:65}
        [[ ${#digest} == 64 && $digest != *[!0-9a-f]* && ${line:64:1} == ' ' ]] || return 125
        style_path "$path" || return 125
        count=$((count + 1)); ((count <= 16384)) || return 125
        style_paths+=("$path")
    done < "$file"
}

# Validate all published artifact bytes before invoking any cached native helper.
style_artifacts_valid() {
    local ready=$1 file digest line actual
    style_small_file "$ready/artifacts" 1024 || return 125
    [[ $(/bin/cat "$ready/artifacts") == FERN_STYLE_ARTIFACTS_V1 ]] || return 125
    for file in program supervisor metadata; do
        style_small_file "$ready/$file.sha256" 65 || return 125
        style_small_file "$ready/$file" 33554432 && [[ -x $ready/$file ]] || return 125
        line=$("$style_openssl" dgst -sha256 -r "$ready/$file") || return 125
        digest=${line:0:64}
        IFS= read -r actual < "$ready/$file.sha256" || return 125
        [[ $actual == "$digest" && ${#digest} == 64 ]] || return 125
    done
}

# Validate a candidate without mutating it; absent/pruned entries simply cannot be retained.
style_candidate() {
    local ready=$1
    style_private "$ready" || return 125
    style_small_file "$ready/owner" 64 || return 125
    [[ $(cat "$ready/owner") == FERN_STYLE_ENTRY_V1 ]] || return 125
    style_small_file "$ready/base" 16777216 || return 125
    cmp -s "$style_work/base" "$ready/base" || return 1
    style_bundle_valid "$ready" || return 125
    style_artifacts_valid "$ready" || return 125
    style_metadata=$ready/metadata
    style_manifest_paths "$ready/dependencies" || return 125
    style_hash_files "$style_work/current.dependencies" "${style_paths[@]}" || return 1
    cmp -s "$style_work/current.dependencies" "$ready/dependencies" || return 1
    style_search_inventory "$ready/search-roots" "$style_work/current.search" || return 1
    cmp -s "$style_work/current.search" "$ready/search" || return 1
}

# Retain both executable inodes before pruning can remove their cache paths.
style_retain() {
    local ready=$1
    style_current_entry=${ready%/ready}
    style_run=$(mktemp -d "$style_cache/run.XXXXXXXX") || return 125
    style_run_identity=$(style_identity "$style_run") || return 125
    printf 'FERN_STYLE_RUN_V1\n' > "$style_run/owner" || return 125
    if ! /bin/ln -- "$ready/program" "$style_run/program" ||
       ! /bin/ln -- "$ready/supervisor" "$style_run/supervisor"; then
        style_run_remove || return 125
        return 1
    fi
    style_private "$style_run"
}

# Scan a bounded number of complete bundles; partial work never qualifies for reuse.
style_lookup() {
    local entry code count=0
    shopt -s nullglob
    for entry in "$style_cache"/entry.*; do
        count=$((count + 1)); ((count <= 64)) || return 125
        [[ -d $entry/ready ]] || continue
        if style_candidate "$entry/ready"; then
            if style_retain "$entry/ready"; then return 0; else
                code=$?
                [[ $code == 1 ]] || return 125
            fi
        else
            code=$?
            [[ $code == 1 || ! -d $entry/ready ]] || return 125
        fi
    done
    return 1
}

# Validate the finite bundle shape before retirement; unexpected files are never recursively removed.
style_entry_shape() {
    local ready=$1 file count=0
    style_private "$ready" || return 125
    style_small_file "$ready/owner" 64 || return 125
    [[ $(/bin/cat "$ready/owner") == FERN_STYLE_ENTRY_V1 ]] || return 125
    shopt -s nullglob
    for file in "$ready"/* "$ready"/.[!.]* "$ready"/..?*; do
        case ${file##*/} in
            bundle|artifacts|base|dependencies|search-roots|search|owner|program|program.sha256|supervisor|supervisor.sha256|metadata|metadata.sha256) ;;
            *) return 125 ;;
        esac
        count=$((count + 1))
        style_small_file "$file" 33554432 || return 125
    done
    [[ $count == 13 ]]
}

# Atomically retire a validated bundle before fixed-file deletion, preserving concurrent lookups.
style_remove_entry() {
    local entry=$1 identity file
    style_private "$entry" || return 125
    style_entry_shape "$entry/ready" || return 125
    identity=$(style_identity "$entry/ready") || return 125
    [[ ! -e $entry/retired && ! -L $entry/retired ]] || return 125
    /bin/mv -- "$entry/ready" "$entry/retired" || {
        [[ ! -e $entry/ready ]] && return 0
        return 125
    }
    [[ $(style_identity "$entry/retired") == "$identity" ]] || return 125
    for file in bundle artifacts base dependencies search-roots search owner program program.sha256 \
        supervisor supervisor.sha256 metadata metadata.sha256; do
        /bin/rm -- "$entry/retired/$file" || return 125
    done
    rmdir "$entry/retired" "$entry" || return 125
}

# Retain up to eight complete entries; never steal another invocation's incomplete work or locks.
style_prune() {
    local entry count=0
    local complete=()
    shopt -s nullglob
    for entry in "$style_cache"/entry.*; do
        count=$((count + 1)); ((count <= 64)) || return 125
        [[ ! -L $entry && ! -L $entry/ready ]] || return 125
        [[ -d $entry/ready ]] || continue
        complete+=("$entry")
    done
    count=${#complete[@]}
    for entry in "${complete[@]}"; do
        ((count > 8)) || break
        [[ $entry != "${style_current_entry-}" ]] || continue
        if [[ -d $entry/ready ]]; then style_remove_entry "$entry" || return 125; fi
        count=$((count - 1))
    done
}

# Seal fixed metadata contents against accidental omission/corruption, independent of artifact bytes.
style_bundle_digest() {
    local ready=$1 output=$2 file line digest index=0
    local names=(artifacts base dependencies search-roots search owner program.sha256 supervisor.sha256 metadata.sha256)
    local files=()
    for file in "${names[@]}"; do
        style_small_file "$ready/$file" 33554432 || return 125
        files+=("$ready/$file")
    done
    "$style_openssl" dgst -sha256 -r "${files[@]}" > "$style_work/bundle.hashes" || return 125
    : > "$output" || return 125
    while IFS= read -r line; do
        ((index < ${#names[@]})) || return 125
        digest=${line:0:64}
        [[ ${#digest} == 64 && $digest != *[!0-9a-f]* && ${line:64:2} == ' *' && ${line:66} == "${files[$index]}" ]] || return 125
        printf '%s %s\n' "$digest" "${names[$index]}" >> "$output" || return 125
        index=$((index + 1))
    done < "$style_work/bundle.hashes"
    [[ $index == ${#names[@]} ]]
}

# Verify the complete fixed metadata before trusting any dependency list or cached helper.
style_bundle_valid() {
    local ready=$1
    style_small_file "$ready/bundle" 1024 || return 125
    style_bundle_digest "$ready" "$style_work/current.bundle" || return 125
    /usr/bin/cmp -s "$ready/bundle" "$style_work/current.bundle" || return 125
}

# Publish a complete cache marker by hard link; an existing incomplete writer is only waited on.
style_cache_marker() {
    local cache=$1 marker=$1/owner file pending=0 temporary attempts
    if [[ ! -e $marker ]]; then
        shopt -s nullglob
        for file in "$cache"/* "$cache"/.[!.]* "$cache"/..?*; do
            [[ ! -e $marker ]] || break
            [[ ${file##*/} == owner.* && -f $file && ! -L $file ]] || return 125
            pending=$((pending + 1)); ((pending <= 64)) || return 125
        done
        if ((pending == 0)); then
            temporary=$(mktemp "$cache/owner.XXXXXXXX") || return 125
            printf 'FERN_STYLE_CACHE_V1\n' > "$temporary" || return 125
            chmod 400 "$temporary" || return 125
            /bin/ln -- "$temporary" "$marker" 2>/dev/null || [[ -e $marker ]] || return 125
            /bin/rm -- "$temporary" || return 125
        else
            for ((attempts=0; attempts<50; attempts++)); do
                [[ ! -e $marker ]] || break
                /bin/sleep 0.01 || return 125
            done
        fi
    fi
    style_small_file "$marker" 64 || return 125
    [[ $(/bin/cat "$marker") == FERN_STYLE_CACHE_V1 ]]
}

# Explicit cleaning retires complete entries only; invocation-owned runs and unfinished work stay intact.
style_clear() {
    local entry count=0
    shopt -s nullglob
    for entry in "$style_cache"/entry.*; do
        count=$((count + 1)); ((count <= 64)) || return 125
        [[ ! -L $entry && ! -L $entry/ready ]] || return 125
        [[ -d $entry/ready ]] || continue
        style_remove_entry "$entry" || return 125
    done
}
